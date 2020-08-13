/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Graphics/FrameGraph.h"
#include <Rendering/CommandList.h>
#include <Maths/Matrix.h>

#include "Material.h"

class LightGrid;
class RenderDevice;
class ShaderCache;
class GraphicsAssetCache;
class PrimitiveCache;
class BaseAllocator;
class PoolAllocator;
class LinearAllocator;
class CommandList;
class FrameGraph;
class Material;
class VirtualFileSystem;
class TextRenderingModule;
class AutomaticExposureModule;
class GlareRenderModule;
class LineRenderingModule;
class FrameCompositionModule;
class AtmosphereRenderModule;
class WorldRenderModule;
class IBLUtilitiesModule;
class EnvironmentProbeStreaming;
class CascadedShadowRenderModule;
class RenderWorld;

struct CameraData;
struct Buffer;
struct Image;

struct GPUBatchData
{
	dkMat4x4f                       ModelMatrix;
	dkVec3f                         BoundingSphereCenter;
	f32                             BoundingSphereRadius;
};

struct GPUShadowDrawCmd
{
    GPUBatchData*       InstancesData;
    u32                 ShadowMeshBatchIndex;
    u32                 InstanceCount;
};

class WorldRenderer
{
public:
    // TODO Avoid exposing stuff like this...
    // We should be able to get rid of this shitty dependency once picking is correctly split.
    WorldRenderModule*       WorldRendering;

    // Return a pointer to the AtmosphereRenderModule instance. Shouldn't be useful outside the editor context.
    DUSK_INLINE AtmosphereRenderModule* getAtmosphereRenderingModule() const { return atmosphereRendering; }

    // Return a pointer to this renderer light grid.  Shouldn't be useful outside the editor context.
    DUSK_INLINE LightGrid*              getLightGrid() const { return lightGrid; }

    // Return a pointer to the CascadedShadowMap render module instance. Shouldn't be useful outside the editor context.
    DUSK_INLINE CascadedShadowRenderModule* getCascadedShadowRenderModule() const { return cascadedShadowMapRendering; }

public:
                     WorldRenderer( BaseAllocator* allocator );
                     WorldRenderer( WorldRenderer& ) = default;
                     ~WorldRenderer();

    void             destroy( RenderDevice* renderDevice );
    void             loadCachedResources( RenderDevice* renderDevice, ShaderCache* shaderCache, GraphicsAssetCache* graphicsAssetCache, VirtualFileSystem* virtualFileSystem );

    void             drawDebugSphere( CommandList& cmdList );

    void             drawWorld( RenderDevice* renderDevice, const f32 deltaTime );

    DrawCmd&            allocateDrawCmd();

    DrawCmd&            allocateSpherePrimitiveDrawCmd();

    GPUShadowDrawCmd&   allocateGPUShadowCullDrawCmd();

    // Prepare the FrameGraph instance for the next frame.
    FrameGraph&      prepareFrameGraph( const Viewport& viewport, const ScissorRegion& scissor, const CameraData* camera = nullptr );

    // Return a pointer to the wireframe material.
    const Material*  getWireframeMaterial() const;

    // Append the render passes to build the default world render pipeline.
    FGHandle      buildDefaultGraph( FrameGraph& frameGraph, const Material::RenderScenario scenario, const dkVec2f& viewportSize, RenderWorld* renderWorld );

    // Return the virtual resource handle to the resolved depth buffer (or the regular depth buffer if multisampling is disabled).
    FGHandle      getResolvedDepth();

private:
    // The memory allocator owning this instance.
    BaseAllocator*   memoryAllocator;

    // Cache to precompute and store basic primitives (for debug or special stuff).
    PrimitiveCache*  primitiveCache;

    // Draw Command allocator.
    LinearAllocator* drawCmdAllocator;

	// GPU Shadow Cull Command allocator.
	LinearAllocator* gpuShadowCullAllocator;

    // The FrameGraph used to render the world.
    FrameGraph*      frameGraph;

    // Current frame draw commands.
    DrawCmd*         frameDrawCmds;

    // If true, RenderModules need to precompute its transistent resources for frame rendering.
    bool             needResourcePrecompute;

    // Material to render wireframe geometry (e.g. debug primitives).
    Material*        wireframeMaterial;

    // Default BRDF DFG LUT. Probably at the wrong place but idk where it should now.
    Image*           brdfDfgLut;

    // Manage light assignation/clustering in the world.
    LightGrid*       lightGrid;

    // Update dynamic environment probe and stream precomputed ones from the disk.
    EnvironmentProbeStreaming* environmentProbeStreaming;

    // Current frame resolved depth render target (if the frame had at least one depth render pass).
    FGHandle      resolvedDepth;

    // Frame composition render module (builds final image at the end of the post-fx pipeline).
    FrameCompositionModule* frameComposition;

    // Glare rendering module (physically-based bloom).
    GlareRenderModule* glareRendering;

    // Automatic exposure compute module (histogram based).
    AutomaticExposureModule* automaticExposure;

    // Atmosphere (sky + atmospheric fog) rendering module.
    AtmosphereRenderModule* atmosphereRendering;

    // CSM rendering (+ implicit depth pyramid compute for auto depth bound compute).
    CascadedShadowRenderModule* cascadedShadowMapRendering;
};
