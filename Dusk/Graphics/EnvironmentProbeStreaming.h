/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct Buffer;
struct Image;
struct PipelineState;

class RenderDevice;
class CommandList;
class FrameGraph;
class ShaderCache;

enum eProbeUpdateStep
{
    PROBE_CAPTURE = 0,
    PROBE_RELIGHT,
    PROBE_CONVOLUTION,
    PROBE_UPDATE_COUNT,
};

enum eProbeUpdateFace
{
	FACE_X_PLUS = 0,
	FACE_X_MINUS,

	FACE_Y_PLUS,
	FACE_Y_MINUS,

	FACE_Z_PLUS,
	FACE_Z_MINUS,

    FACE_COUNT
};

class EnvironmentProbeStreaming
{
public:
                EnvironmentProbeStreaming();
                ~EnvironmentProbeStreaming();

    void        updateProbeCapture( FrameGraph& frameGraph );

    u32         addProbeForStreaming( const dkVec3f& worldPosition, const f32 probeRadius, const dkMat4x4f& inverseModelMatrix );

    void        addProbeRelight( const u32 probeIndex );

    void        addProbeConvlution( const u32 probeIndex );

    void        addProbeRecapture( const u32 probeIndex );

private:
    struct EnvironmentProbe {
        dkMat4x4f   InverseModelMatrix;
        dkVec3f     WorldPosition;
        f32         Radius;
    };

    static constexpr i32 DISTANT_PROBE_BUFFER_COUNT = 2;
	static constexpr i32 PROBE_COMPONENT_COUNT = 3;

private:
    // Distant probe (sky capture only).
    Image*              distantProbe[DISTANT_PROBE_BUFFER_COUNT][PROBE_COMPONENT_COUNT];

    // Distant probe current update step.
    eProbeUpdateStep    distantProbeUpdateStep;

    // Distant probe current face update (see distantProbeUpdateStep).
    eProbeUpdateFace    distantProbeFace;

    // Write index for distant probe update (to keep track of the ToD).
    i32                 distantProbeWriteIndex;

    // Atlas of streamed environment probe.
    Image*              probeAtlas[PROBE_COMPONENT_COUNT];

private:
    void                updateDistantProbe( FrameGraph& frameGraph );
};

