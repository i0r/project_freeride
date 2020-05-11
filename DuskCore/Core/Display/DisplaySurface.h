/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class InputReader;
struct NativeDisplaySurface;

#define DisplayMode( mode )\
        mode( WINDOWED )\
        mode( FULLSCREEN )\
        mode( BORDERLESS )

DUSK_LAZY_ENUM( DisplayMode )
#undef DisplayMode

class DisplaySurface
{
public:
    DUSK_INLINE NativeDisplaySurface*   getNativeDisplaySurface() const { return displaySurface; }
    DUSK_INLINE const eDisplayMode      getDisplayMode() const          { return displayMode; }
    DUSK_INLINE const u32               getWidth() const                { return width; }
    DUSK_INLINE const u32               getHeight() const               { return height; }

public:
                            DisplaySurface( BaseAllocator* allocator );
                            ~DisplaySurface();

    void                    create( const u32 surfaceWidth, const u32 surfaceHeight, const eDisplayMode initialDisplayMode );
    void                    setCaption( const dkChar_t* caption );
    bool                    hasReceivedQuitSignal() const;
    void                    pollSystemEvents( InputReader* inputReader );

private:
    BaseAllocator*          memoryAllocator;
    NativeDisplaySurface*   displaySurface;
    eDisplayMode            displayMode;
    u32                     width;
    u32                     height;
};
