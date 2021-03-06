/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D11
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "Buffer.h"
#include "RenderDevice.h"
#include "CommandList.h"
#include "ResourceAllocationHelpers.h"

#include <Core/StringHelpers.h>

#include <d3d11.h>

Buffer* RenderDevice::createBuffer( const BufferDesc& description, const void* initialData )
{
    ID3D11Device* device = renderContext->PhysicalDevice;

    Buffer* buffer = dk::core::allocate<Buffer>( memoryAllocator );
    buffer->Stride = description.StrideInBytes; // Stride might be used for UAV/SRV creation (deferred until first usage)
    buffer->BindFlags = description.BindFlags;
    buffer->UsageFlags = description.Usage;

    D3D11_SUBRESOURCE_DATA subresourceDataDesc;
    subresourceDataDesc.pSysMem = initialData;
    subresourceDataDesc.SysMemPitch = 0;
    subresourceDataDesc.SysMemSlicePitch = 0;

    const bool isStructuredBuffer = ( description.BindFlags & RESOURCE_BIND_STRUCTURED_BUFFER
                                   || description.BindFlags & RESOURCE_BIND_APPEND_STRUCTURED_BUFFER );

    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.ByteWidth = static_cast< UINT >( description.SizeInBytes );
    bufferDesc.Usage = GetNativeUsage( description.Usage );
    bufferDesc.BindFlags = GetNativeBindFlags( description.BindFlags );
    bufferDesc.CPUAccessFlags = GetCPUUsageFlags( description.Usage );
    bufferDesc.MiscFlags = GetMiscFlags( description.BindFlags );
    bufferDesc.StructureByteStride = static_cast< UINT >( description.StrideInBytes );

    HRESULT operationResult = device->CreateBuffer( &bufferDesc, ( initialData != nullptr ) ? &subresourceDataDesc : nullptr, &buffer->BufferObject );
    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( operationResult ), "Buffer creation FAILED! (error code: 0x%x)", operationResult );
    
    // We need to copy the default view descriptor since the input descriptor is immutable (we could use const_cast crap tho)
    BufferViewDesc defaultViewDesc = description.DefaultView;

    const u16 elementCount = defaultViewDesc.NumElements;
    defaultViewDesc.NumElements = ( elementCount == 0 ) ? ( description.SizeInBytes / Max( 1u, description.StrideInBytes ) ) : elementCount;

    // Standard says that the view format must be DXGI_FORMAT_R32_TYPELESS.
    if ( description.BindFlags & RESOURCE_BIND_RAW ) {
        defaultViewDesc.ViewFormat = VIEW_FORMAT_R32_TYPELESS;
    }

    if ( description.BindFlags & RESOURCE_BIND_SHADER_RESOURCE ) {
        // Create default SRV
        buffer->DefaultShaderResourceView = CreateBufferShaderResourceView( device, *buffer, defaultViewDesc );
    }

    if ( description.BindFlags & RESOURCE_BIND_UNORDERED_ACCESS_VIEW ) {
        // Create default UAV
        buffer->DefaultUnorderedAccessView = CreateBufferUnorderedAccessView( device, *buffer, defaultViewDesc );
    }

    return buffer;
}

void RenderDevice::destroyBuffer( Buffer* buffer )
{
#define RELEASE_IF_ALLOCATED( obj ) if ( obj != nullptr ) { obj->Release(); }
    RELEASE_IF_ALLOCATED( buffer->BufferObject );
    RELEASE_IF_ALLOCATED( buffer->DefaultShaderResourceView );
    RELEASE_IF_ALLOCATED( buffer->DefaultUnorderedAccessView );
#undef RELEASE_IF_ALLOCATED

    dk::core::free( memoryAllocator, buffer );
}

void RenderDevice::setDebugMarker( Buffer& buffer, const dkChar_t* objectName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    buffer.BufferObject->SetPrivateData( WKPDID_D3DDebugObjectName, static_cast< UINT >( wcslen( objectName ) ), WideStringToString( objectName ).c_str() );
#endif
}

