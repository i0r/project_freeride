/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Maths/Matrix.h>

//enum eProbeCaptureStep : uint16_t
//{
//    FACE_X_PLUS = 0,
//    FACE_X_MINUS,
//
//    FACE_Y_PLUS,
//    FACE_Y_MINUS,
//
//    FACE_Z_PLUS,
//    FACE_Z_MINUS,
//};

class BaseAllocator;
class LinearAllocator;
class WorldRenderer;
class LightGrid;
class Model;
class FrameGraph;
struct CameraData;

class DrawCommandBuilder2
{
public:
						DrawCommandBuilder2( BaseAllocator* allocator );
						DrawCommandBuilder2( DrawCommandBuilder2& ) = delete;
						DrawCommandBuilder2& operator = ( DrawCommandBuilder2& ) = delete;
						~DrawCommandBuilder2();

	// Add a world camera to render. A camera should be added to the list only if 
	// it require to render the active world geometry.
    void    			addWorldCameraToRender( CameraData* cameraData );

	// Add a static model instance to the world. If the model has already been added
	// to the world, the model will automatically use instantied draw (no action/
	// batching is required by the user).
	void				addStaticModelInstance( const Model* model, const dkMat4x4f& modelMatrix, const u32 entityIndex = 0 );

	// Build render queues for each type of render scenario and enqueued cameras.
	// This call will also update/stream the light grid entities.
	void				prepareAndDispatchCommands( WorldRenderer* worldRenderer, LightGrid* lightGrid );

private:
	// The memory allocator owning this instance.
	BaseAllocator*		memoryAllocator;

	// Allocator used to allocate local copies of incoming cameras.
    LinearAllocator*	cameraToRenderAllocator;

	// Allocator used to allocate local copies of incoming models.
	LinearAllocator*	staticModelsToRender;

	// Allocator used to allocate instance data to render batched models/geometry.
	LinearAllocator*	instanceDataAllocator;

private:
	// Reset entities allocators (camera, models, etc.). Should be called once the cmds are built and we don't longer need
	// the transistent data.
	void				resetAllocators();

	void                buildGeometryDrawCmds( WorldRenderer* worldRenderer, const CameraData* camera, const u8 cameraIdx, const u8 layer, const u8 viewportLayer ); 
};

//
//class DrawCommandBuilder
//{
//// TODO Cleaner way to store debug/devbuild specific resources?
//#if DUSK_DEVBUILD
//public:
//    Material*                   MaterialDebugIBLProbe;
//    Material*                   MaterialDebugWireframe;
//#endif
//
//public:
//                                DrawCommandBuilder( BaseAllocator* allocator );
//                                DrawCommandBuilder( DrawCommandBuilder& ) = delete;
//                                DrawCommandBuilder& operator = ( DrawCommandBuilder& ) = delete;
//                                ~DrawCommandBuilder();
//
//#if DUSK_DEVBUILD
//    void                        loadDebugResources( GraphicsAssetCache* graphicsAssetCache );
//#endif
//
//    void                        addGeometryToRender( const Mesh* meshResource, const dkMat4x4f* modelMatrix, const uint32_t flagset );
//    void                        addSphereToRender( const dkVec3f& sphereCenter, const float sphereRadius, Material* material );
//    void                        addAABBToRender( const AABB& aabb, Material* material );
//    void                        addIBLProbeToCapture( const IBLProbeData* probeData );
//    void                        addHUDRectangle( const dkVec2f& positionScreenSpace, const dkVec2f& dimensionScreenSpace, const float rotationInRadians, Material* material );
//
//    void                        addHUDText( const dkVec2f& positionScreenSpace, const float size, const dkVec4f& colorAndAlpha, const std::string& value );
//
//    void                        buildRenderQueues( WorldRenderer* worldRenderer, LightGrid* lightGrid );
//
//private:
//    struct MeshInstance {
//        const Mesh*         mesh;
//        const dkMat4x4f*    modelMatrix;
//
//        // TODO Unify with Scene mesh instance flagset
//        union {
//            struct {
//                uint8_t isVisible : 1;
//                uint8_t renderDepth : 1;
//                uint8_t useBatching : 1;
//                uint8_t : 0;
//            };
//
//            uint32_t    flags;
//        };
//    };
//
//    struct IBLProbeCaptureCommand {
//        union {
//            // Higher bytes contains probe index (from the lowest to the highest one available in the queue)
//            // Lower bytes contains face to render (not used in queue sort)
//            struct {
//                uint32_t            EnvProbeArrayIndex;
//                eProbeCaptureStep   Step;
//                uint16_t            __PADDING__;
//            } CommandInfos;
//
//            int64_t ProbeKey;
//        };
//
//        const IBLProbeData*   Probe;
//
//        inline bool operator < ( const IBLProbeCaptureCommand& cmd ) {
//            return ProbeKey < cmd.ProbeKey;
//        }
//    };
//
//    struct IBLProbeConvolutionCommand {
//        union {
//            // Higher bytes contains probe index (from the lowest to the highest one available in the queue)
//            // Lower bytes contains mip index (with padding)
//            struct {
//                uint32_t            EnvProbeArrayIndex;
//                eProbeCaptureStep   Step;
//                uint16_t            MipIndex;
//            } CommandInfos;
//
//            int64_t ProbeKey;
//        };
//
//        const IBLProbeData* Probe;
//
//        inline bool operator < ( const IBLProbeConvolutionCommand& cmd ) {
//            return ProbeKey < cmd.ProbeKey;
//        }
//    };
//
//    struct PrimitiveInstance {
//        dkMat4x4f      modelMatrix;
//        Material*       material;
//    };
//
//    struct TextDrawCommand
//    {
//        std::string     stringToPrint;
//        dkVec4f        color;
//        dkVec2f        positionScreenSpace;
//        float           scale;
//    };
//
//private:
//    
//    LinearAllocator*                          meshes;
//    LinearAllocator*                          spheresToRender;
//    dkMat4x4f                              sphereToRender[8192];
//
//    LinearAllocator*                          primitivesToRender;
//    dkMat4x4f                              primitivesModelMatricess[8192];
//
//    LinearAllocator*                          textToRenderAllocator;
//
//    StackAllocator*                         probeCaptureCmdAllocator;
//    StackAllocator*                         probeConvolutionCmdAllocator;
//
//    std::stack<IBLProbeCaptureCommand*>     probeCaptureCmds;
//    std::stack<IBLProbeConvolutionCommand*> probeConvolutionCmds;
//
//private:
//    void                        resetEntityCounters();
//    void                        buildMeshDrawCmds( WorldRenderer* worldRenderer, CameraData* camera, const uint8_t cameraIdx, const uint8_t layer, const uint8_t viewportLayer );
//    void                        buildHUDDrawCmds( WorldRenderer* worldRenderer, CameraData* camera, const uint8_t cameraIdx );
//};
