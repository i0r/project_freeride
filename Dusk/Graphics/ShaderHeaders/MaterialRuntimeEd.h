#ifndef __MATERIAL_RUNTIME_EDITOR_H__
#define __MATERIAL_RUNTIME_EDITOR_H__ 1

#ifdef __cplusplus
#include "HLSLCppInterop.h"
#else
#define DUSK_IS_MEMORY_ALIGNED_STATIC( x, y )
#endif

struct MaterialEdAttribute
{
    // One dimensional input use the first scalar; two dimensional use the first two; etc.
    float3 Input;
    
    // Type of input used by this attribute.
    // 0 - Unused/1 - 1D/2 - 2D/3 - 3D/4 - Texture Sampler,
    int    Type;
};

struct MaterialEdContribution
{
    int                 BlendMode;
    float               DiffuseContribution;
    float               SpecularContribution;
    float               NormalContribution;
};

struct MaterialEdLayer
{
    MaterialEdAttribute BaseColor;
    MaterialEdAttribute Reflectance;
    MaterialEdAttribute Roughness;
    MaterialEdAttribute Metalness;
    MaterialEdAttribute AmbientOcclusion;
    MaterialEdAttribute Emissivity;    
    MaterialEdAttribute BlendMask;
    MaterialEdAttribute ClearCoat;
    MaterialEdAttribute ClearCoatGlossiness;
    float2              LayerScale;
    float2              LayerOffset;
    
    MaterialEdContribution Contribution;
};
DUSK_IS_MEMORY_ALIGNED_STATIC( MaterialEdLayer, 16 );

struct MaterialEdData
{
    // TODO Share Material::MAX_LAYER_COUNT between GPU and CPU.
    // MaterialEditor layers.
    MaterialEdLayer Layers[4];

    // MaterialEditor active layer count.
    int             LayerCount;

    // 0 - Default / 1 - ClearCoat
    int             ShadingModel;

    bool            WriteVelocity;
    bool            ReceiveShadow;
    bool            ScaleUVByModelScale;

    bool            __PADDING__[5];
};
DUSK_IS_MEMORY_ALIGNED_STATIC( MaterialEdData, 16 );
#endif
