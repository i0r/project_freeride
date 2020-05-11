/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_DEVBUILD
#if DUSK_USE_IMGUI
class RenderDevice;
class ShaderCache;
class GraphicsAssetCache;
class FrameGraph;
class CommandList;

struct ImDrawList;
struct PipelineState;
struct Image;
struct Buffer;

using ResHandle_t = u32;
using MutableResHandle_t = u32;

#include <Maths/Matrix.h>

class ImGuiRenderModule
{
public:
                                    ImGuiRenderModule();
                                    ImGuiRenderModule( ImGuiRenderModule& ) = delete;
                                    ImGuiRenderModule& operator = ( ImGuiRenderModule& ) = delete;
                                    ~ImGuiRenderModule();

    void                            loadCachedResources( RenderDevice& renderDevice, ShaderCache& shaderCache, GraphicsAssetCache& graphicsAssetCache );
    void                            destroy( RenderDevice& renderDevice );

    ResHandle_t                     render( FrameGraph& frameGraph, MutableResHandle_t renderTarget );

    // Should be called whenever the main thread wants to rebuild ImGui renderlist.
    // This is a blocking function (the function might spin until the RenderPass has finished its vertex buffer update).
    void                            lockRenderList();

    // Should be called once the renderlist update is done.
    void                            unlockRenderList();

private:
    i32                             imguiCmdListCount;
    ImDrawList**                    imguiCmdLists;

    Image*                          fontAtlas;

    dkVec2f                         displaySize;
    dkVec2f                         displayPos;

    Buffer*                         vertexBuffer;
    Buffer*                         indiceBuffer;

    // True if the main thread is currently updating ImGui renderlist.
    std::atomic<bool>               isRenderListLocked;

    // True if the render thread is currently uploading the vertices to the GPU.
    std::atomic<bool>               isRenderListRendering;
    
private:
    void                            update( CommandList& cmdList );

    // Return isRenderListLocked state. This function is thread safe.
    bool                            isRenderListAvailable();

    // Return isRenderListRendering state. This function is thread safe.
    bool                            isRenderListUploadDone();
};
#endif
#endif