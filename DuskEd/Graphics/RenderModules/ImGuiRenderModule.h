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

#include <Maths/Matrix.h>
#include <atomic>

#include "Graphics/FrameGraph.h"

class ImGuiRenderModule
{
public:
                                    ImGuiRenderModule();
                                    ImGuiRenderModule( ImGuiRenderModule& ) = delete;
                                    ImGuiRenderModule& operator = ( ImGuiRenderModule& ) = delete;
                                    ~ImGuiRenderModule();

    void                            loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );
    void                            destroy( RenderDevice& renderDevice );

    FGHandle                        render( FrameGraph& frameGraph, FGHandle renderTarget );

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