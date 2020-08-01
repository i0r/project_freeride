/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_DEVBUILD
#if DUSK_USE_RENDERDOC
#include <RenderDoc/renderdoc_app.h>

class DisplaySurface;
class RenderDevice;

class RenderDocHelper
{
public:
    DUSK_INLINE bool        isAvailable() const { return ( renderDocAPI != nullptr ); }
    DUSK_INLINE bool        isReferencingAllResources() const { return renderDocAPI->GetCaptureOptionU32( eRENDERDOC_Option_RefAllResources ); }
    DUSK_INLINE bool        isCapturingAllCmdLists() const { return renderDocAPI->GetCaptureOptionU32( eRENDERDOC_Option_CaptureAllCmdLists ); }

    DUSK_INLINE void        referenceAllResources( const bool state ) const { renderDocAPI->SetCaptureOptionU32( eRENDERDOC_Option_RefAllResources, ( state ) ? 1u : 0u ); }
    DUSK_INLINE void        captureAllCmdLists( const bool state ) const { renderDocAPI->SetCaptureOptionU32( eRENDERDOC_Option_CaptureAllCmdLists, ( state ) ? 1u : 0u ); }

    DUSK_INLINE void        beginCapture() const { renderDocAPI->StartFrameCapture( nullptr, nullptr ); }
    DUSK_INLINE void        endCapture() const { renderDocAPI->EndFrameCapture( nullptr, nullptr ); }

    DUSK_INLINE const char* getCaptureStorageFolder() const { return captureStorageFolder.c_str(); }
    DUSK_INLINE const char* getAPIVersion() const { return apiVersionString.c_str(); }

public:
                            RenderDocHelper();
                            RenderDocHelper( RenderDocHelper& ) = delete;
                            RenderDocHelper& operator = ( RenderDocHelper& ) = delete;
                            ~RenderDocHelper();

    void                    create();
    void                    attachTo( const DisplaySurface & displaySurface, const RenderDevice & renderDevice );

    void                    triggerCapture( const u32 frameCountToCapture = 1u );
    bool                    openLatestCapture();

private:
    void*                   renderDocLibrary;
    RENDERDOC_API_1_2_0*    renderDocAPI;
    u32                     pendingFrameCountToCapture;
    std::string             captureStorageFolder;
    std::string             apiVersionString;
};
#endif
#endif
