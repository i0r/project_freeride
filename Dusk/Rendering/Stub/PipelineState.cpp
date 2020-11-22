/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_STUB
Shader* RenderDevice::createShader( const eShaderStage stage, const void* bytecode, const size_t bytecodeSize )
{
    return nullptr;
}

void RenderDevice::destroyShader( Shader* shader )
{

}

PipelineState* RenderDevice::createPipelineState( const PipelineStateDesc& description )
{
    return nullptr;
}

void CommandList::prepareAndBindResourceList()
{

}

void RenderDevice::destroyPipelineState( PipelineState* pipelineState )
{

}

void RenderDevice::getPipelineStateCache( PipelineState* pipelineState, void** binaryData, size_t& binaryDataSize )
{

}

void RenderDevice::destroyPipelineStateCache( PipelineState* pipelineState )
{

}

void CommandList::bindPipelineState( PipelineState* pipelineState )
{

}

void CommandList::begin()
{

}

void CommandList::bindConstantBuffer( const dkStringHash_t hashcode, Buffer* buffer )
{

}

void CommandList::bindImage( const dkStringHash_t hashcode, Image* image, const ImageViewDesc viewDescription )
{

}

void CommandList::bindBuffer( const dkStringHash_t hashcode, Buffer* buffer, const eViewFormat viewFormat )
{

}

void CommandList::bindSampler( const dkStringHash_t hashcode, Sampler* sampler )
{

}
#endif
