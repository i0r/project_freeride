/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Timer.h"

#if DUSK_UNIX
#include <time.h>

// Return time as microseconds
static i64 GetCurrentTimeImpl()
{
    timespec time;
    clock_gettime( CLOCK_MONOTONIC, &time );

    return ( static_cast< i64 >( time.tv_sec ) * 1000000 + time.tv_nsec / 1000 );
}
#elif DUSK_WIN
#include <time.h>

LARGE_INTEGER GetFrequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency( &frequency );
    return frequency;
}

// Return time as microseconds
static i64 GetCurrentTimeImpl()
{
    static LARGE_INTEGER frequency = GetFrequency();

    LARGE_INTEGER time;
    QueryPerformanceCounter( &time );

    return 1000000 * time.QuadPart / frequency.QuadPart;
}
#endif

Timer::Timer()
    : startTime( 0ull )
    , previousTime( 0ull )
{
	reset();
}

void Timer::start()
{
    reset();
}

void Timer::reset()
{
    startTime = GetCurrentTimeImpl();
    previousTime = startTime;
}

f64 Timer::getDelta()
{
    i64 currentTime = GetCurrentTimeImpl();

    i64 deltaTime = currentTime - previousTime;
    previousTime = currentTime;

    return static_cast< f64 >( deltaTime );
}

f64 Timer::getDeltaAsMiliseconds()
{
    return getDelta() * 0.001;
}

f64 Timer::getDeltaAsSeconds()
{
    return getDelta() * 0.000001;
}

f64 Timer::getElapsedTime() const
{
    return static_cast< f64 >( GetCurrentTimeImpl() - startTime );
}

f64 Timer::getElapsedTimeAsMiliseconds() const
{
    return getElapsedTime() * 0.001;
}

f64 Timer::getElapsedTimeAsSeconds() const
{
    return getElapsedTime() * 0.000001;
}
