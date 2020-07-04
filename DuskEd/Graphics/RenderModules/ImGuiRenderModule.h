/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_DEVBUILD
#if DUSK_USE_IMGUI
class RenderDevice;
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
#include <atomic>

class ImGuiRenderModule
{
public:
                                    ImGuiRenderModule();
                                    ImGuiRenderModule( ImGuiRenderModule& ) = delete;
                                    ImGuiRenderModule& operator = ( ImGuiRenderModule& ) = delete;
                                    ~ImGuiRenderModule();

    void                            loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );
    void                            destroy( RenderDevice& renderDevice );

    ResHandle_t                     render( FrameGraph& frameGraph, MutableResHandle_t renderTarget );

    void                            lockForUpload();

    void                            lockForRendering();

	void                            unlock();

private:
    i32                             imguiCmdListCount;
    ImDrawList**                    imguiCmdLists;

    Image*                          fontAtlas;

    dkVec2f                         displaySize;
    dkVec2f                         displayPos;

    Buffer*                         vertexBuffer;
    Buffer*                         indiceBuffer;

    std::atomic<i32>                renderingLock;
    
private:
	bool                            acquireLock( const i32 nextState );

    void                            update( CommandList& cmdList );
};
#endif
#endif