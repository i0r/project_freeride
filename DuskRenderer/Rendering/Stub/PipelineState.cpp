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

void RenderDevice::prepareAndBindResourceList( CommandList& commandList, const PipelineState& pipelineState, const ResourceList& resourceList )
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

void CommandList::begin( PipelineState* pipelineState )
{

}
#endif
