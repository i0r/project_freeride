/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class ShaderCache;
class GraphicsAssetCache;

struct PipelineState;

#include <Graphics/FrameGraph.h>

class AutomaticExposureModule
{
public:
    static constexpr dkStringHash_t PERSISTENT_BUFFER_HASHCODE_READ = DUSK_STRING_HASH( "AutoExposure/ReadBuffer" );
    static constexpr dkStringHash_t PERSISTENT_BUFFER_HASHCODE_WRITE = DUSK_STRING_HASH( "AutoExposure/WriteBuffer" );

public:
                        AutomaticExposureModule();
                        AutomaticExposureModule( AutomaticExposureModule& ) = delete;
                        AutomaticExposureModule& operator = ( AutomaticExposureModule& ) = delete;
                        ~AutomaticExposureModule();

    void                importResourcesToGraph( FrameGraph& frameGraph );
    ResHandle_t         computeExposure( FrameGraph& frameGraph, ResHandle_t lightRenderTarget, const dkVec2u& screenSize );

    void                destroy( RenderDevice& renderDevice );
    void                loadCachedResources( RenderDevice& renderDevice );

private:
    struct AutoExposureInfo {
        f32             EngineLuminanceFactor;
        f32             TargetLuminance;
        f32             MinLuminanceLDR;
        f32             MaxLuminanceLDR;
        f32             MiddleGreyLuminanceLDR;
        f32             EV;
        f32             Fstop;
        u32             PeakHistogramValue;

        AutoExposureInfo()
            : EngineLuminanceFactor( 1.0f )
            , TargetLuminance( 0.5f )
            , MinLuminanceLDR( 0.0f )
            , MaxLuminanceLDR( 1.0f )
            , MiddleGreyLuminanceLDR( 1.0f )
            , EV( 0.0f )
            , Fstop( 0.0f )
            , PeakHistogramValue( 0u )
        {

        }
    };

private:
    Buffer*             autoExposureBuffer[2];
    u32                 exposureTarget;
    AutoExposureInfo    autoExposureInfo;

private:
    ResHandle_t         addBinComputePass( FrameGraph& frameGraph, const ResHandle_t inputRenderTarget, const dkVec2u& screenSize );
    ResHandle_t         addHistogramMergePass( FrameGraph& frameGraph, const ResHandle_t perTileHistoBuffer, const dkVec2u& screenSize );
    ResHandle_t         addExposureComputePass( FrameGraph& frameGraph, const ResHandle_t mergedHistoBuffer );
};
