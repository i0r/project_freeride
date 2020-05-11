/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <vector>

struct PrimitiveData
{
    std::vector<f32>   position;
    std::vector<f32>   uv;
    std::vector<f32>   normal;
    std::vector<u16>   indices;
};

namespace dk
{
    namespace maths
    {
        PrimitiveData CreateSpherePrimitive( const u32 stacks = 16u, const u32 slices = 16u );
        PrimitiveData CreateQuadPrimitive();
    }
}