void CommandList::copyBuffer( Buffer* sourceBuffer, Buffer* destBuffer )
{
	CommandPacket::CopyResource* commandPacket = dk::core::allocate<CommandPacket::CopyResource>( nativeCommandList->CommandPacketAllocator );
	commandPacket->Identifier = CPI_COPY_RESOURCE;
	commandPacket->SourceResource = sourceBuffer->BufferObject;
	commandPacket->DestResource = destBuffer->BufferObject;

	nativeCommandList->Commands.push( reinterpret_cast< u32* >( commandPacket ) );
}

template<bool HasCustomOffsets>
DUSK_INLINE void BuildBindVertexBufferPacket( CommandPacket::BindVertexBuffer* commandPacket, const Buffer** buffers, const u32* offsets, const u32 bufferCount, const u32 startBindIndex )
{
    commandPacket->Identifier = CPI_BIND_VERTEX_BUFFER;
    commandPacket->BufferCount = bufferCount;
    commandPacket->StartBindIndex = startBindIndex;

    for ( u32 i = 0; i < bufferCount; i++ ) {
        commandPacket->Buffers[i] = buffers[i]->BufferObject;
        commandPacket->Strides[i] = buffers[i]->Stride;
        commandPacket->Offsets[i] = ( HasCustomOffsets ) ? offsets[i] : 0u;
    }
}

void CommandList::bindVertexBuffer( const Buffer** buffers, const u32* offsets, const u32 bufferCount, const u32 startBindIndex )
{
    CommandPacket::BindVertexBuffer* commandPacket = dk::core::allocate<CommandPacket::BindVertexBuffer>( nativeCommandList->CommandPacketAllocator );
    memset( commandPacket, 0, sizeof( CommandPacket::BindVertexBuffer ) );

    if ( offsets == nullptr ) {
        BuildBindVertexBufferPacket<false>( commandPacket, buffers, nullptr, bufferCount, startBindIndex );
    } else {
        BuildBindVertexBufferPacket<true>( commandPacket, buffers, offsets, bufferCount, startBindIndex );
    }

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::bindIndiceBuffer( const Buffer* buffer, const bool use32bitsIndices )
{
    CommandPacket::BindIndiceBuffer* commandPacket = dk::core::allocate<CommandPacket::BindIndiceBuffer>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_BIND_INDICE_BUFFER;
    commandPacket->ViewFormat = ( use32bitsIndices ) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    commandPacket->Offset = 0u;
    
    // Yep it sucks but this is required since we might have to update this buffer registers during the deferred execution
    // of the command (this is quite rare but it happens during the gpu driven shadow rendering).
    commandPacket->Buffer = const_cast<Buffer*>( buffer );

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::updateBuffer( Buffer& buffer, const void* data, const size_t dataSize )
{
    DUSK_DEV_ASSERT( buffer.BufferObject, "Buffer IS NULL!" );

    CommandPacket::UpdateBuffer* commandPacket = dk::core::allocate<CommandPacket::UpdateBuffer>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_UPDATE_BUFFER;
    commandPacket->BufferObject = buffer.BufferObject;
    commandPacket->DataSize = dataSize;
    commandPacket->Data = dk::core::allocateArray<u8>( nativeCommandList->CommandPacketAllocator, dataSize );
    memcpy( commandPacket->Data, data, dataSize );

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void* CommandList::mapBuffer( Buffer& buffer, const u32 startOffsetInBytes, const u32 sizeInBytes )
{
    D3D11_MAPPED_SUBRESOURCE mappedSubresource;

    const bool IsStagingBuffer = ( buffer.UsageFlags == RESOURCE_USAGE_STAGING );
    
    const D3D11_MAP MapType = ( IsStagingBuffer ) ? D3D11_MAP_READ : D3D11_MAP_WRITE_DISCARD;
    const UINT MapFlags = ( IsStagingBuffer ) ? D3D11_MAP_FLAG_DO_NOT_WAIT : 0;

    HRESULT operationResult = nativeCommandList->ImmediateContext->Map( buffer.BufferObject, 0, MapType, MapFlags, &mappedSubresource );
    DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Failed to map buffer! (error code: 0x%x)", operationResult );

    return mappedSubresource.pData;
}

void CommandList::unmapBuffer( Buffer& buffer )
{
    nativeCommandList->ImmediateContext->Unmap( buffer.BufferObject, 0 );
}

void CommandList::transitionBuffer( Buffer& buffer, const eResourceState state )
{

}
#endif
