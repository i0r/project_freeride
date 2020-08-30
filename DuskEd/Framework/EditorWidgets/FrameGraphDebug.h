/*
	Dusk Source Code
	Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;

class FrameGraphDebugWidget
{
public:
	FrameGraphDebugWidget();
	~FrameGraphDebugWidget();

#if DUSK_USE_IMGUI
	// Display FrameGraphDebugWidget panel (as a ImGui window).
	void                displayEditorWindow( const FrameGraph* frameGraph );
#endif

	// Open the FrameGraph debug window.
	void                openWindow();

private:
	// Pointer to the active frame graph instance.
	const FrameGraph*	frameGraphInstance;

	// Window state (true if visible; false otherwise).
	bool				isOpen;

	u32					selectedBufferIndex;

	u32					selectedImageIndex;
};
