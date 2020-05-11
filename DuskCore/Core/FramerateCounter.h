/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct FramerateCounter
{
public:
    f32   MinFramePerSecond;
    f32   MaxFramePerSecond;
    f32   AvgFramePerSecond;

    f32   MinDeltaTime;
    f32   MaxDeltaTime;
    f32   AvgDeltaTime;

public:
            FramerateCounter();
            FramerateCounter( FramerateCounter& ) = default;
            FramerateCounter& operator = ( FramerateCounter& ) = default;
            ~FramerateCounter();
    
    void    onFrame( const f32 frameTime );

private:
    static constexpr i32 SAMPLE_COUNT = 128;

private:
    f32     fpsSamples[FramerateCounter::SAMPLE_COUNT];
    f32     dtSamples[FramerateCounter::SAMPLE_COUNT];
    i32     currentSampleIdx;
};
