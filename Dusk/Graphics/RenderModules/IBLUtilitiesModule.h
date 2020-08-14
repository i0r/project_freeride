/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class GraphicsAssetCache;
class RenderDevice;

struct Image;



void    AddCubeFaceIrradianceComputePass( FrameGraph& frameGraph, Image* cubemap, Image* irradianceCube, const u32 faceIndex );
void    AddCubeFaceFilteringPass( FrameGraph& frameGraph, Image* cubemap, Image* filteredCube, const u32 faceIndex, const u32 mipIndex );
void    AddBrdfDfgLutComputePass( FrameGraph& frameGraph, Image* brdfDfgLut );
