/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "MaterialGenerator.h"

#include <FileSystem/VirtualFileSystem.h>
#include <Io/TextStreamHelpers.h>

#include <Core/Parser.h>
#include <Core/RenderPassGenerator.h>

#include <Graphics/RuntimeShaderCompiler.h>

// Describe the binding for a Rendering scenario.
struct ScenarioBinding {
    // Hashcode of the vertex shader name.
    std::string VertexShaderName;

    // Hashcode of the pixel shader name.
	std::string PixelShaderName;
};

// Return the first shader hashcode with matching name. Return an empty string if the given shader name does not exist.
std::string FindShaderPermutationHashcode( const std::vector<RenderLibraryGenerator::GeneratedShader>& shaderList, const std::string& shaderName, const std::string& passName )
{
    // We need to do a crappy linear search to find the matching shader stage hashcode (not great; might need some optimization in a near future...).
    for ( const RenderLibraryGenerator::GeneratedShader& shader : shaderList ) {
        if ( shader.OriginalName == shaderName && shader.RenderPassOwner == passName ) {
            return shader.Hashcode;
        }
    }

    return "";
}

DUSK_INLINE void SerializeFlag( FileSystemObject* stream, const char* flagName, const bool value )
{
    std::string serializedFlag;
    serializedFlag.append( flagName );
    serializedFlag.append( " = " );
    serializedFlag.append( value ? "true" : "false" );
    serializedFlag.append( ";\n" );

    stream->writeString( serializedFlag );
}

void SerializeScenario( FileSystemObject* stream, const char* scenarioName, const ScenarioBinding& scenarioBinding )
{
    stream->writeString( "\tscenario \"" );
    stream->writeString( scenarioName );
    stream->writeString( "\" {\n" );

    stream->writeString( "\t\tvertex = \"" );
    stream->writeString( scenarioBinding.VertexShaderName );
    stream->writeString( "\";\n" );

    stream->writeString( "\t\tpixel = \"" );
    stream->writeString( scenarioBinding.PixelShaderName );
    stream->writeString( "\";\n" );

    stream->writeString( "\t}\n" );
}

// Material Layer HLSL name.
// Root layer has the same name as the blended result since we use it as a base.
constexpr const char* AttributesNames[7] = {
    "BaseColor",
    "Reflectance",
    "Roughness",
    "Metalness",
    "AmbientOcclusion",
    "Emissivity",
    "BlendMask"
};

MaterialGenerator::MaterialGenerator( BaseAllocator* allocator, VirtualFileSystem* virtualFileSystem )
    : attributeGetterCount( 0 )
    , texResourceCount( 0 )
    , virtualFs( virtualFileSystem )
    , memoryAllocator( allocator )
    , shaderCompiler( dk::core::allocate<RuntimeShaderCompiler>( allocator, allocator, virtualFileSystem ) )
{

}

MaterialGenerator::~MaterialGenerator()
{

}

std::string MaterialGenerator::buildTextureLayerName( const char* attributeName, const char* layerName )
{
    // Format each attribute texture name (since each layer can have its own attribute texture).
    std::string resourceName = "Texture_";
    resourceName.append( layerName );
    resourceName.append( "_" );
    resourceName.append( attributeName );

    return resourceName;
}

EditableMaterial MaterialGenerator::createEditableMaterial( const Material* material )
{
    EditableMaterial editableMat;

    return editableMat;
}

