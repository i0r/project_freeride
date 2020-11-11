#pragma once

struct Image;

class CpuProfiler;
class GraphicsAssetCache;

class CpuProfilerWidget
{
public:
	CpuProfilerWidget();
	~CpuProfilerWidget();

#if DUSK_USE_IMGUI
	// Display CpuProfilerWidget panel (as a ImGui window).
	void                displayEditorWindow();
#endif

	// Open the Render Doc Helper window.
	void                openWindow();

	// Load cached resources from cache.
	void				loadCachedResources( GraphicsAssetCache* graphicsAssetCache );

private:
	// Window state (true if visible; false otherwise).
	bool				isOpen;
};