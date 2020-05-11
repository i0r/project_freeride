/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D11
#include <d3d11.h>
#include <d3d11_1.h>

struct RenderContext
{
                                RenderContext();
                                ~RenderContext();

    // Immediate render device (logical instance).
    ID3D11DeviceContext*	    ImmediateContext;

    // Physical render device.
    ID3D11Device*			    PhysicalDevice;

    // Device swapchain.
    IDXGISwapChain*			    SwapChain;

    // Interval for buffer swap.
    UINT                        SynchronisationInterval;

#if DUSK_ENABLE_GPU_DEBUG_MARKER
    // Debug Marker module.
    ID3DUserDefinedAnnotation*  Annotation;
#endif

    struct RegisterData {
        enum class Type {
            UNUSED = 0,
            IMAGE_RESOURCE,
            BUFFER_RESOURCE
        } ResourceType;

        union {
            Image* ImageObject;
            Buffer* BufferObject;
        };

        RegisterData()
            : ResourceType( Type::UNUSED )
            , ImageObject( nullptr )
        {

        }
    };

    // Active Viewport (modified during command replay).
    Viewport                    ActiveViewport;

    // Active ScissorRegion (modified during command replay).
    ScissorRegion               ActiveScissor;

    RegisterData                CsUavRegistersInfo[D3D11_1_UAV_SLOT_COUNT];

    RegisterData                SrvRegistersInfo[eShaderStage::SHADER_STAGE_COUNT][D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT];

    // Current UAV register usage (for compute shader stage).
    ID3D11UnorderedAccessView*  CsUavRegisters[D3D11_1_UAV_SLOT_COUNT];

    u32                         CsUavRegisterUpdateStart;

    i32                         CsUavRegisterUpdateCount;

    u32                         SrvRegisterUpdateStart[eShaderStage::SHADER_STAGE_COUNT];

    i32                         SrvRegisterUpdateCount[eShaderStage::SHADER_STAGE_COUNT];

    // Current SRV register usage (for each shader stages).
    ID3D11ShaderResourceView*   SrvRegisters[eShaderStage::SHADER_STAGE_COUNT][D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT];

    u32                         CBufferRegisterUpdateStart[eShaderStage::SHADER_STAGE_COUNT];

    i32                         CBufferRegisterUpdateCount[eShaderStage::SHADER_STAGE_COUNT];

    // Current cbuffer register usage (for each shader stages).
    ID3D11Buffer*               CBufferRegisters[eShaderStage::SHADER_STAGE_COUNT][D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_COUNT];

    // Current framebuffer attachments.
    Image*                      FramebufferAttachment[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];

    // Current framebuffer depth buffer.
    Image*                      FramebufferDepthBuffer;

    // Active PipelineState.
    PipelineState*              BindedPipelineState;
};
#endif
