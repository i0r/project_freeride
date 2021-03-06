#include <Shared.h>

#if DUSK_USE_FBXSDK
#include "FbxParser.h"
#include <fbxsdk.h>

#include <Core/Environment.h>
#include <Core/StringHelpers.h>

// Return the index for an element array accordingly to its reference mode.
static i32 GetArrayIndexByReferenceMode( 
    const FbxLayerElement::EReferenceMode referenceMode, 
    const FbxLayerElementArrayTemplate<int>& indexArray, 
    const i32 polygonVertIdx )
{
    switch ( referenceMode ) {
    case FbxLayerElement::eDirect:
        return polygonVertIdx;
    case FbxLayerElement::eIndexToDirect:
        return indexArray.GetAt( polygonVertIdx );
    case FbxLayerElement::eIndex:
    default:
        return 0;
    }
}

static FbxNode* FindParentWithAttributesRecurse( FbxNode* node, const FbxNodeAttribute::EType attribute )
{
    if ( node == nullptr ) {
        return nullptr;
    }

    const FbxNodeAttribute* nodeAttribute = node->GetNodeAttribute();
    if ( nodeAttribute != nullptr && nodeAttribute->GetAttributeType() == attribute ) {
        return node;
    }

    return FindParentWithAttributesRecurse( node->GetParent(), attribute );
}

FbxParser::FbxParser()
    : fbxManager( nullptr )
    , fbxScene( nullptr )
    , memoryAllocator( nullptr )
    , importer( nullptr )
    , parsedModel( nullptr )
{
}

FbxParser::~FbxParser()
{
    if ( parsedModel != nullptr ) {
        dk::core::free( memoryAllocator, parsedModel );
        parsedModel = nullptr;
    }

    fbxScene = nullptr;
    importer = nullptr;

    if ( fbxManager != nullptr ) {
        fbxManager->Destroy();
        fbxManager = nullptr;
    }

    memoryAllocator = nullptr;
}

void FbxParser::create( BaseAllocator* allocator )
{
    memoryAllocator = allocator;

    //The first thing to do is to create the FBX Manager which is the object allocator for almost all the classes in the SDK
    fbxManager = FbxManager::Create();
    DUSK_RAISE_FATAL_ERROR( fbxManager, "Autodesk FBX SDK: Unable to create FBX Manager!\n" );
    DUSK_LOG_INFO( "Autodesk FBX SDK version %hs\n", fbxManager->GetVersion() );

    //Create an IOSettings object. This object holds all import/export settings.
    FbxIOSettings* ios = FbxIOSettings::Create( fbxManager, IOSROOT );
    fbxManager->SetIOSettings( ios );

    //Load plugins from the executable directory (optional)
    FbxString lPath = FbxGetApplicationDirectory();
    fbxManager->LoadPluginsDirectory( lPath.Buffer() );

    //Create an FBX scene. This object holds most objects imported/exported from/to files.
    fbxScene = FbxScene::Create( fbxManager, "My Scene" );
    DUSK_RAISE_FATAL_ERROR( fbxScene, "Autodesk FBX SDK: Unable to create FBX scene!\n" );

    importer = FbxImporter::Create( fbxManager, "" );
}

