/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D12
struct ID3D12GraphicsCommandList;
struct ID3D12CommandSignature;
struct ID3D12CommandAllocator;
struct PipelineState;
struct RenderContext;

struct NativeCommandList
{
                                NativeCommandList();
                                ~NativeCommandList();

    ID3D12GraphicsCommandList*  graphicsCmdList;
    ID3D12CommandSignature*     currentCommandSignature; // Set by the current pipeline state
    ID3D12CommandAllocator**    allocator;
    PipelineState*              BindedPipelineState;
    RenderContext*              renderContext; // Yeah it sucks and definitely need some refactoring
};
#endif