Material* MaterialGenerator::createMaterial( const EditableMaterial& editableMaterial )
{
    resetMaterialTemporaryOutput();

    // We read each material layer first.
    for ( i32 layerIdx = 0; layerIdx < editableMaterial.LayerCount; layerIdx++ ) {
        const EditableMaterialLayer& layer = editableMaterial.Layers[layerIdx];
        const char* layerName = MaterialLayerNames[layerIdx];

        appendLayerRead( layerIdx, layer );
        buildMaterialParametersMap( layerName, layer );
    }

    // Then we perform blending between each layer (starting from the root).
    // This step is skipped for single layered materials.
    for ( i32 layerIdx = 1; layerIdx < editableMaterial.LayerCount; layerIdx++ ) {
        const EditableMaterialLayer& rootLayer = editableMaterial.Layers[0];
        const EditableMaterialLayer& aboveLayer = editableMaterial.Layers[layerIdx];

        appendLayerBlend( rootLayer, aboveLayer, MaterialLayerNames[0], MaterialLayerNames[layerIdx] );
    }

    // "Inject" this material informations in the render pass template.
    FileSystemObject* matTemplate = virtualFs->openFile( DUSK_STRING( "EditorAssets/RenderPasses/PrimitiveLighting.tdrpl" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    if ( matTemplate == nullptr ) {
        DUSK_LOG_ERROR( "Material renderpass template does not exist!\n" );
        return nullptr;
    }

    std::string assetStr;
    dk::io::LoadTextFile( matTemplate, assetStr );

    matTemplate->close();
    
    dk::core::ReplaceWord( assetStr, "DUSK_LAYERS_RESOURCES;", materialResourcesCode );
    dk::core::ReplaceWord( assetStr, "DUSK_LAYERS_FUNCTIONS;", materialSharedCode );
    dk::core::ReplaceWord( assetStr, "DUSK_LAYERS_GET;", materialLayersGetter );
    dk::core::ReplaceWord( assetStr, "DUSK_BAKED_TEXTURE_FETCH;", bakedTextureCode );
   
    dk::core::ReplaceWord( assetStr, "pass ", std::string( "pass " ) + editableMaterial.Name );

    // Generate permutations for this material.
    RenderLibraryGenerator renderLibGenerator;

    Parser parser( assetStr.c_str() );
    parser.generateAST();

    const u32 typeCount = parser.getTypeCount();
    for ( u32 t = 0; t < typeCount; t++ ) {
        const TypeAST& typeAST = parser.getType( t );
        switch ( typeAST.Type ) {
        case TypeAST::LIBRARY:
        {
            renderLibGenerator.generateFromAST( typeAST, false, false );

            const std::vector<RenderLibraryGenerator::GeneratedShader>& libraryShaders = renderLibGenerator.getGeneratedShaders();
            for ( const RenderLibraryGenerator::GeneratedShader& shader : libraryShaders ) {
                RuntimeShaderCompiler::GeneratedBytecode compiledShader = shaderCompiler->compileShaderModel5( shader.ShaderStage, shader.GeneratedSource.c_str(), shader.GeneratedSource.size() );

                if ( compiledShader.Length == 0ull || compiledShader.Bytecode == nullptr ) {
                    DUSK_LOG_ERROR( "'%hs' : compilation failed!\n", shader.Hashcode.c_str() );
                    continue;
                }

                dkString_t shaderBinPath = dkString_t( DUSK_STRING( "EditorAssets/shaders/sm5/" ) ) + StringToWideString( shader.Hashcode );
                FileSystemObject* compiledShaderBin = virtualFs->openFile( shaderBinPath, eFileOpenMode::FILE_OPEN_MODE_WRITE | eFileOpenMode::FILE_OPEN_MODE_BINARY );
                if ( compiledShaderBin->isGood() ) {
                    compiledShaderBin->write( compiledShader.Bytecode, compiledShader.Length );
                
                    compiledShaderBin->close();

                    DUSK_LOG_INFO( "'%hs' : shader has been saved successfully!\n", shader.Hashcode.c_str() );
                }
            }
        } break;
        }
    }

    // Generate material descriptor (pipeline flags, external assets required, etc.).
    const std::string namedGenericPass = std::string( editableMaterial.Name ) + "LightPass";
    const std::string namedGenericPassEd = std::string( editableMaterial.Name ) + "LightPassEd";
    const std::string namedGenericPickingPass = std::string( editableMaterial.Name ) + "LightPickingPass";
    const std::string namedGenericPickingPassEd = std::string( editableMaterial.Name ) + "LightPickingPassEd";

    ScenarioBinding DefaultBinding;
    ScenarioBinding DefaultEdBinding;
    ScenarioBinding DefaultPickingBinding;
    ScenarioBinding DefaultPickingEdBinding;

    const std::vector<RenderLibraryGenerator::RenderPassInfos>& renderPasses = renderLibGenerator.getGeneratedRenderPasses();
    for ( const RenderLibraryGenerator::RenderPassInfos& renderPass : renderPasses ) {
        ScenarioBinding* matchingBinding = nullptr;

        // TODO Remove this crappy string to string comparison and make something better!
        if ( renderPass.RenderPassName == namedGenericPass ) {
            matchingBinding = &DefaultBinding;
		} else if ( renderPass.RenderPassName == namedGenericPassEd ) {
            matchingBinding = &DefaultEdBinding;
        } else if ( renderPass.RenderPassName == namedGenericPickingPass ) {
            matchingBinding = &DefaultPickingBinding;
        } else if ( renderPass.RenderPassName == namedGenericPickingPassEd ) {
            matchingBinding = &DefaultPickingEdBinding;
        } else {
            DUSK_LOG_WARN( "Unknown RenderPass '%hs' found!\n", renderPass.RenderPassName.c_str() );
            continue;
        }

        matchingBinding->VertexShaderName = FindShaderPermutationHashcode( renderLibGenerator.getGeneratedShaders(), renderPass.StageShaderNames[0], renderPass.RenderPassName );
        matchingBinding->PixelShaderName = FindShaderPermutationHashcode( renderLibGenerator.getGeneratedShaders(), renderPass.StageShaderNames[3], renderPass.RenderPassName );
    }

    FileSystemObject* materialDescriptor = virtualFs->openFile( dkString_t( DUSK_STRING( "EditorAssets/materials/" ) ) + StringToWideString( editableMaterial.Name ) + DUSK_STRING( ".mat" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
    if ( materialDescriptor->isGood() ) {
        materialDescriptor->writeString( "material \"" );
        materialDescriptor->writeString( editableMaterial.Name );
        materialDescriptor->writeString( "\" {\n" );

        // Misc. infos.
        materialDescriptor->writeString( "\tversion = " );
        materialDescriptor->writeString( std::to_string( MaterialGenerator::Version ) );
        materialDescriptor->writeString( ";\n" );

        // Write Material flags (we only write flags affecting the pipeline state;
        // flags are directly baked in the shader binary otherwise).
        SerializeFlag( materialDescriptor, "\tisAlphaBlended", editableMaterial.IsAlphaBlended );
        SerializeFlag( materialDescriptor, "\tisDoubleFace", editableMaterial.IsDoubleFace );
        SerializeFlag( materialDescriptor, "\tenableAlphaToCoverage", editableMaterial.UseAlphaToCoverage );
		SerializeFlag( materialDescriptor, "\tisAlphaTested", editableMaterial.IsAlphaTested );
		SerializeFlag( materialDescriptor, "\tisWireframe", editableMaterial.IsWireframe );

        // Write Parameters.
        if ( !mutableParameters.empty() ) {
            materialDescriptor->writeString( "\tparameters {\n" );

            for ( const MutableParameter& param : mutableParameters ) {
                materialDescriptor->writeString( "\t\t \"" );
                materialDescriptor->writeString( param.ParameterName );
                materialDescriptor->writeString( "\" = \"" );
                materialDescriptor->writeString( param.ParameterValue );
                materialDescriptor->writeString( "\";\n" );
            }
            materialDescriptor->writeString( "\t}\n" );
        }

        // Write each Rendering scenario.
		SerializeScenario( materialDescriptor, "Default", DefaultBinding );
        SerializeScenario( materialDescriptor, "Editor_Default", DefaultEdBinding );
        SerializeScenario( materialDescriptor, "Default_Picking", DefaultPickingBinding );
        SerializeScenario( materialDescriptor, "Editor_Default_Picking", DefaultPickingEdBinding );

        materialDescriptor->writeString( "}\n" );

        materialDescriptor->close();
    }

    // TEST crap test
    FileSystemObject* TestmaterialDescriptor = virtualFs->openFile( dkString_t( DUSK_STRING( "EditorAssets/materials/" ) ) + StringToWideString( editableMaterial.Name ) + DUSK_STRING( ".mat" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    Material* material = new Material( memoryAllocator );
    material->deserialize( TestmaterialDescriptor );
    materialDescriptor->close();

    return material;
}

void MaterialGenerator::resetMaterialTemporaryOutput()
{
    materialLayersGetter.clear();
    materialSharedCode.clear();
    materialResourcesCode.clear();
    bakedTextureCode.clear();

    mutableParameters.clear();

    attributeGetterCount = 0;
    texResourceCount = 0;
}

void MaterialGenerator::appendLayerBlend( const EditableMaterialLayer& bottomLayer, const EditableMaterialLayer& topLayer, const char* bottomLayerName, const char* topLayerName )
{
    switch ( topLayer.BlendMode ) {
    case Additive:
    {
        appendAttributeBlendAdditive( "BaseColor", bottomLayerName, topLayerName, topLayer.DiffuseContribution );
        appendAttributeBlendAdditive( "Reflectance", bottomLayerName, topLayerName, topLayer.SpecularContribution );
        appendAttributeBlendAdditive( "Roughness", bottomLayerName, topLayerName, topLayer.SpecularContribution );
        appendAttributeBlendAdditive( "Metalness", bottomLayerName, topLayerName, topLayer.SpecularContribution );
        break;
    } break;
    case Multiplicative:
    {
        appendAttributeBlendMultiplicative( "BaseColor", bottomLayerName, topLayerName, topLayer.DiffuseContribution );
        appendAttributeBlendMultiplicative( "Reflectance", bottomLayerName, topLayerName, topLayer.SpecularContribution );
        appendAttributeBlendMultiplicative( "Roughness", bottomLayerName, topLayerName, topLayer.SpecularContribution );
        appendAttributeBlendMultiplicative( "Metalness", bottomLayerName, topLayerName, topLayer.SpecularContribution );
    } break;
    }
}

void MaterialGenerator::processAttributeParameter( const char* layerName, const char* attributeName, const MaterialAttribute& attribute )
{
    switch ( attribute.Type ) {
    case MaterialAttribute::Texture_2D:
    {
        const std::string resourceName = buildTextureLayerName( attributeName, layerName );
        const std::string resourceValue = WideStringToString( attribute.AsTexture.PathToTextureAsset );

        mutableParameters.push_back( MutableParameter( resourceName.c_str(), resourceValue.c_str() ) );
    } break;
    case MaterialAttribute::Mutable_3D:
    {
        mutableParameters.push_back( MutableParameter( attributeName, dk::io::Vector3DToString( attribute.AsFloat3 ).c_str() ) );
    } break;
    default:
        break;
    }
}

void MaterialGenerator::buildMaterialParametersMap( const char* layerName, const EditableMaterialLayer& layer )
{
    processAttributeParameter( layerName, "BaseColor", layer.BaseColor );
    processAttributeParameter( layerName, "Reflectance", layer.Reflectance );
    processAttributeParameter( layerName, "Roughness", layer.Roughness );
    processAttributeParameter( layerName, "Metalness", layer.Metalness );
    processAttributeParameter( layerName, "AmbientOcclusion", layer.AmbientOcclusion );
    processAttributeParameter( layerName, "Emissivity", layer.Emissivity );
    processAttributeParameter( layerName, "BlendMask", layer.BlendMask );
}

void MaterialGenerator::appendAttributeBlendAdditive( const char* attributeName, const char* bottomLayerName, const char* topLayerName, const f32 contributionFactor )
{
    std::string bottomAttribute = std::string( bottomLayerName ) + "." + attributeName;
    std::string topAttribute = std::string( topLayerName ) + "." + attributeName;

    // Generates: lerp( bottom, top, top.blendMask * contribution )
    materialLayersGetter.append( bottomAttribute );
    materialLayersGetter.append( "=lerp(" );

    // "bottom,"
    materialLayersGetter.append( bottomAttribute );
    materialLayersGetter.append( "," );

    // "top,"
    materialLayersGetter.append( topAttribute );
    materialLayersGetter.append( "," );

    // "top.blendMask * contribution"
    materialLayersGetter.append( topLayerName );
    materialLayersGetter.append( ".BlendMask*" );
    materialLayersGetter.append( std::to_string( contributionFactor ) );
    materialLayersGetter.append( ");\n" );
}

void MaterialGenerator::appendAttributeBlendMultiplicative( const char* attributeName, const char* bottomLayerName, const char* topLayerName, const f32 contributionFactor /*= 1.0f */ )
{
    std::string bottomAttribute = std::string( bottomLayerName ) + "." + attributeName;
    std::string topAttribute = std::string( topLayerName ) + "." + attributeName;

    // Generates: lerp( bottom, bottom * top, top.blendMask * contribution )
    materialLayersGetter.append( bottomAttribute );
    materialLayersGetter.append( "=lerp(" );

    // "bottom,"
    materialLayersGetter.append( bottomAttribute );
    materialLayersGetter.append( "," );

    // "bottom * top,"
    materialLayersGetter.append( bottomAttribute );
    materialLayersGetter.append( "*" );
    materialLayersGetter.append( topAttribute );
    materialLayersGetter.append( "," );

    // "top.blendMask * contribution"
    materialLayersGetter.append( topLayerName );
    materialLayersGetter.append( ".BlendMask*" );
    materialLayersGetter.append( std::to_string( contributionFactor ) );
    materialLayersGetter.append( ");\n" );
}

void MaterialGenerator::appendLayerRead( const i32 layerIndex, const EditableMaterialLayer& layer )
{
    const char* layerName = MaterialLayerNames[layerIndex];

    materialLayersGetter.append( "Material " );
    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".BaseColor=" );
    appendAttributeFetch3D( 0, layerIndex, layer.BaseColor, layer.Scale, layer.Offset );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".Reflectance=" );
    appendAttributeFetch1D( 1, layerIndex, layer.Reflectance, layer.Scale, layer.Offset );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".Roughness=" );
    appendAttributeFetch1D( 2, layerIndex, layer.Roughness, layer.Scale, layer.Offset );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".Metalness=" );
    appendAttributeFetch1D( 3, layerIndex, layer.Metalness, layer.Scale, layer.Offset );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".AmbientOcclusion=" );
    appendAttributeFetch1D( 4, layerIndex, layer.AmbientOcclusion, layer.Scale, layer.Offset );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".Emissivity=" );
    appendAttributeFetch1D( 5, layerIndex, layer.Emissivity, layer.Scale, layer.Offset );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".BlendMask=" );
    appendAttributeFetch1D( 6, layerIndex, layer.BlendMask, layer.Scale, layer.Offset );
    materialLayersGetter.append( ";\n" );
}