void FbxParser::load( const char* filePath )
{
    if ( parsedModel != nullptr ) {
        dk::core::free( memoryAllocator, parsedModel );
        parsedModel = nullptr;
    }

    const bool operationResult = importer->Initialize( filePath, -1, fbxManager->GetIOSettings() );
    if ( !operationResult ) {
        DUSK_LOG_ERROR( "Failed to import FBX '%hs' : %hs\n", filePath, importer->GetStatus().GetErrorString() );
        return;
    }

    // Set the import states. By default, the import states are always set to 
    // true. The code below shows how to change these states.
    FbxIOSettings& ioSettings = *fbxManager->GetIOSettings();
    ioSettings.SetBoolProp( IMP_FBX_MATERIAL, true );
    ioSettings.SetBoolProp( IMP_FBX_TEXTURE, true );
    ioSettings.SetBoolProp( IMP_FBX_LINK, true );
    ioSettings.SetBoolProp( IMP_FBX_SHAPE, true );
    ioSettings.SetBoolProp( IMP_FBX_GOBO, true );
    ioSettings.SetBoolProp( IMP_FBX_ANIMATION, true );
    ioSettings.SetBoolProp( IMP_FBX_GLOBAL_SETTINGS, true );

    bool importResult = importer->Import( fbxScene );
    if ( !importResult ) {
        DUSK_LOG_ERROR( "Failed to import FBX '%s' : %s\n", filePath, importer->GetStatus().GetErrorString() );
        return;
    }
    
    FbxGeometryConverter clsConverter( fbxManager );
    clsConverter.Triangulate( fbxScene, true );

    if ( importer->GetStatus().GetCode() != FbxStatus::eSuccess && importer->GetStatus().GetCode() != FbxStatus::eFailure ) {
        DUSK_LOG_WARN( "Imported FBX '%s' with one or several errors : %s\n", filePath, importer->GetStatus().GetErrorString() );
    }

    parsedModel = dk::core::allocate<ParsedModel>( memoryAllocator );

    dkString_t workingDir;
    dk::core::RetrieveWorkingDirectory( workingDir );
    
    parsedModel->ModelPath = WideStringToString(workingDir) + filePath;
    parsedModel->ModelPath = dk::core::SimplifyPath( parsedModel->ModelPath );
    
    parsedModel->ModelName = dk::core::GetFilenameOnlyFromPath( StringToWideString( parsedModel->ModelPath ) );

    parsedModel->LodCount = 1;
    
    // Iterate over geometry nodes
    i32 geometryNodeCount = fbxScene->GetGeometryCount();
    for ( i32 n = 0; n < geometryNodeCount; n++ ) {
        FbxGeometry* geometryNode = fbxScene->GetGeometry( n );
        FbxNode* node = geometryNode->GetNode();
        if ( node->GetNodeAttribute() == nullptr ) {
            continue;
        } 

        switch ( node->GetNodeAttribute()->GetAttributeType() ) {
        case FbxNodeAttribute::eMesh: {
            FbxMesh* mesh = static_cast< FbxMesh* >( geometryNode );

            ParsedMesh parsedMesh;
            parsedMesh.MemoryAllocator = memoryAllocator;

            // FBX exporter dont always export mesh name, so we should check the node name as a fallback
            const char* meshName = mesh->GetName();
            const char* nodeName = node->GetName();
            if ( meshName != nullptr && meshName[0] != '\0' ) {
                parsedMesh.Name = meshName;
            } else if ( nodeName != nullptr && nodeName[0] != '\0' ) {
                parsedMesh.Name = nodeName;
            } else {
                parsedMesh.Name = "ImportedMesh_" + std::to_string( n );
            }

            FbxNode* lodGroupNode = FindParentWithAttributesRecurse( node, FbxNodeAttribute::eLODGroup );
            if ( lodGroupNode != nullptr ) {
                parsedMesh.LodGroup = lodGroupNode->GetName();
            
                for ( i32 i = 0; i < lodGroupNode->GetChildCount(); i++ ) {
                    if ( lodGroupNode->GetChild( i ) == node ) {
                        parsedMesh.LodIndex = i;
                        break;
                    }
                }
            } else {
                // If the mesh does not have any lod, assume it is the LOD0 geometry.
                parsedMesh.LodGroup = "LOD0";
                parsedMesh.LodIndex = 0;
            }

            // Update the total lod count (we start with at least one lod).
            if ( parsedMesh.LodIndex > ( parsedModel->LodCount - 1 ) ) {
                parsedModel->LodCount++;
            }

            // Compute Mesh AABB
            mesh->ComputeBBox();

            FbxDouble3 aabbMin = mesh->BBoxMin.Get();
            dkVec3f aabbMinPoint = dkVec3f( 
                static_cast< f32 >( aabbMin.mData[0] ), 
                static_cast< f32 >( aabbMin.mData[1] ), 
                static_cast< f32 >( aabbMin.mData[2] ) 
            );

            FbxDouble3 aabbMax = mesh->BBoxMax.Get();
            dkVec3f aabbMaxPoint = dkVec3f( 
                static_cast< f32 >( aabbMax.mData[0] ), 
                static_cast< f32 >( aabbMax.mData[1] ), 
                static_cast< f32 >( aabbMax.mData[2] ) 
            );

            dk::maths::CreateAABBFromMinMaxPoints( parsedMesh.MeshAABB, aabbMinPoint, aabbMaxPoint );

            parsedMesh.IndexCount = mesh->GetPolygonVertexCount();
            parsedMesh.VertexCount = mesh->GetControlPointsCount();
            parsedMesh.TriangleCount = mesh->GetPolygonCount();

            DUSK_LOG_INFO( "'%hs' : started parsing Mesh '%hs' (vertex count: %i triangle count: %i index count: %i)\n", 
                           filePath, parsedMesh.Name.c_str(), parsedMesh.VertexCount, parsedMesh.TriangleCount, parsedMesh.IndexCount );

            // Assign mesh attributes flags
            parsedMesh.Flags.HasPositionAttribute = ( mesh->GetControlPointsCount() != 0 );
            parsedMesh.Flags.HasNormals = ( mesh->GetElementNormal() != nullptr );
            parsedMesh.Flags.HasUvmap0 = ( mesh->GetElementUV() != nullptr );
            parsedMesh.Flags.HasVertexColor0 = ( mesh->GetElementVertexColor() != nullptr );
            parsedMesh.Flags.IsIndexed = ( mesh->GetPolygonVertices() != nullptr );

            // Copy index buffer.
            i32* indexList = mesh->GetPolygonVertices();
            parsedMesh.IndexList = dk::core::allocateArray<i32>( memoryAllocator, mesh->GetPolygonVertexCount() );
            memcpy( parsedMesh.IndexList, indexList, mesh->GetPolygonVertexCount() * sizeof( i32 ) );

            // Copy vertex position. We don't iterate per polygon since we have to keep the vertices order for primitive
            // indexing.
            if ( mesh->GetControlPointsCount() != 0 ) {
                parsedMesh.PositionVertices = dk::core::allocateArray<dkVec3f>( memoryAllocator, mesh->GetControlPointsCount() );
                for ( i32 i = 0; i < mesh->GetControlPointsCount(); i++ ) {
                    parsedMesh.PositionVertices[i] = dkVec3f(
                        static_cast< f32 >( mesh->GetControlPointAt( i )[0] ),
                        static_cast< f32 >( mesh->GetControlPointAt( i )[1] ),
                        static_cast< f32 >( mesh->GetControlPointAt( i )[2] )
                    );
                }

            }

            if ( parsedMesh.Flags.HasNormals ) {
                parsedMesh.NormalVertices = dk::core::allocateArray<dkVec3f>( memoryAllocator, mesh->GetControlPointsCount() );

                FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
                if ( normalElement->GetMappingMode() == FbxGeometryElement::eByControlPoint ) {
                    for ( i32 vertexIndex = 0; vertexIndex < parsedMesh.VertexCount; vertexIndex++ ) {
                        i32 normalIndex = GetArrayIndexByReferenceMode( normalElement->GetReferenceMode(), normalElement->GetIndexArray(), vertexIndex );

                        FbxVector4 normal = normalElement->GetDirectArray().GetAt( normalIndex );

                        parsedMesh.NormalVertices[vertexIndex] = dkVec3f( 
                            static_cast< f32 >( normal[0] ), 
                            static_cast< f32 >( normal[1] ), 
                            static_cast< f32 >( normal[2] ) 
                        );
                    }
                } else if ( normalElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex ) {
                    for ( i32 triIdx = 0; triIdx < parsedMesh.TriangleCount; triIdx++ ) {
                        for ( i32 vertIdx = 0; vertIdx < 3; vertIdx++ ) {
                            i32 indexByPolygonVertex = ( triIdx * 3 ) + vertIdx;
                            i32 normalIndex = GetArrayIndexByReferenceMode( normalElement->GetReferenceMode(), normalElement->GetIndexArray(), indexByPolygonVertex );

                            FbxVector4 normal = normalElement->GetDirectArray().GetAt( normalIndex );

                            i32 vertexPolygon = parsedMesh.IndexList[indexByPolygonVertex];
                            parsedMesh.NormalVertices[vertexPolygon] = dkVec3f( 
                                static_cast< f32 >( normal[0] ), 
                                static_cast< f32 >( normal[1] ), 
                                static_cast< f32 >( normal[2] ) 
                            );
                        }
                    }
                }
            }

            if ( parsedMesh.Flags.HasUvmap0 ) {
                parsedMesh.TexCoordsVertices = dk::core::allocateArray<dkVec2f>( memoryAllocator, mesh->GetControlPointsCount() );
                
                FbxGeometryElementUV* uvElement = mesh->GetElementUV();
                if ( uvElement->GetMappingMode() == FbxGeometryElement::eByControlPoint ) {
                    for ( i32 vertexIndex = 0; vertexIndex < parsedMesh.VertexCount; vertexIndex++ ) {
                        i32 uvIndex = GetArrayIndexByReferenceMode( uvElement->GetReferenceMode(), uvElement->GetIndexArray(), vertexIndex );

                        FbxVector2 uv = uvElement->GetDirectArray().GetAt( uvIndex );

                        parsedMesh.TexCoordsVertices[vertexIndex] = dkVec2f(
                            static_cast< f32 >( uv[0] ),
                            static_cast< f32 >( uv[1] )
                        );
                    }
                } else if ( uvElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex ) {
                    for ( i32 triIdx = 0; triIdx < parsedMesh.TriangleCount; triIdx++ ) {
                        for ( i32 vertIdx = 0; vertIdx < 3; vertIdx++ ) {
                            i32 indexByPolygonVertex = ( triIdx * 3 ) + vertIdx;
                            i32 uvIndex = GetArrayIndexByReferenceMode( uvElement->GetReferenceMode(), uvElement->GetIndexArray(), indexByPolygonVertex );

                            FbxVector2 uv = uvElement->GetDirectArray().GetAt( uvIndex );

                            i32 vertexPolygon = parsedMesh.IndexList[indexByPolygonVertex];
                            parsedMesh.TexCoordsVertices[vertexPolygon] = dkVec2f(
                                static_cast< f32 >( uv[0] ),
                                static_cast< f32 >( uv[1] )
                            );
                        }
                    }
                }
            }

            if ( parsedMesh.Flags.HasVertexColor0 ) {
                parsedMesh.ColorVertices = dk::core::allocateArray<dkVec3f>( memoryAllocator, mesh->GetControlPointsCount() );

                FbxGeometryElementVertexColor* colorElement = mesh->GetElementVertexColor();
                if ( colorElement->GetMappingMode() == FbxGeometryElement::eByControlPoint ) {
                    for ( i32 vertexIndex = 0; vertexIndex < parsedMesh.VertexCount; vertexIndex++ ) {
                        i32 colorIndex = GetArrayIndexByReferenceMode( colorElement->GetReferenceMode(), colorElement->GetIndexArray(), vertexIndex );

                        FbxColor color = colorElement->GetDirectArray().GetAt( colorIndex );

                        parsedMesh.ColorVertices[vertexIndex] = dkVec3f(
                            static_cast< f32 >( color[0] ),
                            static_cast< f32 >( color[1] ),
                            static_cast< f32 >( color[2] )
                        );
                    }
                } else if ( colorElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex ) {
                    for ( i32 triIdx = 0; triIdx < parsedMesh.TriangleCount; triIdx++ ) {
                        for ( i32 vertIdx = 0; vertIdx < 3; vertIdx++ ) {
                            i32 indexByPolygonVertex = ( triIdx * 3 ) + vertIdx;
                            i32 colorIndex = GetArrayIndexByReferenceMode( colorElement->GetReferenceMode(), colorElement->GetIndexArray(), indexByPolygonVertex );

                            FbxColor color = colorElement->GetDirectArray().GetAt( colorIndex );

                            i32 vertexPolygon = parsedMesh.IndexList[indexByPolygonVertex];
                            parsedMesh.ColorVertices[vertexPolygon] = dkVec3f(
                                static_cast< f32 >( color[0] ),
                                static_cast< f32 >( color[1] ),
                                static_cast< f32 >( color[2] )
                            );
                        }
                    }
                }
            }

            parsedModel->ModelMeshes.push_back( parsedMesh );
        } break;
        }
    }

    DUSK_LOG_INFO( "'%hs' : parsed %i meshes\n", filePath, parsedModel->ModelMeshes.size() );
}

ParsedModel* FbxParser::getParsedModel() const
{
    return parsedModel;
}
#endif