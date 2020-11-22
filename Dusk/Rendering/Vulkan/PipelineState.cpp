/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_VULKAN
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "RenderDevice.h"
#include "CommandList.h"
#include "PipelineState.h"
#include "ComparisonFunctions.h"
#include "ImageHelpers.h"
#include "Buffer.h"
#include "Sampler.h"
#include "Image.h"
#include "ImageHelpers.h"

#include <Core/Allocators/LinearAllocator.h>
#include <Maths/Helpers.h>

#define SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS 1
#include "ThirdParty/SPIRV-Cross/src/spirv_cross.hpp"

#include "vulkan.h"

static constexpr VkPrimitiveTopology VK_PRIMITIVE_TOPOLOGY[ePrimitiveTopology::PRIMITIVE_TOPOLOGY_COUNT] =
{
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
};

static constexpr VkBlendFactor VK_BLEND_SOURCE[eBlendSource::BLEND_SOURCE_COUNT] =
{
    VK_BLEND_FACTOR_ZERO,
    VK_BLEND_FACTOR_ONE,

    VK_BLEND_FACTOR_SRC_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,

    VK_BLEND_FACTOR_SRC_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,

    VK_BLEND_FACTOR_DST_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,

    VK_BLEND_FACTOR_DST_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,

    VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,

    VK_BLEND_FACTOR_CONSTANT_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
};

static constexpr VkBlendOp VK_BLEND_OPERATION[eBlendOperation::BLEND_OPERATION_COUNT] =
{
    VK_BLEND_OP_ADD,
    VK_BLEND_OP_SUBTRACT,
    VK_BLEND_OP_MIN,
    VK_BLEND_OP_MAX,
};

static constexpr VkStencilOp VK_STENCIL_OPERATION[eStencilOperation::STENCIL_OPERATION_COUNT] =
{
    VK_STENCIL_OP_KEEP,
    VK_STENCIL_OP_ZERO,
    VK_STENCIL_OP_REPLACE,
    VK_STENCIL_OP_INCREMENT_AND_WRAP,
    VK_STENCIL_OP_INCREMENT_AND_CLAMP,
    VK_STENCIL_OP_DECREMENT_AND_WRAP,
    VK_STENCIL_OP_DECREMENT_AND_CLAMP,
    VK_STENCIL_OP_INVERT
};

static constexpr VkPolygonMode VK_FM[eFillMode::FILL_MODE_COUNT] =
{
    VK_POLYGON_MODE_FILL,
    VK_POLYGON_MODE_LINE,
};

static constexpr VkCullModeFlags VK_CM[eCullMode::CULL_MODE_COUNT] =
{
    VK_CULL_MODE_NONE,
    VK_CULL_MODE_FRONT_BIT,
    VK_CULL_MODE_BACK_BIT
};

struct Shader
{
    VkShaderModule              shaderModule;
    VkShaderStageFlagBits    stageFlags;

    // Compute Infos
    u32 workGroupSizeX;
    u32 workGroupSizeY;
    u32 workGroupSizeZ;

    // Misc Infos
    struct {
        std::string name;

        u32 descriptorSetIndex;
        u32 bindingIndex;

        VkDescriptorType type;
        size_t resourceSize;

        struct {
            size_t offset;
            size_t range;
        } rangeBinding;
    } resourcesReflectionInfos[96];

    u32 reflectedResourceCount;
};

