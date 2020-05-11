/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D11
struct ID3D11VertexShader;
struct ID3D11HullShader;
struct ID3D11DomainShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D11InputLayout;
enum D3D_PRIMITIVE_TOPOLOGY;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;

#include <unordered_map>
#include <d3d11.h>

struct PipelineState
{
    ID3D11VertexShader*                 vertexShader;
    ID3D11HullShader*                   tesselationControlShader;
    ID3D11DomainShader*                 tesselationEvalShader;
    ID3D11PixelShader*                  pixelShader;
    ID3D11ComputeShader*                computeShader;

    UINT                                blendMask;
    UINT                                stencilRef;

    D3D11_PRIMITIVE_TOPOLOGY            PrimitiveTopology;
    ID3D11BlendState*                   BlendState;
    ID3D11RasterizerState*              RasterizerState;
    ID3D11DepthStencilState*            DepthStencilState;
    ID3D11InputLayout*                  inputLayout;
    
    // BlendState description key (for fast state caching).
    BlendStateDesc                      BlendStateKey;
    RasterizerStateDesc                 RaterizerStateKey;
    DepthStencilStateDesc               DepthStencilStateKey;

    ID3D11SamplerState*                 staticSamplers[8];
    u32                                 staticSamplerCount;

    enum class InternalResourceType {
        RESOURCE_TYPE_UNKNOWN,
        RESOURCE_TYPE_SHADER_RESOURCE_VIEW,
        RESOURCE_TYPE_UNORDERED_ACCESS_VIEW,
        RESOURCE_TYPE_CBUFFER_VIEW
    };

    struct ResourceEntry {
        struct StageBinding {
            union {
                ID3D11ShaderResourceView**   shaderResourceView;
                ID3D11UnorderedAccessView**  unorderedAccessView;
                ID3D11SamplerState**         samplerState;
                ID3D11Buffer**               cbufferView;
            };
            u32 RegisterIndex;
            i32 ShaderStageIndex;
        } stageBindings[eShaderStage::SHADER_STAGE_COUNT];
        StageBinding*   activeBindings[eShaderStage::SHADER_STAGE_COUNT];
        
        i32 activeStageCount;
        InternalResourceType type;

        ResourceEntry()
            : type( InternalResourceType::RESOURCE_TYPE_UNKNOWN )
            , activeStageCount( 0 )
        {
            memset( stageBindings, 0, sizeof( StageBinding ) * eShaderStage::SHADER_STAGE_COUNT );
            memset( activeBindings, 0, sizeof( StageBinding* ) );
        }
    } resources[PipelineStateDesc::MAX_RESOURCE_COUNT];
    
    i32 resourceCount;
    std::unordered_map<dkStringHash_t, ResourceEntry*>  bindingSet;

    template<typename Res, i32 MAX>
    struct ResourceBinding {
        Res  VERTEX[MAX];
        Res  TESSELATION_CONTROL[MAX];
        Res  TESSELATION_EVALUATION[MAX];
        Res  PIXEL[MAX];
        Res  COMPUTE[MAX];
    };

    ResourceBinding<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT>  shaderResourceViews;
    ResourceBinding<ID3D11Buffer*, D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT>   constantBuffers;
    ResourceBinding<ID3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT>        samplers;

    ID3D11UnorderedAccessView*                  uavBuffers[16];
    UINT                                        uavBuffersBindCount;

    FLOAT                       colorClearValue[4];
    FLOAT                       depthClearValue;
    u8                          stencilClearValue;

    DXGI_FORMAT                 rtvFormat[8];
    DXGI_FORMAT                 dsvFormat;
    bool                        clearRtv[8];
    bool                        clearDsv;
    i32                         rtvCount;
};

DUSK_INLINE void BindPipelineState_Replay( RenderContext* renderContext, PipelineState* pipelineState );
DUSK_INLINE void PrepareAndBindResources_Replay( RenderContext* renderContext, const PipelineState* pipelineState );
DUSK_INLINE void BindBuffer_Replay( RenderContext* renderContext, const dkStringHash_t hashcode, Buffer* buffer );  
DUSK_INLINE void BindImage_Replay( RenderContext* renderContext, const dkStringHash_t hashcode, Image* image );

void FlushSRVRegisterUpdate( RenderContext* renderContext );
void FlushCBufferRegisterUpdate( RenderContext* renderContext );
void FlushUAVRegisterUpdate( RenderContext* renderContext );

bool ClearImageUAVRegister( RenderContext* renderContext, Image* image );
bool ClearImageSRVRegister( RenderContext* renderContext, Image* image );

void ClearImageFBOAttachment( RenderContext* renderContext, Image* image );

bool ClearBufferSRVRegister( RenderContext* renderContext, Buffer* buffer );
bool ClearBufferUAVRegister( RenderContext* renderContext, Buffer* buffer );
#endif