void MaterialGenerator::appendTextureSampling( std::string& output, const std::string& resourceName, const std::string& samplingOffset, const std::string& samplingScale, const char* uvMapName )
{
    output.append( resourceName );
    output.append( ".Sample(TextureSampler, ( " );
    output.append( uvMapName );
    output.append( " + " );
    output.append( samplingOffset );
    output.append( " ) * " );
    output.append( samplingScale );
    output.append( " )" );
}

DUSK_INLINE std::string DuskVector2ToHLSLFloat2( const dkVec2f& inVec )
{
    std::string str;
    str.append( "float2( " );
    str.append( std::to_string( inVec.x ) );
    str.append( ", " );
    str.append( std::to_string( inVec.y ) );
    str.append( ")" );

    return str;
}

void MaterialGenerator::appendAttributeFetch1D( const i32 attributeIndex, const i32 layerIndex, const MaterialAttribute& attribute, const dkVec2f& samplingScale, const dkVec2f& samplingOffset )
{
    switch ( attribute.Type ) {
    case MaterialAttribute::Constant_1D:
    {
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
    } break;
    case MaterialAttribute::Constant_3D:
    {
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[0] ) );
    } break;
    case MaterialAttribute::Mutable_3D:
    {
    } break;
    case MaterialAttribute::Texture_2D:
    {
        const char* layerName = MaterialLayerNames[layerIndex];
        const char* attributeName = AttributesNames[attributeIndex];

        // Format each attribute texture name (since each layer can have its own attribute texture).
        const std::string resourceName = buildTextureLayerName( attributeName, layerName );
        std::string samplingOffsetString = DuskVector2ToHLSLFloat2( samplingOffset );
        std::string samplingScaleString = DuskVector2ToHLSLFloat2( samplingScale );
        appendTextureSampling( materialLayersGetter, resourceName, samplingOffsetString, samplingScaleString );
        materialLayersGetter.append( ".r" );

        // Declare texture in the resource list.
        // TODO Might need refactoring to implement atlas/shared texture correctly.
        materialResourcesCode.append( "Texture2D " + resourceName + "{\nswizzle = float;\n}\n" );

        // Create interactive material texture fetch call.
        bakedTextureCode.append( "if ( layerIndex == " );
        bakedTextureCode.append( std::to_string( layerIndex ) );
        bakedTextureCode.append( " && attributeIndex == " );
        bakedTextureCode.append( std::to_string( attributeIndex ) );
        bakedTextureCode.append( ") return " );
        appendTextureSampling( bakedTextureCode, resourceName, "offset", "scale", "uvMapTexCoords" );
        bakedTextureCode.append( ".rrrr;" );
    } break;

    case MaterialAttribute::Code_Piece:
    {
        std::string attributeGetterFuncName = "GetAttribute_" + std::to_string( attributeGetterCount++ ) + "()";

        materialSharedCode.append( "float " );
        materialSharedCode.append( attributeGetterFuncName );
        materialSharedCode.append( "\n{\n" );
        materialSharedCode.append( attribute.AsCodePiece.Content );
        materialSharedCode.append( "\n}\n" );

        materialLayersGetter.append( attributeGetterFuncName );
    } break;
    }
}
    
