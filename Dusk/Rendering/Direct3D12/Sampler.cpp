/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D12
#include <Rendering/RenderDevice.h>

#include "RenderDevice.h"
#include "Sampler.h"
#include "ComparisonFunctions.h"

Sampler* RenderDevice::createSampler( const SamplerDesc& description )
{
    Sampler* sampler = dk::core::allocate<Sampler>( memoryAllocator );

    D3D12_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter = FILTER_LUT[description.filter];
    samplerDesc.AddressU = TEXTURE_ADDRESS_LUT[description.addressU];
    samplerDesc.AddressV = TEXTURE_ADDRESS_LUT[description.addressV];
    samplerDesc.AddressW = TEXTURE_ADDRESS_LUT[description.addressW];
    samplerDesc.MipLODBias = 0.0F;
    if ( description.filter == SAMPLER_FILTER_ANISOTROPIC_8
      || description.filter == SAMPLER_FILTER_COMPARISON_ANISOTROPIC_8 ) {
        samplerDesc.MaxAnisotropy = 8;
    } else if ( description.filter == SAMPLER_FILTER_ANISOTROPIC_16
             || description.filter == SAMPLER_FILTER_COMPARISON_ANISOTROPIC_16 ) {
        samplerDesc.MaxAnisotropy = 16;
    } else {
        samplerDesc.MaxAnisotropy = 0;
    }

    samplerDesc.ComparisonFunc = COMPARISON_FUNCTION_LUT[description.comparisonFunction];

    for ( i32 i = 0; i < 4; i++ ) {
        samplerDesc.BorderColor[i] = description.borderColor[i];
    }

    samplerDesc.MinLOD = static_cast<FLOAT>( description.minLOD );
    samplerDesc.MaxLOD = static_cast<FLOAT>( description.maxLOD );

    // Allocate descriptor from the sampler descriptor heap
    // The heap is static since we assume samplers are immutable and won't be modified at runtime
    D3D12_CPU_DESCRIPTOR_HANDLE heapHandle = renderContext->samplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    sampler->descriptor = heapHandle;
    heapHandle.ptr += renderContext->device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );

    renderContext->device->CreateSampler( &samplerDesc, sampler->descriptor );

    return sampler;
}

void RenderDevice::destroySampler( Sampler* sampler )
{
    dk::core::free( memoryAllocator, sampler );
}
#endif
