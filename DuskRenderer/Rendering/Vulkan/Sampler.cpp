/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_VULKAN
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "RenderDevice.h"
#include "Sampler.h"
#include "ComparisonFunctions.h"

Sampler* RenderDevice::createSampler( const SamplerDesc& description )
{
    const bool useAnisotropicFiltering = ( description.filter == eSamplerFilter::SAMPLER_FILTER_ANISOTROPIC_16
                                           || description.filter == eSamplerFilter::SAMPLER_FILTER_ANISOTROPIC_8
                                           || description.filter == eSamplerFilter::SAMPLER_FILTER_COMPARISON_ANISOTROPIC_8
                                           || description.filter == eSamplerFilter::SAMPLER_FILTER_COMPARISON_ANISOTROPIC_16 );

    const bool useComparisonFunction = ( description.comparisonFunction != eComparisonFunction::COMPARISON_FUNCTION_ALWAYS );

    // Build object descriptor
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_MAG_FILTER[description.filter];
    samplerInfo.minFilter = VK_MIN_FILTER[description.filter];
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS[description.addressU];
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS[description.addressV];
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS[description.addressW];
    samplerInfo.anisotropyEnable = static_cast< VkBool32 >( useAnisotropicFiltering );
    samplerInfo.maxAnisotropy = ( description.filter == eSamplerFilter::SAMPLER_FILTER_COMPARISON_ANISOTROPIC_8 ) ? 8.0f : 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = static_cast< VkBool32 >( useComparisonFunction );
    samplerInfo.compareOp = COMPARISON_FUNCTION_LUT[description.comparisonFunction];
    samplerInfo.mipmapMode = VK_MIP_MAP_MODE[description.filter];
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = static_cast< f32 >( description.minLOD );
    samplerInfo.maxLod = static_cast< f32 >( description.maxLOD );

    VkSampler samplerObject;

    VkResult operationResult = vkCreateSampler( renderContext->device, &samplerInfo, nullptr, &samplerObject );
    DUSK_ASSERT( operationResult == VK_SUCCESS , "Sampler creation failed! (error code: 0x%x)", operationResult )

    Sampler* sampler = dk::core::allocate<Sampler>( memoryAllocator );
    sampler->samplerState = samplerObject;

    return sampler;
}

void RenderDevice::destroySampler( Sampler* sampler )
{
    vkDestroySampler( renderContext->device, sampler->samplerState, nullptr );

    dk::core::free( memoryAllocator, sampler );
}
#endif