void MaterialGenerator::appendAttributeFetch2D( const i32 attributeIndex, const i32 layerIndex, const MaterialAttribute& attribute, const dkVec2f& samplingScale, const dkVec2f& samplingOffset )
{
    switch ( attribute.Type ) {
    case MaterialAttribute::Constant_1D:
    {
        materialLayersGetter.append( "float2(" );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( ")" );
    } break;
    case MaterialAttribute::Constant_3D:
    {
        materialLayersGetter.append( "float2(" );
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[0] ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[1] ) );
        materialLayersGetter.append( ")" );
    } break;
    case MaterialAttribute::Mutable_3D:
    {
    } break;
    case MaterialAttribute::Texture_2D:
    {
        const char* layerName = MaterialLayerNames[layerIndex];
        const char* attributeName = AttributesNames[attributeIndex];

        // Format each attribute texture name (since each layer can have its own attribute texture).
        const std::string resourceName = buildTextureLayerName( attributeName, layerName );
        std::string samplingOffsetString = DuskVector2ToHLSLFloat2( samplingOffset );
        std::string samplingScaleString = DuskVector2ToHLSLFloat2( samplingScale );
        appendTextureSampling( materialLayersGetter, resourceName, samplingOffsetString, samplingScaleString );
        materialLayersGetter.append( ".rg" );

        // Declare texture in the resource list.
        // TODO Might need refactoring to implement atlas/shared texture correctly.
        materialResourcesCode.append( "Texture2D " + resourceName + "{\nswizzle = float2;\n}\n" );

        // Create interactive material texture fetch call.
        bakedTextureCode.append( "if ( layerIndex == " );
        bakedTextureCode.append( std::to_string( layerIndex ) );
        bakedTextureCode.append( " && attributeIndex == " );
        bakedTextureCode.append( std::to_string( attributeIndex ) );
        bakedTextureCode.append( ") return " );
        appendTextureSampling( bakedTextureCode, resourceName, "offset", "scale", "uvMapTexCoords" );
        bakedTextureCode.append( ".rgrg;\n" );
    } break;

    case MaterialAttribute::Code_Piece:
    {
        std::string attributeGetterFuncName = "GetAttribute_" + std::to_string( attributeGetterCount++ ) + "()";

        materialSharedCode.append( "float2 " );
        materialSharedCode.append( attributeGetterFuncName );
        materialSharedCode.append( "\n{\n" );
        materialSharedCode.append( attribute.AsCodePiece.Content );
        materialSharedCode.append( "\n}\n" );

        materialLayersGetter.append( attributeGetterFuncName );
    } break;
    }
}
    
