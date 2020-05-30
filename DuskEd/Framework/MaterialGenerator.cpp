/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "MaterialGenerator.h"

#include <FileSystem/VirtualFileSystem.h>
#include <Io/TextStreamHelpers.h>

#include <Parser.h>
#include <RenderPassGenerator.h>

#include <Graphics/RuntimeShaderCompiler.h>

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

EditableMaterial MaterialGenerator::createEditableMaterial( const Material* material )
{
    EditableMaterial editableMat;

    return editableMat;
}

Material* MaterialGenerator::createMaterial( const EditableMaterial& editableMaterial )
{
    resetMaterialTemporaryOutput();

    // Material Layer HLSL name.
    // Root layer has the same name as the blended result since we use it as a base.
    constexpr const char* MaterialLayerNames[Material::MAX_LAYER_COUNT] = {
        "BlendedMaterial",
        "MaterialLayer0",
        "MaterialLayer1",
        "MaterialLayer2"
    };

    // We read each material layer first.
    for ( i32 layerIdx = 0; layerIdx < editableMaterial.LayerCount; layerIdx++ ) {
        const EditableMaterialLayer& layer = editableMaterial.Layers[layerIdx];
        appendLayerRead( MaterialLayerNames[layerIdx], layer );
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

    dk::core::ReplaceWord( assetStr, "DUSK_LAYERS_RESOURCES", materialResourcesCode );
    dk::core::ReplaceWord( assetStr, "DUSK_LAYERS_FUNCTIONS", materialSharedCode );
    dk::core::ReplaceWord( assetStr, "DUSK_LAYERS_GET", materialLayersGetter );

    // Generate permutations for this material.
    Parser parser( assetStr.c_str() );
    parser.generateAST();

    const u32 typeCount = parser.getTypeCount();
    for ( u32 t = 0; t < typeCount; t++ ) {
        const TypeAST& typeAST = parser.getType( t );
        switch ( typeAST.Type ) {
        case TypeAST::LIBRARY:
        {
            RenderLibraryGenerator renderLibGenerator;
            renderLibGenerator.generateFromAST( typeAST, false, false );

            const std::vector<RenderLibraryGenerator::GeneratedShader>& libraryShaders = renderLibGenerator.getGeneratedShaders();
            for ( const RenderLibraryGenerator::GeneratedShader& shader : libraryShaders ) {
                RuntimeShaderCompiler::GeneratedBytecode compiledShader = shaderCompiler->compileShaderModel5( shader.ShaderStage, shader.GeneratedSource.c_str(), shader.GeneratedSource.size() );

                if ( compiledShader.Length == 0ull ) {
                    DUSK_LOG_ERROR( "'%hs' : compilation failed!\n", shader.ShaderName.c_str() );
                    continue;
                }

                dkString_t shaderBinPath = dkString_t( DUSK_STRING( "GameData/shaders/sm5/" ) ) + StringToWideString( shader.ShaderName );
                FileSystemObject* compiledShaderBin = virtualFs->openFile( shaderBinPath, eFileOpenMode::FILE_OPEN_MODE_WRITE );
                if ( compiledShaderBin->isGood() ) {
                    compiledShaderBin->write( compiledShader.Bytecode, compiledShader.Length );
                
                    DUSK_LOG_INFO( "'%hs' : shader has been saved successfully!\n", shader.ShaderName.c_str() );
                }
            }
        } break;
        }
    }

    // Bind the generated permutations as the active material.

    return nullptr;
}

void MaterialGenerator::resetMaterialTemporaryOutput()
{
    materialLayersGetter.clear();
    materialSharedCode.clear();
    materialResourcesCode.clear();

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

void MaterialGenerator::appendLayerRead( const char* layerName, const EditableMaterialLayer& layer )
{
    materialLayersGetter.append( "Material " );
    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ";/n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".BaseColor=" );
    appendAttributeFetch3D( layer.BaseColor );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".Reflectance=" );
    appendAttributeFetch1D( layer.Reflectance );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".Roughness=" );
    appendAttributeFetch1D( layer.Roughness );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".Metalness=" );
    appendAttributeFetch1D( layer.Metalness );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".AmbientOcclusion=" );
    appendAttributeFetch1D( layer.AmbientOcclusion );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".Emissivity=" );
    appendAttributeFetch1D( layer.Emissivity );
    materialLayersGetter.append( ";\n" );

    materialLayersGetter.append( layerName );
    materialLayersGetter.append( ".BlendMask=" );
    appendAttributeFetch1D( layer.BlendMask );
    materialLayersGetter.append( ";\n" );
}

void MaterialGenerator::appendAttributeFetch1D( const MaterialAttribute& attribute )
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
        std::string resourceName = "Texture_" + std::to_string( texResourceCount++ );

        materialLayersGetter.append( resourceName );
        materialLayersGetter.append( ".Sample(TextureSampler, $TEXCOORD0).r" );

        // Declare texture in the resource list.
        // TODO Might need refactoring to implement atlas/shared texture correctly.
        materialResourcesCode.append( "Texture2D " + resourceName + "{\nswizzle = float;\n}\n" );
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
    
void MaterialGenerator::appendAttributeFetch2D( const MaterialAttribute& attribute )
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
        std::string resourceName = "Texture_" + std::to_string( texResourceCount++ );

        materialLayersGetter.append( resourceName );
        materialLayersGetter.append( ".Sample(TextureSampler, $TEXCOORD0).rg" );

        // Declare texture in the resource list.
        // TODO Might need refactoring to implement atlas/shared texture correctly.
        materialResourcesCode.append( "Texture2D " + resourceName + "{\nswizzle = float2;\n}\n" );
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
    
void MaterialGenerator::appendAttributeFetch3D( const MaterialAttribute& attribute )
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
        std::string resourceName = "Texture_" + std::to_string( texResourceCount++ );

        materialLayersGetter.append( resourceName );
        materialLayersGetter.append( ".Sample(TextureSampler, $TEXCOORD0).rgb" );

        // Declare texture in the resource list.
        // TODO Might need refactoring to implement atlas/shared texture correctly.
        materialResourcesCode.append( "Texture2D " + resourceName + "{\nswizzle = float3;\n}\n" );
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

void MaterialGenerator::appendAttributeFetch4D( const MaterialAttribute& attribute )
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
        std::string resourceName = "Texture_" + std::to_string( texResourceCount++ );

        materialLayersGetter.append( resourceName );
        materialLayersGetter.append( ".Sample(TextureSampler, $TEXCOORD0).rgba" );

        // Declare texture in the resource list.
        // TODO Might need refactoring to implement atlas/shared texture correctly.
        materialResourcesCode.append( "Texture2D " + resourceName + "{\nswizzle = float4;\n}\n" );
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
