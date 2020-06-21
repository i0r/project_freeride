/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "FramerateCounter.h"

#include <Maths/Helpers.h>

#include <limits>
#include <string.h>

FramerateCounter::FramerateCounter()
    : MinFramePerSecond( std::numeric_limits<f32>::max() )
    , MaxFramePerSecond( 0.0f )
    , AvgFramePerSecond( 0.0f )
    , MinDeltaTime( std::numeric_limits<f32>::max() )
    , MaxDeltaTime( 0.0f )
    , AvgDeltaTime( 0.0f )
    , fpsSamples{ 0.0f }
    , dtSamples{ 0.0f }
    , currentSampleIdx( 0 )
{
}

FramerateCounter::~FramerateCounter()
{
    MinFramePerSecond = 0.0f;
    MaxFramePerSecond = 0.0f;
    AvgFramePerSecond = 0.0f;

    MinDeltaTime = 0.0f;
    MaxDeltaTime = 0.0f;
    AvgDeltaTime = 0.0f;

    memset( fpsSamples, 0, sizeof( f32 ) * FramerateCounter::SAMPLE_COUNT );
    memset( dtSamples, 0, sizeof( f32 ) * FramerateCounter::SAMPLE_COUNT );

    currentSampleIdx = 0;
}

void FramerateCounter::onFrame( const f32 frameTime )
{
    constexpr f32 EPSILON = 1e-9f;

    fpsSamples[currentSampleIdx % SAMPLE_COUNT] = 1.0f / Max( EPSILON, frameTime );

    // Update FPS
    f32 fps = 0.0f;
    for ( int i = 0; i < SAMPLE_COUNT; i++ ) {
        fps += fpsSamples[i];
    }
    fps /= SAMPLE_COUNT;

    MinFramePerSecond = Min( MinFramePerSecond, fps );
    MaxFramePerSecond = Max( MaxFramePerSecond, fps );
    AvgFramePerSecond = fps;

    // Update dt
    dtSamples[currentSampleIdx % SAMPLE_COUNT] = frameTime;
    float dt = 0.0f;
    for ( int i = 0; i < SAMPLE_COUNT; i++ ) {
        dt += dtSamples[i];
    }
    dt /= SAMPLE_COUNT;

    // Convert to ms
    dt *= 1000.0f;

    MinDeltaTime = Min( MinDeltaTime, dt );
    MaxDeltaTime = Max( MaxDeltaTime, dt );
    AvgDeltaTime = dt;

    if ( currentSampleIdx == std::numeric_limits<int>::max() ) {
        currentSampleIdx = 0;
    }

    ++currentSampleIdx;
}