void MaterialGenerator::appendAttributeFetch3D( const i32 attributeIndex, const i32 layerIndex, const MaterialAttribute& attribute, const dkVec2f& samplingScale, const dkVec2f& samplingOffset )
{
    switch ( attribute.Type ) {
    case MaterialAttribute::Constant_1D:
    {
        materialLayersGetter.append( "float3(" );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( ")" );
    } break;
    case MaterialAttribute::Constant_3D:
    {
        materialLayersGetter.append( "float3(" );
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[0] ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[1] ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[2] ) );
        materialLayersGetter.append( ")" );
    } break;
    case MaterialAttribute::Mutable_3D:
    {
    } break;
    case MaterialAttribute::Texture_2D:
    {
        const char* layerName = MaterialLayerNames[layerIndex];
        const char* attributeName = AttributesNames[attributeIndex];

        // Format each attribute texture name (since each layer can have its own attribute texture).
        const std::string resourceName = buildTextureLayerName( attributeName, layerName );
        std::string samplingOffsetString = DuskVector2ToHLSLFloat2( samplingOffset );
        std::string samplingScaleString = DuskVector2ToHLSLFloat2( samplingScale );
        appendTextureSampling( materialLayersGetter, resourceName, samplingOffsetString, samplingScaleString );
        materialLayersGetter.append( ".rgb" );

        // Declare texture in the resource list.
        // TODO Might need refactoring to implement atlas/shared texture correctly.
        materialResourcesCode.append( "Texture2D " + resourceName + "{\nswizzle = float3;\n}\n" );

        // Create interactive material texture fetch call.
        bakedTextureCode.append( "if ( layerIndex == " );
        bakedTextureCode.append( std::to_string( layerIndex ) );
        bakedTextureCode.append( " && attributeIndex == " );
        bakedTextureCode.append( std::to_string( attributeIndex ) );
        bakedTextureCode.append( ") return " );
        appendTextureSampling( bakedTextureCode, resourceName, "offset", "scale", "uvMapTexCoords" );
        bakedTextureCode.append( ".rgbr;" );
    } break;

    case MaterialAttribute::Code_Piece:
    {
        std::string attributeGetterFuncName = "GetAttribute_" + std::to_string( attributeGetterCount++ ) + "()";

        materialSharedCode.append( "float3 " );
        materialSharedCode.append( attributeGetterFuncName );
        materialSharedCode.append( "\n{\n" );
        materialSharedCode.append( attribute.AsCodePiece.Content );
        materialSharedCode.append( "\n}\n" );

        materialLayersGetter.append( attributeGetterFuncName );
    } break;
    }
}

