/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D11
#include "Sampler.h"
#include "ComparisonFunctions.h"

#include "RenderDevice.h"

Sampler* RenderDevice::createSampler( const SamplerDesc& description )
{
    D3D11_SAMPLER_DESC samplerDesc = {
        D3D11_SAMPLER_FILTER[description.filter],
        D3D11_SAMPLER_ADDRESS[description.addressU],
        D3D11_SAMPLER_ADDRESS[description.addressV],
        D3D11_SAMPLER_ADDRESS[description.addressW]
    };

    if ( description.filter == SAMPLER_FILTER_ANISOTROPIC_8 )
        samplerDesc.MaxAnisotropy = 8;
    else if ( description.filter == SAMPLER_FILTER_ANISOTROPIC_16 )
        samplerDesc.MaxAnisotropy = 16;

    samplerDesc.MinLOD = static_cast< f32 >( description.minLOD );
    samplerDesc.MaxLOD = static_cast< f32 >( description.maxLOD );
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_FUNCTION[description.comparisonFunction];

    for ( i32 i = 0; i < 4; i++ ) {
        samplerDesc.BorderColor[i] = description.borderColor[i];
    }

    ID3D11Device* nativeDevice = renderContext->PhysicalDevice;

    Sampler* sampler = dk::core::allocate<Sampler>( memoryAllocator );
    nativeDevice->CreateSamplerState( &samplerDesc, &sampler->samplerState );

    return sampler;
}

void RenderDevice::destroySampler( Sampler* sampler )
{
    sampler->samplerState->Release();

    dk::core::free( memoryAllocator, sampler );
}
#endif