DUSK_INLINE VkShaderStageFlags GetDescriptorStageFlags( const uint32_t shaderStageBindBitfield )
{
    VkShaderStageFlags bindFlags = 0u;

    if ( shaderStageBindBitfield & SHADER_STAGE_VERTEX ) {
        bindFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    }

    if ( shaderStageBindBitfield & SHADER_STAGE_TESSELATION_CONTROL ) {
        bindFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    }

    if ( shaderStageBindBitfield & SHADER_STAGE_TESSELATION_EVALUATION ) {
        bindFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }

    if ( shaderStageBindBitfield & SHADER_STAGE_PIXEL ) {
        bindFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    if ( shaderStageBindBitfield & SHADER_STAGE_COMPUTE ) {
        bindFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
    }

    return bindFlags;
}

DUSK_INLINE VkShaderStageFlagBits GetVkStageFlags( const eShaderStage shaderStage )
{
    switch ( shaderStage ) {
    case SHADER_STAGE_VERTEX:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case SHADER_STAGE_TESSELATION_CONTROL:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case SHADER_STAGE_TESSELATION_EVALUATION:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case SHADER_STAGE_PIXEL:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case SHADER_STAGE_COMPUTE:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
        return VK_SHADER_STAGE_ALL_GRAPHICS;
    }
}

void ReflectShaderResources( spirv_cross::Compiler* compiler,  Shader* shader, const spirv_cross::SmallVector<spirv_cross::Resource>& allResources, const VkDescriptorType descriptorType )
{
    for ( size_t i = 0; i < allResources.size(); ++i ) {
        auto& resource = allResources[i];
        const spirv_cross::SPIRType type = compiler->get_type( resource.type_id );

        u32 descriptorSet = compiler->get_decoration( resource.id, spv::DecorationDescriptorSet );
        u32 binding = compiler->get_decoration( resource.id, spv::DecorationBinding );

        const spirv_cross::SmallVector<spirv_cross::BufferRange> bufferRanges = compiler->get_active_buffer_ranges( resource.id );

        auto& reflectedResource = shader->resourcesReflectionInfos[shader->reflectedResourceCount++];
        reflectedResource.name = resource.name;
        reflectedResource.bindingIndex = binding;
        reflectedResource.descriptorSetIndex = descriptorSet;

        reflectedResource.type = descriptorType;

        if ( type.image.dim == spv::Dim::DimBuffer  ) {
            if ( descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ) {
                reflectedResource.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            } else if ( descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ) {
                reflectedResource.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            }
        }

        //reflectedResource.resourceSize = compiler->get_declared_struct_size(type);

        // Align buffer offset to respect device limits
        reflectedResource.rangeBinding.offset = 0; //bufferRanges[0].offset > 0x100 ? ( bufferRanges[0].offset + ( 0x100 - ( bufferRanges[0].offset % 0x100 ) ) ) : bufferRanges[0].offset;
        reflectedResource.rangeBinding.range = VK_WHOLE_SIZE; //( bufferRanges.size() == 1 ) ? bufferRanges[0].range : VK_WHOLE_SIZE;
   }
}

Shader* RenderDevice::createShader( const eShaderStage stage, const void* bytecode, const size_t bytecodeSize )
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0u;
    createInfo.codeSize = bytecodeSize;
    createInfo.pCode = reinterpret_cast< const u32* >( bytecode );

    VkShaderModule shaderModule;
    VkResult operationResult = vkCreateShaderModule( renderContext->device, &createInfo, nullptr, &shaderModule );
    DUSK_ASSERT( operationResult == VK_SUCCESS, "Failed to load precompiled shader (error code: 0x%x)", operationResult )

    Shader* shader = dk::core::allocate<Shader>( memoryAllocator );
    shader->shaderModule = shaderModule;
    shader->stageFlags = GetVkStageFlags( stage );

    // Retrieve reflection infos
    spirv_cross::Compiler* compiler = new spirv_cross::Compiler( (uint32_t*)bytecode, bytecodeSize / sizeof( uint32_t ) );
    std::string& entryPointName = compiler->get_entry_points_and_stages()[0].name;

   /* if ( stage == eShaderStage::SHADER_STAGE_COMPUTE )  {
        spirv_cross::SPIREntryPoint* pEntryPoint = &compiler->get_entry_point(entryPointName, compiler->get_execution_model());

        shader->workGroupSizeX = pEntryPoint->workgroup_size.x;
        shader->workGroupSizeY = pEntryPoint->workgroup_size.y;
        shader->workGroupSizeZ = pEntryPoint->workgroup_size.z;
    }*/

    spirv_cross::ShaderResources allResources = compiler->get_shader_resources();
    shader->reflectedResourceCount = 0;

    ReflectShaderResources( compiler, shader, allResources.uniform_buffers, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    ReflectShaderResources( compiler, shader, allResources.storage_buffers, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
    ReflectShaderResources( compiler, shader, allResources.separate_images, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
    ReflectShaderResources( compiler, shader, allResources.storage_images, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
    ReflectShaderResources( compiler, shader, allResources.separate_samplers, VK_DESCRIPTOR_TYPE_SAMPLER );

    return shader;
}

void RenderDevice::destroyShader( Shader* shader )
{
    vkDestroyShaderModule( renderContext->device, shader->shaderModule, nullptr );
    dk::core::free( memoryAllocator, shader );
}

void CreateShaderStageDescriptor( VkPipelineShaderStageCreateInfo& shaderStageInfos, const eShaderStage shaderStage, const Shader* shader )
{
    shaderStageInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfos.pNext = nullptr;
    shaderStageInfos.flags = 0u;
    shaderStageInfos.module = shader->shaderModule;

    switch ( shaderStage ) {
    case eShaderStage::SHADER_STAGE_VERTEX:
        shaderStageInfos.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStageInfos.pName = "EntryPointVS";
        break;
    case eShaderStage::SHADER_STAGE_COMPUTE:
        shaderStageInfos.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStageInfos.pName = "EntryPointCS";
        break;
    case eShaderStage::SHADER_STAGE_PIXEL:
        shaderStageInfos.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStageInfos.pName = "EntryPointPS";
        break;
    case eShaderStage::SHADER_STAGE_TESSELATION_CONTROL:
        shaderStageInfos.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        shaderStageInfos.pName = "EntryPointDS";
        break;
    case eShaderStage::SHADER_STAGE_TESSELATION_EVALUATION:
        shaderStageInfos.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        shaderStageInfos.pName = "EntryPointHS";
        break;
    default:
        shaderStageInfos.pName = "main";
        break;
    }

    shaderStageInfos.pSpecializationInfo = nullptr;
}

DUSK_INLINE VkPipelineInputAssemblyStateCreateInfo* FillInputAssemblyDesc( const PipelineStateDesc& description, VkPipelineInputAssemblyStateCreateInfo& inputAssembly )
{
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.pNext = nullptr;
    inputAssembly.flags = 0u;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY[description.PrimitiveTopology];
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    return &inputAssembly;
}

DUSK_INLINE VkPipelineRasterizationStateCreateInfo* FillRasterizationInfos( const RasterizerStateDesc& rasterizerDescription, VkPipelineRasterizationStateCreateInfo& rasterizerStateInfos )
{
    rasterizerStateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerStateInfos.pNext = nullptr;
    rasterizerStateInfos.flags = 0;
    rasterizerStateInfos.depthClampEnable = VK_FALSE; // static_cast< VkBool32 >( rasterizerDescription.depthBiasClamp != 0.0f );
    rasterizerStateInfos.rasterizerDiscardEnable = VK_FALSE;
    rasterizerStateInfos.polygonMode = VK_FM[rasterizerDescription.FillMode];
    rasterizerStateInfos.cullMode = VK_CM[rasterizerDescription.CullMode];
    rasterizerStateInfos.frontFace = ( rasterizerDescription.UseTriangleCCW ) ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rasterizerStateInfos.depthBiasEnable = static_cast< VkBool32 >( rasterizerDescription.DepthBias != 0.0f );
    rasterizerStateInfos.depthBiasConstantFactor = rasterizerDescription.DepthBias;
    rasterizerStateInfos.depthBiasClamp = rasterizerDescription.DepthBiasClamp;
    rasterizerStateInfos.depthBiasSlopeFactor = rasterizerDescription.SlopeScale;
    rasterizerStateInfos.lineWidth = 1.0f;

    return &rasterizerStateInfos;
}

DUSK_INLINE VkPipelineMultisampleStateCreateInfo* FillMultisampleStateInfos( const PipelineStateDesc& description, VkPipelineMultisampleStateCreateInfo& multisampleStateInfos )
{
    multisampleStateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateInfos.pNext = nullptr;
    multisampleStateInfos.flags = 0u;
    multisampleStateInfos.rasterizationSamples = GetVkSampleCount( description.samplerCount );
    multisampleStateInfos.sampleShadingEnable = VK_FALSE;
    multisampleStateInfos.minSampleShading = 1.0f;
    multisampleStateInfos.pSampleMask = nullptr;
    multisampleStateInfos.alphaToCoverageEnable = ( description.BlendState.EnableAlphaToCoverage ) ? VK_TRUE : VK_FALSE;
    multisampleStateInfos.alphaToOneEnable = VK_FALSE;

    return &multisampleStateInfos;
}

DUSK_INLINE VkPipelineDepthStencilStateCreateInfo* FillDepthStencilState( const DepthStencilStateDesc& depthStencilDescription, VkPipelineDepthStencilStateCreateInfo& depthStencilStateInfos )
{
    depthStencilStateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateInfos.pNext = nullptr;
    depthStencilStateInfos.flags = 0u;
    depthStencilStateInfos.depthTestEnable = static_cast< VkBool32 >( depthStencilDescription.EnableDepthTest );
    depthStencilStateInfos.depthWriteEnable = static_cast< VkBool32 >( depthStencilDescription.EnableDepthWrite );
    depthStencilStateInfos.depthCompareOp = COMPARISON_FUNCTION_LUT[depthStencilDescription.DepthComparisonFunc];
    depthStencilStateInfos.depthBoundsTestEnable = depthStencilDescription.EnableDepthBoundsTest;
    depthStencilStateInfos.stencilTestEnable = depthStencilDescription.EnableStencilTest;

    VkStencilOpState frontStencilState;
    frontStencilState.failOp = VK_STENCIL_OPERATION[depthStencilDescription.Front.FailOperation];
    frontStencilState.passOp = VK_STENCIL_OPERATION[depthStencilDescription.Front.PassOperation];
    frontStencilState.depthFailOp = VK_STENCIL_OPERATION[depthStencilDescription.Front.ZFailOperation];
    frontStencilState.compareOp = COMPARISON_FUNCTION_LUT[depthStencilDescription.Front.ComparisonFunction];
    frontStencilState.compareMask = depthStencilDescription.StencilReadMask;
    frontStencilState.writeMask = depthStencilDescription.StencilWriteMask;
    frontStencilState.reference = depthStencilDescription.StencilRefValue;
    depthStencilStateInfos.front = frontStencilState;

    VkStencilOpState backStencilState;
    backStencilState.failOp = VK_STENCIL_OPERATION[depthStencilDescription.Back.FailOperation];
    backStencilState.passOp = VK_STENCIL_OPERATION[depthStencilDescription.Back.PassOperation];
    backStencilState.depthFailOp = VK_STENCIL_OPERATION[depthStencilDescription.Back.ZFailOperation];
    backStencilState.compareOp = COMPARISON_FUNCTION_LUT[depthStencilDescription.Back.ComparisonFunction];
    backStencilState.compareMask = depthStencilDescription.StencilReadMask;
    backStencilState.writeMask = depthStencilDescription.StencilWriteMask;
    backStencilState.reference = depthStencilDescription.StencilRefValue;
    depthStencilStateInfos.back = backStencilState;

    depthStencilStateInfos.minDepthBounds = depthStencilDescription.DepthBoundsMin;
    depthStencilStateInfos.maxDepthBounds = depthStencilDescription.DepthBoundsMax;

    return &depthStencilStateInfos;
}

DUSK_INLINE VkPipelineColorBlendStateCreateInfo* FillBlendStateInfos( const BlendStateDesc& blendStateDescription, VkPipelineColorBlendStateCreateInfo& blendCreateInfos )
{
    // Describing color blending
    // Note: all paramaters except blendEnable and colorWriteMask are irrelevant here
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Note: all attachments must have the same values unless a device feature is enabled
    blendCreateInfos = {};
    blendCreateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendCreateInfos.logicOpEnable = VK_FALSE;
    blendCreateInfos.logicOp = VK_LOGIC_OP_COPY;
    blendCreateInfos.attachmentCount = 1;
    blendCreateInfos.pAttachments = &colorBlendAttachmentState;
    blendCreateInfos.blendConstants[0] = 0.0f;
    blendCreateInfos.blendConstants[1] = 0.0f;
    blendCreateInfos.blendConstants[2] = 0.0f;
    blendCreateInfos.blendConstants[3] = 0.0f;

 /*   blendCreateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendCreateInfos.pNext = nullptr;
    blendCreateInfos.flags = 0;

    blendCreateInfos.attachmentCount = 1;
    blendCreateInfos.logicOpEnable = VK_FALSE;
    blendCreateInfos.logicOp = VK_LOGIC_OP_COPY;

    blendCreateInfos.blendConstants[0] = 0.0f;
    blendCreateInfos.blendConstants[1] = 0.0f;
    blendCreateInfos.blendConstants[2] = 0.0f;
    blendCreateInfos.blendConstants[3] = 0.0f;

    VkPipelineColorBlendAttachmentState blendAttachments[8];
    for ( i32 i = 0; i < 8; i++ ) {
        VkPipelineColorBlendAttachmentState& blendAttachementState = blendAttachments[i];

        blendAttachementState.blendEnable = ( blendStateDescription.enableBlend ) ? VK_TRUE : VK_FALSE;

        // Reset write mask
        blendAttachementState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        if ( blendStateDescription.writeMask[0] )
            blendAttachementState.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
        else if ( blendStateDescription.writeMask[1] )
            blendAttachementState.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        else if ( blendStateDescription.writeMask[2] )
            blendAttachementState.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        else if ( blendStateDescription.writeMask[3] )
            blendAttachementState.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

        blendAttachementState.srcColorBlendFactor = VK_BLEND_SOURCE[blendStateDescription.blendConfColor.source];
        blendAttachementState.dstColorBlendFactor = VK_BLEND_SOURCE[blendStateDescription.blendConfColor.dest];
        blendAttachementState.colorBlendOp = VK_BLEND_OPERATION[blendStateDescription.blendConfColor.operation];

        blendAttachementState.srcAlphaBlendFactor = VK_BLEND_SOURCE[blendStateDescription.blendConfAlpha.source];
        blendAttachementState.dstAlphaBlendFactor = VK_BLEND_SOURCE[blendStateDescription.blendConfAlpha.dest];
        blendAttachementState.alphaBlendOp = VK_BLEND_OPERATION[blendStateDescription.blendConfAlpha.operation];
    }

    blendCreateInfos.pAttachments = blendAttachments;
*/
    return &blendCreateInfos;
}

void BuildStageDescriptorBinding(
        const Shader* shader,
        const VkShaderStageFlagBits stageFlags,
        VkDescriptorSetLayoutBinding descriptorSetBindings[8][96],
        u32& descriptorSetCount,
        u32* descriptorBindingCount,
        std::unordered_map<dkStringHash_t, PipelineState::ResourceBinding>& bindingSet,
        const VkSampler* staticSamplers = nullptr
)
{
    if ( shader == nullptr ) {
        return;
    }

    for ( u32 i = 0; i < shader->reflectedResourceCount; i++ ) {
        auto& reflectedRes = shader->resourcesReflectionInfos[i];

        u32& bindingCount = descriptorBindingCount[reflectedRes.descriptorSetIndex];

        descriptorSetCount = Max( descriptorSetCount, ( reflectedRes.descriptorSetIndex + 1 ) );
        bindingCount = Max( bindingCount, ( reflectedRes.bindingIndex + 1 ) );

        VkDescriptorSetLayoutBinding& descriptorSetLayoutBinding = descriptorSetBindings[reflectedRes.descriptorSetIndex][reflectedRes.bindingIndex];
        descriptorSetLayoutBinding.binding = reflectedRes.bindingIndex;
        descriptorSetLayoutBinding.descriptorType = reflectedRes.type;
        descriptorSetLayoutBinding.descriptorCount = 1u;
        descriptorSetLayoutBinding.stageFlags |= stageFlags;
        descriptorSetLayoutBinding.pImmutableSamplers = ( reflectedRes.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ) ? staticSamplers : nullptr;

        dkStringHash_t resourceHashcode = dk::core::CRC32( reflectedRes.name );

        PipelineState::ResourceBinding& resourceBinding = bindingSet[resourceHashcode];
        resourceBinding.bindingIndex = reflectedRes.bindingIndex;
        resourceBinding.descriptorSet = reflectedRes.descriptorSetIndex;
        resourceBinding.range = reflectedRes.rangeBinding.range;
        resourceBinding.offset = reflectedRes.rangeBinding.offset;
        resourceBinding.type = reflectedRes.type;
    }
}

void CreatePipelineLayout( VkDevice device, const PipelineStateDesc& description, PipelineState& pipelineState )
{
    VkDescriptorSetLayoutBinding descriptorSetBindings[8][96];
    memset( descriptorSetBindings, 0, sizeof( descriptorSetBindings ) );
    u32 descriptorSetCount = 0;
    u32 descriptorBindingCount[8] = { 0 };

    for ( i32 i = 0; i < description.StaticSamplers.StaticSamplerCount; i++ ) {
        const SamplerDesc& samplerDesc = description.StaticSamplers.StaticSamplersDesc[i];

        const bool useAnisotropicFiltering = ( samplerDesc.filter == eSamplerFilter::SAMPLER_FILTER_ANISOTROPIC_16
                                               || samplerDesc.filter == eSamplerFilter::SAMPLER_FILTER_ANISOTROPIC_8
                                               || samplerDesc.filter == eSamplerFilter::SAMPLER_FILTER_COMPARISON_ANISOTROPIC_8
                                               || samplerDesc.filter == eSamplerFilter::SAMPLER_FILTER_COMPARISON_ANISOTROPIC_16 );

        const bool useComparisonFunction = ( samplerDesc.comparisonFunction != eComparisonFunction::COMPARISON_FUNCTION_ALWAYS );

        // Build object descriptor
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_MAG_FILTER[samplerDesc.filter];
        samplerInfo.minFilter = VK_MIN_FILTER[samplerDesc.filter];
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS[samplerDesc.addressU];
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS[samplerDesc.addressV];
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS[samplerDesc.addressW];
        samplerInfo.anisotropyEnable = static_cast< VkBool32 >( useAnisotropicFiltering );
        samplerInfo.maxAnisotropy = ( samplerDesc.filter == eSamplerFilter::SAMPLER_FILTER_COMPARISON_ANISOTROPIC_8 ) ? 8.0f : 16.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = static_cast< VkBool32 >( useComparisonFunction );
        samplerInfo.compareOp = COMPARISON_FUNCTION_LUT[samplerDesc.comparisonFunction];
        samplerInfo.mipmapMode = VK_MIP_MAP_MODE[samplerDesc.filter];
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = static_cast< f32 >( samplerDesc.minLOD );
        samplerInfo.maxLod = static_cast< f32 >( samplerDesc.maxLOD );

        VkSampler samplerObject;
        VkResult operationResult = vkCreateSampler( device, &samplerInfo, nullptr, &samplerObject );
        DUSK_ASSERT( operationResult == VK_SUCCESS , "Sampler creation failed! (error code: 0x%x)", operationResult )
        pipelineState.immutableSamplers[i] = samplerObject;

        VkDescriptorSetLayoutBinding& descriptorSetLayoutBinding = descriptorSetBindings[0][i];
        descriptorSetLayoutBinding.binding = static_cast<uint32_t>( i );
        descriptorSetLayoutBinding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorSetLayoutBinding.descriptorCount = 1u;
        descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
        descriptorSetLayoutBinding.pImmutableSamplers = &samplerObject;
    }

    pipelineState.immutableSamplerCount = description.StaticSamplers.StaticSamplerCount;

    // Build DescriptorSet binding layout using Reflection infos
    if ( description.PipelineType == PipelineStateDesc::GRAPHICS ) {
        BuildStageDescriptorBinding( description.vertexShader, VK_SHADER_STAGE_VERTEX_BIT, descriptorSetBindings, descriptorSetCount, descriptorBindingCount, pipelineState.bindingSet, pipelineState.immutableSamplers );
        BuildStageDescriptorBinding( description.tesselationControlShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, descriptorSetBindings, descriptorSetCount, descriptorBindingCount, pipelineState.bindingSet, pipelineState.immutableSamplers );
        BuildStageDescriptorBinding( description.tesselationEvalShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, descriptorSetBindings, descriptorSetCount, descriptorBindingCount, pipelineState.bindingSet, pipelineState.immutableSamplers );
        BuildStageDescriptorBinding( description.pixelShader, VK_SHADER_STAGE_FRAGMENT_BIT, descriptorSetBindings, descriptorSetCount, descriptorBindingCount, pipelineState.bindingSet, pipelineState.immutableSamplers );
    } else if ( description.PipelineType == PipelineStateDesc::COMPUTE ) {
        BuildStageDescriptorBinding( description.computeShader, VK_SHADER_STAGE_COMPUTE_BIT, descriptorSetBindings, descriptorSetCount, descriptorBindingCount, pipelineState.bindingSet, pipelineState.immutableSamplers );
    }

    // Descriptor Set Layout
    for ( u32 i = 0; i < descriptorSetCount; i++ ) {
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo;
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutInfo.pNext = nullptr;
        descriptorSetLayoutInfo.flags = 0u;
        descriptorSetLayoutInfo.bindingCount = descriptorBindingCount[i];
        descriptorSetLayoutInfo.pBindings = descriptorSetBindings[i];

        vkCreateDescriptorSetLayout( device, &descriptorSetLayoutInfo, nullptr, &pipelineState.descriptorSetLayouts[i] );
    }

    // Allocate pipeline Layout for the current resource list
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext = nullptr;
    pipelineLayoutInfo.flags = 0u;
    pipelineLayoutInfo.setLayoutCount = descriptorSetCount;
    pipelineLayoutInfo.pSetLayouts = pipelineState.descriptorSetLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0u;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    pipelineState.descriptorSetCount = descriptorSetCount;

    VkResult opResult = vkCreatePipelineLayout( device, &pipelineLayoutInfo, nullptr, &pipelineState.layout );
    DUSK_RAISE_FATAL_ERROR( ( opResult == VK_SUCCESS ), "Pipeline Layout creation FAILED! (error code: %i)\n", opResult );
}

DUSK_INLINE VkRenderPass CreateRenderPass( VkDevice device, const PipelineStateDesc& description )
{
    VkAttachmentDescription attachments[FramebufferLayoutDesc::MAX_ATTACHMENT_COUNT];
    memset( attachments, 0, sizeof( attachments ) );

    VkAttachmentReference writeReference[8];
    memset( writeReference, 0, sizeof( writeReference ) );

    for ( i32 i = 0; i < FramebufferLayoutDesc::MAX_ATTACHMENT_COUNT; i++ ) {
        writeReference[i].attachment = VK_ATTACHMENT_UNUSED;
    }

    VkAttachmentReference depthStencilReference;
    depthStencilReference.attachment = VK_ATTACHMENT_UNUSED;

    const VkSampleCountFlagBits samplerCount = GetVkSampleCount( description.samplerCount );

    u32 attachmentCount = description.FramebufferLayout.getAttachmentCount();
    const u32 colorAttachmentCount = attachmentCount;

    u32 i = 0;
    for ( ; i < attachmentCount; i++ ) {
        auto& resource = description.FramebufferLayout.Attachments[i];

        VkAttachmentDescription& attachmentDesc = attachments[i];
        attachmentDesc.flags = 0u;
        attachmentDesc.format = VK_IMAGE_FORMAT[resource.viewFormat];
        attachmentDesc.samples = samplerCount;
        attachmentDesc.loadOp = ( resource.targetState == FramebufferLayoutDesc::DONT_CARE ) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        if ( resource.bindMode == FramebufferLayoutDesc::WRITE_SWAPCHAIN ) {
            attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        } else {
            attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        VkAttachmentReference& attachmentReference = writeReference[i];
        attachmentReference.attachment = i;
        attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if ( description.FramebufferLayout.depthStencilAttachment.bindMode != FramebufferLayoutDesc::UNUSED ) {
        auto& resource = description.FramebufferLayout.depthStencilAttachment;

        VkAttachmentDescription& attachmentDesc = attachments[i];
        attachmentDesc.flags = 0u;
        attachmentDesc.format = VK_IMAGE_FORMAT[resource.viewFormat];
        attachmentDesc.samples = samplerCount;
        attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDesc.stencilLoadOp = ( resource.targetState == FramebufferLayoutDesc::DONT_CARE ) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

        attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthStencilReference.attachment = i;
        depthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachmentCount++;
    }

    VkSubpassDescription subpassDesc;
    subpassDesc.flags = 0u;
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.pInputAttachments = nullptr;
    subpassDesc.inputAttachmentCount = 0;
    subpassDesc.pColorAttachments = writeReference;
    subpassDesc.colorAttachmentCount = colorAttachmentCount;
    subpassDesc.pDepthStencilAttachment = ( depthStencilReference.attachment = VK_ATTACHMENT_UNUSED ) ? nullptr : &depthStencilReference;
    subpassDesc.pResolveAttachments = nullptr;
    subpassDesc.preserveAttachmentCount = 0u;
    subpassDesc.pPreserveAttachments = nullptr;

    /*VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependency.srcAccessMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_ACCESS_MEMORY_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
*/
    VkRenderPassCreateInfo renderPassInfo;
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.flags = 0u;
    renderPassInfo.attachmentCount = attachmentCount;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1u;
    renderPassInfo.pSubpasses = &subpassDesc;
    renderPassInfo.dependencyCount = 0;
    renderPassInfo.pDependencies = nullptr;

    VkRenderPass renderPass;
    VkResult opResult = vkCreateRenderPass( device, &renderPassInfo, nullptr, &renderPass );
    DUSK_RAISE_FATAL_ERROR( ( opResult == VK_SUCCESS ), "Render Pass creation FAILED! (error code: %i)\n", opResult );

    return renderPass;
}

DUSK_INLINE PipelineState* CreateComputePso( VkDevice device, BaseAllocator* memoryAllocator, const PipelineStateDesc& description )
{
    PipelineState* pipelineState = dk::core::allocate<PipelineState>( memoryAllocator );
    CreatePipelineLayout( device, description, *pipelineState );

    pipelineState->stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Create Compute Shader Stage
    VkPipelineShaderStageCreateInfo shaderStageInfos;
    shaderStageInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfos.pNext = nullptr;
    shaderStageInfos.flags = 0u;
    shaderStageInfos.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfos.module = description.computeShader->shaderModule;
    shaderStageInfos.pName = "EntryPointCS";
    shaderStageInfos.pSpecializationInfo = nullptr;

    // Fill PSO description
    VkComputePipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0u;
    pipelineInfo.stage = shaderStageInfos;
    pipelineInfo.layout = pipelineState->layout;

    // TODO Might be nice to have this feature later...
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    // Create PSO
    VkResult opResult = vkCreateComputePipelines( device, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &pipelineState->pipelineObject );
    DUSK_RAISE_FATAL_ERROR( ( opResult == VK_SUCCESS ), "Compute Pipeline creation FAILED! (error code: %i)\n", opResult );

    return pipelineState;
}

DUSK_INLINE PipelineState* CreateGraphicsPso(VkDevice device, BaseAllocator* memoryAllocator, const PipelineStateDesc& description )
{
    PipelineState* pipelineState = dk::core::allocate<PipelineState>( memoryAllocator );

    CreatePipelineLayout( device, description, *pipelineState );
    pipelineState->renderPass = CreateRenderPass( device, description );
    pipelineState->fboLayout = description.FramebufferLayout;

    switch ( description.ColorClearValue ) {
    case PipelineStateDesc::CLEAR_COLOR_WHITE:
        for ( i32 i = 0; i < 4; i++ ) {
            pipelineState->defaultClearValue.color.float32[i] = 1.0f;
        }
        break;
    case PipelineStateDesc::CLEAR_COLOR_BLACK:
        for ( i32 i = 0; i < 4; i++ ) {
            pipelineState->defaultClearValue.color.float32[i] = 0.0f;
        }
        break;
    }

    pipelineState->defaultDepthClearValue.depthStencil.depth = description.depthClearValue;
    pipelineState->defaultDepthClearValue.depthStencil.stencil = description.stencilClearValue;

    VkGraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0u;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizerStateInfos;
    VkPipelineMultisampleStateCreateInfo multisampleStateInfos;
    VkPipelineDepthStencilStateCreateInfo depthStencilStateInfos;
    VkPipelineColorBlendStateCreateInfo blendCreateInfos;

    // Don't bake viewport dimensions (allow viewport resizing at runtime; more convenient)
    static constexpr VkDynamicState dynamicStates[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext = VK_NULL_HANDLE;
    dynamicState.flags = 0;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkVertexInputAttributeDescription vertexInputAttributeDesc[8];
    VkVertexInputBindingDescription  vertexInputBindingDesc[8];

    uint32_t i = 0;
    for ( ; ; i++ ) {
        if ( description.InputLayout.Entry[i].semanticName == nullptr ) {
            break;
        }

        vertexInputAttributeDesc[i].location = i;
        vertexInputAttributeDesc[i].binding = description.InputLayout.Entry[i].vertexBufferIndex;
        vertexInputAttributeDesc[i].format = VK_IMAGE_FORMAT[description.InputLayout.Entry[i].format];
        vertexInputAttributeDesc[i].offset = description.InputLayout.Entry[i].offsetInBytes;

        // TODO This is crappy since we assume each attribute will use its own buffer (which might not always be the case!)
        VkVertexInputBindingDescription& vboBindDesc = vertexInputBindingDesc[i];
        vboBindDesc.binding = i;
        vboBindDesc.stride = static_cast<uint32_t>( VK_VIEW_FORMAT_SIZE[description.InputLayout.Entry[i].format] );
        vboBindDesc.inputRate = ( description.InputLayout.Entry[i].instanceCount ) > 1u ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
    }

    VkPipelineVertexInputStateCreateInfo vertexInputStateDesc;
    vertexInputStateDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateDesc.pNext = nullptr;
    vertexInputStateDesc.flags = 0u;
    vertexInputStateDesc.vertexBindingDescriptionCount = i;
    vertexInputStateDesc.pVertexBindingDescriptions = vertexInputBindingDesc;
    vertexInputStateDesc.vertexAttributeDescriptionCount = i;
    vertexInputStateDesc.pVertexAttributeDescriptions = vertexInputAttributeDesc;

    pipelineInfo.pInputAssemblyState = FillInputAssemblyDesc( description, inputAssembly );
    pipelineInfo.pTessellationState = nullptr;
    pipelineInfo.pViewportState = nullptr;
    pipelineInfo.pRasterizationState = FillRasterizationInfos( description.RasterizerState, rasterizerStateInfos );
    pipelineInfo.pMultisampleState = FillMultisampleStateInfos( description, multisampleStateInfos );
    pipelineInfo.pDepthStencilState = FillDepthStencilState( description.DepthStencilState, depthStencilStateInfos );
    // Describing color blending
    // Note: all paramaters except blendEnable and colorWriteMask are irrelevant here
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Note: all attachments must have the same values unless a device feature is enabled
    /* blendCreateInfos = {};
    blendCreateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendCreateInfos.logicOpEnable = VK_FALSE;
    blendCreateInfos.logicOp = VK_LOGIC_OP_COPY;
    blendCreateInfos.attachmentCount = 1;
    blendCreateInfos.pAttachments = &colorBlendAttachmentState;
    blendCreateInfos.blendConstants[0] = 0.0f;
    blendCreateInfos.blendConstants[1] = 0.0f;
    blendCreateInfos.blendConstants[2] = 0.0f;
    blendCreateInfos.blendConstants[3] = 0.0f;
*/
    blendCreateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendCreateInfos.pNext = nullptr;
    blendCreateInfos.flags = 0;

    blendCreateInfos.attachmentCount = description.FramebufferLayout.getAttachmentCount();
    blendCreateInfos.logicOpEnable = VK_FALSE;
    blendCreateInfos.logicOp = VK_LOGIC_OP_COPY;

    blendCreateInfos.blendConstants[0] = 0.0f;
    blendCreateInfos.blendConstants[1] = 0.0f;
    blendCreateInfos.blendConstants[2] = 0.0f;
    blendCreateInfos.blendConstants[3] = 0.0f;

    const BlendStateDesc& blendStateDescription = description.BlendState;
    VkPipelineColorBlendAttachmentState blendAttachments[FramebufferLayoutDesc::MAX_ATTACHMENT_COUNT];
    for ( u32 i = 0; i < blendCreateInfos.attachmentCount; i++ ) {
        VkPipelineColorBlendAttachmentState& blendAttachementState = blendAttachments[i];

        blendAttachementState.blendEnable = ( blendStateDescription.EnableBlend ) ? VK_TRUE : VK_FALSE;

        // Reset write mask
        blendAttachementState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        if ( blendStateDescription.WriteR )
            blendAttachementState.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
        else if ( blendStateDescription.WriteG )
            blendAttachementState.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        else if ( blendStateDescription.WriteB )
            blendAttachementState.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        else if ( blendStateDescription.WriteA )
            blendAttachementState.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

        blendAttachementState.srcColorBlendFactor = VK_BLEND_SOURCE[blendStateDescription.BlendConfColor.Source];
        blendAttachementState.dstColorBlendFactor = VK_BLEND_SOURCE[blendStateDescription.BlendConfColor.Destination];
        blendAttachementState.colorBlendOp = VK_BLEND_OPERATION[blendStateDescription.BlendConfColor.Operation];

        blendAttachementState.srcAlphaBlendFactor = VK_BLEND_SOURCE[blendStateDescription.BlendConfAlpha.Source];
        blendAttachementState.dstAlphaBlendFactor = VK_BLEND_SOURCE[blendStateDescription.BlendConfAlpha.Destination];
        blendAttachementState.alphaBlendOp = VK_BLEND_OPERATION[blendStateDescription.BlendConfAlpha.Operation];
    }

    blendCreateInfos.pAttachments = blendAttachments;
    pipelineInfo.pColorBlendState  = &blendCreateInfos;

    pipelineInfo.pDynamicState = nullptr; //dynamicState;
    pipelineInfo.pVertexInputState = &vertexInputStateDesc;

    VkPipelineViewportStateCreateInfo vp;
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.pNext = VK_NULL_HANDLE;
    vp.flags = 0;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkViewport vkViewport;
    vkViewport.x = 0.0f;
    vkViewport.y = 0.0f;
    vkViewport.width = 1280.0f;
    vkViewport.height = 720.0f;
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;

    VkRect2D vkScissor;
    vkScissor.offset.x = 0;
    vkScissor.offset.y = 0;
    vkScissor.extent.width = 1280;
    vkScissor.extent.height = 720;

    vp.pViewports = &vkViewport;
    vp.pScissors = &vkScissor;

    pipelineInfo.pViewportState = &vp;


    uint32_t pipelineStageCount = 0u;
    VkPipelineShaderStageCreateInfo pipelineStages[SHADER_STAGE_COUNT];
    pipelineState->stageFlags = 0;

    if ( description.vertexShader != nullptr ) {
        CreateShaderStageDescriptor( pipelineStages[pipelineStageCount++], eShaderStage::SHADER_STAGE_VERTEX, description.vertexShader );
        pipelineState->stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    }

    if ( description.tesselationEvalShader != nullptr ) {
        CreateShaderStageDescriptor( pipelineStages[pipelineStageCount++], eShaderStage::SHADER_STAGE_TESSELATION_EVALUATION, description.tesselationEvalShader );
        pipelineState->stageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }

    if ( description.tesselationControlShader != nullptr ) {
        CreateShaderStageDescriptor( pipelineStages[pipelineStageCount++], eShaderStage::SHADER_STAGE_TESSELATION_CONTROL, description.tesselationControlShader );
        pipelineState->stageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    }

    if ( description.pixelShader != nullptr ) {
        CreateShaderStageDescriptor( pipelineStages[pipelineStageCount++], eShaderStage::SHADER_STAGE_PIXEL, description.pixelShader );
        pipelineState->stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    pipelineInfo.pStages = pipelineStages;
    pipelineInfo.stageCount = pipelineStageCount;
    pipelineInfo.layout = pipelineState->layout;

    pipelineInfo.renderPass = pipelineState->renderPass;

    // TODO Might be nice to have this feature later...
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkResult opResult = vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &pipelineState->pipelineObject );
    DUSK_RAISE_FATAL_ERROR( ( opResult == VK_SUCCESS ), "Compute Pipeline creation FAILED! (error code: %i)\n", opResult );

    return pipelineState;
}

PipelineState* RenderDevice::createPipelineState( const PipelineStateDesc& description )
{
    if ( description.PipelineType == PipelineStateDesc::COMPUTE ) {
        return CreateComputePso( renderContext->device, memoryAllocator, description );
    } else {
        return CreateGraphicsPso( renderContext->device, memoryAllocator, description );
    }

    DUSK_RAISE_FATAL_ERROR( false, "Unknown PSO Type! (API has been upgraded and Vulkan is not up to date?)" );

    // Should never be reached
    return nullptr;
}

void CommandList::prepareAndBindResourceList()
{
    NativeCommandList* nativeCommandList = getNativeCommandList();
    PipelineState* pipelineState = nativeCommandList->BindedPipelineState;

    for ( i32 i = 0; i < pipelineState->immutableSamplerCount; i++ ) {
        VkWriteDescriptorSet& writeDescriptor = nativeCommandList->writeDescriptorSets[nativeCommandList->writeDescriptorSetsCount++];
        writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptor.pNext = nullptr;
        writeDescriptor.dstSet = nativeCommandList->activeDescriptorSets[0];
        writeDescriptor.dstBinding = static_cast<uint32_t>( i );
        writeDescriptor.dstArrayElement = 0u;
        writeDescriptor.descriptorCount = 1u;
        writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;

        VkDescriptorImageInfo& imageInfos = nativeCommandList->imageInfos[nativeCommandList->imageInfosCount++];
        imageInfos.sampler = pipelineState->immutableSamplers[i];

        writeDescriptor.pImageInfo = &imageInfos;
    }

    vkUpdateDescriptorSets( nativeCommandList->device, nativeCommandList->writeDescriptorSetsCount, nativeCommandList->writeDescriptorSets, 0u, nullptr );

    const bool isGfxCmdList = ( commandListType == CommandList::Type::GRAPHICS );
    VkPipelineBindPoint bindPoint = ( isGfxCmdList ) ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;

    // Bind DescriptorSets prior to PSO binding
    vkCmdBindDescriptorSets(
        nativeCommandList->cmdList,
        bindPoint,
        pipelineState->layout,
        0u,
        pipelineState->descriptorSetCount,
        nativeCommandList->activeDescriptorSets,
        0u,
        nullptr
    );

    // Bind Pipeline State
    vkCmdBindPipeline( nativeCommandList->cmdList, bindPoint, pipelineState->pipelineObject );
}

void RenderDevice::destroyPipelineState( PipelineState* pipelineState )
{
    vkDestroyPipeline( renderContext->device, pipelineState->pipelineObject, nullptr );
    vkDestroyPipelineLayout( renderContext->device, pipelineState->layout, nullptr );
    vkDestroyRenderPass( renderContext->device, pipelineState->renderPass, nullptr );

    dk::core::free( memoryAllocator, pipelineState );
}

void RenderDevice::getPipelineStateCache( PipelineState* pipelineState, void** binaryData, size_t& binaryDataSize )
{
    VkPipelineCacheCreateInfo cacheCreateInfos;
    cacheCreateInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheCreateInfos.flags = 0;
    cacheCreateInfos.pNext = VK_NULL_HANDLE;
    cacheCreateInfos.pInitialData = nullptr;
    cacheCreateInfos.initialDataSize = 0;

    // Create the cache first
    VkResult cacheCreationResult = vkCreatePipelineCache( renderContext->device, &cacheCreateInfos, nullptr, &pipelineState->pipelineCache );
    DUSK_ASSERT( ( cacheCreationResult == VK_SUCCESS ), "vkCreatePipelineCache FAILED! (error code: %i)\n", cacheCreationResult )

    // Do a first dummy call to retrieve the size of the pso cache blob
    VkResult opResult = vkGetPipelineCacheData( renderContext->device, pipelineState->pipelineCache, &binaryDataSize, nullptr );
    DUSK_ASSERT( ( opResult == VK_SUCCESS ), "vkGetPipelineCacheData FAILED! (error code: %i)\n", opResult )

    // Allocate the blob and retrieve the binary data
    *binaryData = pipelineStateCacheAllocator->allocate( binaryDataSize );
    vkGetPipelineCacheData( renderContext->device, pipelineState->pipelineCache, &binaryDataSize, *binaryData );
}

void RenderDevice::destroyPipelineStateCache( PipelineState* pipelineState )
{
    vkDestroyPipelineCache( renderContext->device, pipelineState->pipelineCache, nullptr );

    // Reset the allocator offset (might be problematic if this ever get multithreaded, but for now it's ok)
    pipelineStateCacheAllocator->clear();
}

void CommandList::begin()
{
    VkCommandBufferBeginInfo cmdBufferInfos = {};
    cmdBufferInfos.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferInfos.pNext = nullptr;
    cmdBufferInfos.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    cmdBufferInfos.pInheritanceInfo = nullptr;

    vkBeginCommandBuffer( nativeCommandList->cmdList, &cmdBufferInfos );

    nativeCommandList->isInRenderPass = false;
    nativeCommandList->waitForSwapchainRetrival = false;
    nativeCommandList->waitForPreviousCmdList = false;

    nativeCommandList->activeViewport = Viewport();
    nativeCommandList->bufferInfosCount = 0;
    nativeCommandList->imageInfosCount = 0;
    nativeCommandList->writeDescriptorSetsCount = 0;
    nativeCommandList->bufferInfosCount = 0;
}

void CommandList::bindPipelineState( PipelineState* pipelineState )
{
    // NOTE Do not bind the PSO nor the DescriptorSet yet, since we need to do the binding/update of the sets
    nativeCommandList->BindedPipelineState = pipelineState;

    if ( pipelineState == nullptr ) {
        return;
    }

    VkDescriptorSetAllocateInfo allocInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = nativeCommandList->descriptorPool;
    allocInfo.descriptorSetCount = pipelineState->descriptorSetCount;
    allocInfo.pSetLayouts = pipelineState->descriptorSetLayouts;

    // Allocate Descriptor sets from the pool and update it immediately.
    vkAllocateDescriptorSets( nativeCommandList->device, &allocInfo, nativeCommandList->activeDescriptorSets );
}

void CommandList::bindConstantBuffer( const dkStringHash_t hashcode, Buffer* buffer )
{
    std::unordered_map<dkStringHash_t, PipelineState::ResourceBinding>& bindingSet = nativeCommandList->BindedPipelineState->bindingSet;

    auto it = bindingSet.find( hashcode );
    DUSK_DEV_ASSERT( ( it != bindingSet.end( ) ), "Unknown resource hashcode! (shader source might have been updated?)" )

    PipelineState::ResourceBinding& binding = it->second;

    VkDescriptorBufferInfo& bufferInfos = nativeCommandList->bufferInfos[nativeCommandList->bufferInfosCount++];
    bufferInfos.buffer = buffer->resource[resourceFrameIndex];
    bufferInfos.range = VK_WHOLE_SIZE; // it->second.range;
    bufferInfos.offset = 0; // it->second.offset;

    VkWriteDescriptorSet& writeDescriptor = nativeCommandList->writeDescriptorSets[nativeCommandList->writeDescriptorSetsCount++];
    writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptor.pNext = nullptr;
    writeDescriptor.dstSet = nativeCommandList->activeDescriptorSets[binding.descriptorSet];
    writeDescriptor.dstBinding = binding.bindingIndex;
    writeDescriptor.dstArrayElement = 0u;
    writeDescriptor.descriptorCount = 1u;
    writeDescriptor.descriptorType = binding.type;
    writeDescriptor.pBufferInfo = &bufferInfos;
}

void CommandList::bindImage( const dkStringHash_t hashcode, Image* image, const ImageViewDesc viewDescription )
{
    std::unordered_map<dkStringHash_t, PipelineState::ResourceBinding>& bindingSet = nativeCommandList->BindedPipelineState->bindingSet;

    auto it = bindingSet.find( hashcode );
    DUSK_DEV_ASSERT( ( it != bindingSet.end( ) ), "Unknown resource hashcode! (shader source might have been updated?)" )

    PipelineState::ResourceBinding& binding = it->second;

    eViewFormat viewFormat = viewDescription.ViewFormat;

    if ( image->renderTargetView[viewFormat][resourceFrameIndex] == VK_NULL_HANDLE ) {
        image->renderTargetView[viewFormat][resourceFrameIndex] = CreateImageView(
                    nativeCommandList->device,
                    image->resource[resourceFrameIndex],
                    image->viewType,
                    image->defaultFormat,
                    image->aspectFlag,
                    0,
                    image->arraySize,
                    0,
                    image->mipCount
        );
    }

    VkDescriptorImageInfo& imageInfos = nativeCommandList->imageInfos[nativeCommandList->imageInfosCount++];
    imageInfos.imageView = image->renderTargetView[viewFormat][resourceFrameIndex];
    imageInfos.imageLayout = image->currentLayout[resourceFrameIndex];
    imageInfos.sampler = nativeCommandList->BindedPipelineState->immutableSamplers[0];

    VkWriteDescriptorSet& writeDescriptor = nativeCommandList->writeDescriptorSets[nativeCommandList->writeDescriptorSetsCount++];
    writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptor.pNext = nullptr;
    writeDescriptor.dstSet = nativeCommandList->activeDescriptorSets[binding.descriptorSet];
    writeDescriptor.dstBinding = binding.bindingIndex;
    writeDescriptor.dstArrayElement = 0u;
    writeDescriptor.descriptorCount = 1u;
    writeDescriptor.descriptorType = binding.type;
    writeDescriptor.pImageInfo = &imageInfos;
}

void CommandList::bindBuffer( const dkStringHash_t hashcode, Buffer* buffer,  const eViewFormat viewFormat )
{
    std::unordered_map<dkStringHash_t, PipelineState::ResourceBinding>& bindingSet = nativeCommandList->BindedPipelineState->bindingSet;

    auto it = bindingSet.find( hashcode );
    DUSK_DEV_ASSERT( ( it != bindingSet.end( ) ), "Unknown resource hashcode! (shader source might have been updated?)" )

    PipelineState::ResourceBinding& binding = it->second;

    VkDescriptorBufferInfo& bufferInfos = nativeCommandList->bufferInfos[nativeCommandList->bufferInfosCount++];
    bufferInfos.buffer = buffer->resource[resourceFrameIndex];
    bufferInfos.range = VK_WHOLE_SIZE; //it->second.range;
    bufferInfos.offset = 0; // it->second.offset;

    VkWriteDescriptorSet& writeDescriptor = nativeCommandList->writeDescriptorSets[nativeCommandList->writeDescriptorSetsCount++];
    writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptor.pNext = nullptr;
    writeDescriptor.dstSet = nativeCommandList->activeDescriptorSets[binding.descriptorSet];
    writeDescriptor.dstBinding = binding.bindingIndex;
    writeDescriptor.dstArrayElement = 0u;
    writeDescriptor.descriptorCount = 1u;
    writeDescriptor.descriptorType = binding.type;
    writeDescriptor.pBufferInfo = &bufferInfos;
}

void CommandList::bindSampler( const dkStringHash_t hashcode, Sampler* sampler )
{

}
#endif