void MaterialGenerator::appendAttributeFetch4D( const i32 attributeIndex, const i32 layerIndex, const MaterialAttribute& attribute, const dkVec2f& samplingScale, const dkVec2f& samplingOffset )
{
    switch ( attribute.Type ) {
    case MaterialAttribute::Constant_1D:
    {
        materialLayersGetter.append( "float4(" );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat ) );
        materialLayersGetter.append( ")" );
    } break;
    case MaterialAttribute::Constant_3D:
    {
        materialLayersGetter.append( "float4(" );
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[0] ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[1] ) );
        materialLayersGetter.append( "," );
        materialLayersGetter.append( std::to_string( attribute.AsFloat3[2] ) );
        materialLayersGetter.append( ", 1.0f )" );
    } break;
    case MaterialAttribute::Mutable_3D:
    {
    } break;
    case MaterialAttribute::Texture_2D:
    {
        const char* layerName = MaterialLayerNames[layerIndex];
        const char* attributeName = AttributesNames[attributeIndex];

        // Format each attribute texture name (since each layer can have its own attribute texture).
        const std::string resourceName = buildTextureLayerName( attributeName, layerName );
        std::string samplingOffsetString = DuskVector2ToHLSLFloat2( samplingOffset );
        std::string samplingScaleString = DuskVector2ToHLSLFloat2( samplingScale );
        appendTextureSampling( materialLayersGetter, resourceName, samplingOffsetString, samplingScaleString );
        materialLayersGetter.append( ".rgba" );

        // Declare texture in the resource list.
        // TODO Might need refactoring to implement atlas/shared texture correctly.
        materialResourcesCode.append( "Texture2D " + resourceName + "{\nswizzle = float4;\n}\n" );

        // Create interactive material texture fetch call.
        bakedTextureCode.append( "if ( layerIndex == " );
        bakedTextureCode.append( std::to_string( layerIndex ) );
        bakedTextureCode.append( " && attributeIndex == " );
        bakedTextureCode.append( std::to_string( attributeIndex ) );
        bakedTextureCode.append( ") return " );
        appendTextureSampling( bakedTextureCode, resourceName, "offset", "scale", "uvMapTexCoords" );
        bakedTextureCode.append( ".rgba;" );
    } break;

    case MaterialAttribute::Code_Piece:
    {
        std::string attributeGetterFuncName = "GetAttribute_" + std::to_string( attributeGetterCount++ ) + "()";

        materialSharedCode.append( "float4 " );
        materialSharedCode.append( attributeGetterFuncName );
        materialSharedCode.append( "\n{\n" );
        materialSharedCode.append( attribute.AsCodePiece.Content );
        materialSharedCode.append( "\n}\n" );

        materialLayersGetter.append( attributeGetterFuncName );
    } break;
    }
}
