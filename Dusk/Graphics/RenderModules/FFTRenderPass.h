/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

using ResHandle_t = uint32_t;

// Output of a FFT renderpass (inverse, convolution, etc.).
struct FFTPassOutput 
{
    ResHandle_t RealPart;
    ResHandle_t ImaginaryPart;
};

FFTPassOutput AddFFTComputePass( FrameGraph& frameGraph, ResHandle_t input, f32 inputTargetWidth, f32 inputTargetHeight );
ResHandle_t AddInverseFFTComputePass( FrameGraph& frameGraph, FFTPassOutput& inputInFrequencyDomain, f32 outputTargetWidth, f32 outputTarget );

// Dimension (in pixels) for the FFT image.
constexpr i32 FFT_TEXTURE_DIMENSION = 512;
