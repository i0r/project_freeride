#pragma once

#include <Maths/Vector.h>
#include <Maths/Matrix.h>

// Typedef CPP type with HLSL aliases.
// This way we can include HLSL headers in CPP code and vice-versa.
using float4x4 = dkMat4x4f;
using float4 = dkVec4f;
using float3 = dkVec3f;
using float2 = dkVec2f;
using uint = u32;

// Dummy declaration to avoid compilation errors
using Texture1D = i32;
using Texture2D = i32;
using Texture3D = i32;
