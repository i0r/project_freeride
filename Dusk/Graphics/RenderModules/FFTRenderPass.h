/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;

#include "Graphics/FrameGraph.h"

// Output of a FFT renderpass (inverse, convolution, etc.).
struct FFTPassOutput 
{
    FGHandle RealPart;
    FGHandle ImaginaryPart;

    FFTPassOutput()
        : RealPart( FGHandle::Invalid )
        , ImaginaryPart( FGHandle::Invalid )
    {

    }
};

FFTPassOutput AddFFTComputePass( FrameGraph& frameGraph, FGHandle input, f32 inputTargetWidth, f32 inputTargetHeight );
FGHandle AddInverseFFTComputePass( FrameGraph& frameGraph, FFTPassOutput& inputInFrequencyDomain, f32 outputTargetWidth, f32 outputTarget );

// Dimension (in pixels) for the FFT image.
constexpr i32 FFT_TEXTURE_DIMENSION = 512;
