/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class RenderDevice;
class WorldRenderer;
class Mesh;
class VertexArrayObject;
class LightGrid;
class LinearAllocator;
class StackAllocator;
class Material;
class GraphicsAssetCache;

struct CameraData;
struct IBLProbeData;
struct AABB;

#include <stack>

#include <Shared.h>
#include <Maths/Vector.h>
#include <Maths/Matrix.h>

enum eProbeCaptureStep : uint16_t
{
    FACE_X_PLUS = 0,
    FACE_X_MINUS,

    FACE_Y_PLUS,
    FACE_Y_MINUS,

    FACE_Z_PLUS,
    FACE_Z_MINUS,
};

class DrawCommandBuilder
{
// TODO Cleaner way to store debug/devbuild specific resources?
#if DUSK_DEVBUILD
public:
    Material*                   MaterialDebugIBLProbe;
    Material*                   MaterialDebugWireframe;
#endif

public:
                                DrawCommandBuilder( BaseAllocator* allocator );
                                DrawCommandBuilder( DrawCommandBuilder& ) = delete;
                                DrawCommandBuilder& operator = ( DrawCommandBuilder& ) = delete;
                                ~DrawCommandBuilder();

#if DUSK_DEVBUILD
    void                        loadDebugResources( GraphicsAssetCache* graphicsAssetCache );
#endif

    void                        addGeometryToRender( const Mesh* meshResource, const dkMat4x4f* modelMatrix, const uint32_t flagset );
    void                        addSphereToRender( const dkVec3f& sphereCenter, const float sphereRadius, Material* material );
    void                        addAABBToRender( const AABB& aabb, Material* material );
    void                        addCamera( CameraData* cameraData );
    void                        addIBLProbeToCapture( const IBLProbeData* probeData );
    void                        addHUDRectangle( const dkVec2f& positionScreenSpace, const dkVec2f& dimensionScreenSpace, const float rotationInRadians, Material* material );

    void                        addHUDText( const dkVec2f& positionScreenSpace, const float size, const dkVec4f& colorAndAlpha, const std::string& value );

    void                        buildRenderQueues( WorldRenderer* worldRenderer, LightGrid* lightGrid );

private:
    struct MeshInstance {
        const Mesh*         mesh;
        const dkMat4x4f*    modelMatrix;

        // TODO Unify with Scene mesh instance flagset
        union {
            struct {
                uint8_t isVisible : 1;
                uint8_t renderDepth : 1;
                uint8_t useBatching : 1;
                uint8_t : 0;
            };

            uint32_t    flags;
        };
    };

    struct IBLProbeCaptureCommand {
        union {
            // Higher bytes contains probe index (from the lowest to the highest one available in the queue)
            // Lower bytes contains face to render (not used in queue sort)
            struct {
                uint32_t            EnvProbeArrayIndex;
                eProbeCaptureStep   Step;
                uint16_t            __PADDING__;
            } CommandInfos;

            int64_t ProbeKey;
        };

        const IBLProbeData*   Probe;

        inline bool operator < ( const IBLProbeCaptureCommand& cmd ) {
            return ProbeKey < cmd.ProbeKey;
        }
    };

    struct IBLProbeConvolutionCommand {
        union {
            // Higher bytes contains probe index (from the lowest to the highest one available in the queue)
            // Lower bytes contains mip index (with padding)
            struct {
                uint32_t            EnvProbeArrayIndex;
                eProbeCaptureStep   Step;
                uint16_t            MipIndex;
            } CommandInfos;

            int64_t ProbeKey;
        };

        const IBLProbeData* Probe;

        inline bool operator < ( const IBLProbeConvolutionCommand& cmd ) {
            return ProbeKey < cmd.ProbeKey;
        }
    };

    struct PrimitiveInstance {
        dkMat4x4f      modelMatrix;
        Material*       material;
    };

    struct TextDrawCommand
    {
        std::string     stringToPrint;
        dkVec4f        color;
        dkVec2f        positionScreenSpace;
        float           scale;
    };

private:
    BaseAllocator*                          memoryAllocator;

    LinearAllocator*                          cameras;
    LinearAllocator*                          meshes;
    LinearAllocator*                          spheresToRender;
    dkMat4x4f                              sphereToRender[8192];

    LinearAllocator*                          primitivesToRender;
    dkMat4x4f                              primitivesModelMatricess[8192];

    LinearAllocator*                          textToRenderAllocator;

    StackAllocator*                         probeCaptureCmdAllocator;
    StackAllocator*                         probeConvolutionCmdAllocator;

    std::stack<IBLProbeCaptureCommand*>     probeCaptureCmds;
    std::stack<IBLProbeConvolutionCommand*> probeConvolutionCmds;

private:
    void                        resetEntityCounters();
    void                        buildMeshDrawCmds( WorldRenderer* worldRenderer, CameraData* camera, const uint8_t cameraIdx, const uint8_t layer, const uint8_t viewportLayer );
    void                        buildHUDDrawCmds( WorldRenderer* worldRenderer, CameraData* camera, const uint8_t cameraIdx );
};
