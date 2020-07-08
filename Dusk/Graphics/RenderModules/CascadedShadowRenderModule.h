/*
    Dusk Source Code
    Copyright (C) 2020209 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class GraphicsAssetCache;

using ResHandle_t = uint32_t;

class CascadedShadowRenderModule
{
public:
                        CascadedShadowRenderModule();
                        CascadedShadowRenderModule( CascadedShadowRenderModule& ) = delete;
                        CascadedShadowRenderModule& operator = ( CascadedShadowRenderModule& ) = delete;
                        ~CascadedShadowRenderModule();

    void                destroy( RenderDevice& renderDevice );

	void                loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

    void                captureShadowMap( FrameGraph& frameGraph, ResHandle_t depthBuffer, const dkVec2f& depthBufferSize, const DirectionalLightGPU& directionalLight );

private:
    ResHandle_t         reduceDepthBuffer( FrameGraph& frameGraph, ResHandle_t depthBuffer, const dkVec2f& depthBufferSize );

private:
    Buffer*             csmSliceInfosBuffer;
};
