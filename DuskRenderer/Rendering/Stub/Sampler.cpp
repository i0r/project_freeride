/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_STUB
Sampler* RenderDevice::createSampler( const SamplerDesc& description )
{
    DUSK_UNUSED_VARIABLE( description );
    return nullptr;
}

void RenderDevice::destroySampler( Sampler* sampler )
{
    DUSK_UNUSED_VARIABLE( sampler );
}
#endif
