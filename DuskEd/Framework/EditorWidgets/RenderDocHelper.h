#pragma once

struct Image;

class RenderDocHelper;
class GraphicsAssetCache;

class RenderDocHelperWidget
{
public:
	DUSK_INLINE Image* getIconBitmap() const { return renderDocIcon; }

public:
	RenderDocHelperWidget( RenderDocHelper* renderDocHelperInstance );
	~RenderDocHelperWidget();

#if DUSK_USE_IMGUI
	// Display RenderDocHelperWidget panel (as a ImGui window).
	void                displayEditorWindow();
#endif

	// Open the Render Doc Helper window.
	void                openWindow();

	// Load cached resources from cache.
	void				loadCachedResources( GraphicsAssetCache* graphicsAssetCache );

private:
	// Window state (true if visible; false otherwise).
	bool				isOpen;

	// True if a frame has been captured and should be opened in the attached
	// RenderDoc instance.
	bool				awaitingFrameCapture;

	// Number of frame to capture.
	i32					frameToCaptureCount;

	// Bitmap to use as RenderDoc icon.
	Image*				renderDocIcon;

	// Pointer to the RenderDoc helper instance.
	RenderDocHelper*	renderDocHelper;
};