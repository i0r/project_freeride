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

    // Create a system display surface with a given width/height and display mode. For multi screen systems, monitor index
    // is the index of the monitor used to place the window (if 0 will use the primary display). Note that the index is 
    // system specific (you should use user preferences to find the correct index).
    void                    create( const u32 surfaceWidth, const u32 surfaceHeight, const eDisplayMode initialDisplayMode, const i32 initialMonitorIndex = 0u );
    
    // Set this surface caption (if the underlying implementation allows it).
    void                    setCaption( const dkChar_t* caption );

    // Return true if this surface has received a quit signal from the system; false otherwise.
    bool                    hasReceivedQuitSignal() const;

    // Return true if this surface has received a resize event. This call DOES NOT consume the event; instead, you should
    // call acknowledgeResizeEvent().
    bool                    hasReceivedResizeEvent() const;

    // Reset internal flags related to resizing. Should be called once a resize event has been handled by the engine.
    void                    acknowledgeResizeEvent();

    // Poll events received from the system for this frame.
    void                    pollSystemEvents( InputReader* inputReader );

    // Change this surface display mode and update its internal state (with
    // system specifics calls).
    void                    changeDisplayMode( const eDisplayMode newDisplayMode );

private:
    // Memory allocator owning this instance.
    BaseAllocator*          memoryAllocator;
    
    // Opaque structure holding system specifics infos (handle; flags; etc.).
    NativeDisplaySurface*   displaySurface;

    // Current display mode for this surface.
    eDisplayMode            displayMode;

    u32                     width;
    u32                     height;
    i32                     originX;
    i32                     originY;
    u32                     monitorIndex;

private:
    void                    setWindowedDisplayMode();

    void                    setFullscreenDisplayMode();

    void                    setFullscreenBorderlessDisplayMode();
};
