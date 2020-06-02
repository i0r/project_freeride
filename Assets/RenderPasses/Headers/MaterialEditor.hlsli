#ifndef __MATERIAL_EDITOR_H__
#define __MATERIAL_EDITOR_H__ 1
struct MaterialEdAttribute
{
    // One dimensional input use the first scalar; two dimensional use the first two; etc.
    float3 Input;
    
    // Type of input used by this attribute.
    // 0 - Unused/1 - 1D/2 - 2D/3 - 3D/4 - Texture Sampler,
    int    Type;
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
    float2              LayerScale;
    float2              LayerOffset;
};

cbuffer MaterialEditorBuffer : register( b5 )
{
    // TODO Share Material::MAX_LAYER_COUNT between GPU and CPU.
    // MaterialEditor layers.
    MaterialEdLayer MaterialEdLayers[4];
    
    // MaterialEditor active layer count.
    int             MaterialEdLayerCount;
    
    // 0 - Additive/1 - Multiplicative
    int             MaterialEdBlendMode;
};

#include <Material.hlsli>

float FetchAttribute1D( MaterialEdAttribute attribute )
{
    if ( attribute.Type == 0 ) {
        return 0.0f;
    } else if ( attribute.Type == 1 ) {
        return layer.Input.x;
    } else if ( attribute.Type == 2 ) {
        return layer.Input.x;
    } else if ( attribute.Type == 3 ) {
        return layer.Input.x;
    }
}

float3 FetchAttribute3D( MaterialEdAttribute attribute )
{
    if ( attribute.Type == 0 ) {
        return float3( 1, 1, 0 );
    } else if ( attribute.Type == 1 ) {
        return layer.Input.xxx;
    } else if ( attribute.Type == 2 ) {
        return float3( layer.Input.x, layer.Input.y, 0 );
    } else if ( attribute.Type == 3 ) {
        return layer.Input.xyz;
    }
}

float3 BlendAdditive3D( float3 bottom, float3 top, float mask, float contribution )
{
    return lerp( bottom, top, mask * contribution );
}

float BlendAdditive1D( float bottom, float top, float mask, float contribution )
{
    return lerp( bottom, top, mask * contribution );
}

float3 BlendMultiplicative3D( float3 bottom, float3 top, float mask, float contribution )
{
    return lerp( bottom, bottom * top, mask * contribution );
}

float BlendMultiplicative1D( float bottom, float top, float mask, float contribution )
{
    return lerp( bottom, bottom * top, mask * contribution );
}

Material FetchMaterialEdLayer( MaterialEdLayer layer )
{
    Material builtLayer;
    builtLayer.BaseColor = FetchAttribute3D( layer.BaseColor );
    builtLayer.Reflectance = FetchAttribute1D( layer.Reflectance );
    builtLayer.Roughness = FetchAttribute1D( layer.Roughness );
    builtLayer.Metalness = FetchAttribute1D( layer.Metalness );
    builtLayer.AmbientOcclusion = FetchAttribute1D( layer.AmbientOcclusion );
    builtLayer.Emissivity = FetchAttribute1D( layer.Emissivity );
    builtLayer.BlendMask = FetchAttribute1D( layer.BlendMask ); 
    return builtLayer;
}

void BlendMaterials( Material blendedMaterial, Material otherMaterial )
{
    if ( MaterialEdBlendMode == 0 ) {
        blendedMaterial.BaseColor = BlendAdditive3D( blendedMaterial.BaseColor, otherMaterial.BaseColor, otherMaterial.BlendMask, 1.0f ); 
        blendedMaterial.Reflectance = BlendAdditive1D( blendedMaterial.Reflectance, otherMaterial.Reflectance, otherMaterial.BlendMask, 1.0f ); 
        blendedMaterial.Roughness = BlendAdditive1D( blendedMaterial.Roughness, otherMaterial.Roughness, otherMaterial.BlendMask, 1.0f ); 
        blendedMaterial.Metalness = BlendAdditive1D( blendedMaterial.Metalness, otherMaterial.Metalness, otherMaterial.BlendMask, 1.0f ); 
    } else if ( MaterialEdBlendMode == 1 ) {
        blendedMaterial.BaseColor = BlendMultiplicative3D( blendedMaterial.BaseColor, otherMaterial.BaseColor, otherMaterial.BlendMask, 1.0f ); 
        blendedMaterial.Reflectance = BlendMultiplicative1D( blendedMaterial.Reflectance, otherMaterial.Reflectance, otherMaterial.BlendMask, 1.0f ); 
        blendedMaterial.Roughness = BlendMultiplicative1D( blendedMaterial.Roughness, otherMaterial.Roughness, otherMaterial.BlendMask, 1.0f ); 
        blendedMaterial.Metalness = BlendMultiplicative1D( blendedMaterial.Metalness, otherMaterial.Metalness, otherMaterial.BlendMask, 1.0f ); 
    }
}

Material FetchMaterialAttributes()
{
    Material generatedMaterial = FetchMaterialEdLayer( MaterialEdLayer[0] );
    
    for ( int layerIdx = 1; layerIdx < MaterialEdLayerCount; layerIdx++ ) {
        Material layerMaterial = FetchMaterialEdLayer( MaterialEdLayer[layerIdx] );
        BlendMaterials( generatedMaterial, layerMaterial );
    }
    
    return generatedMaterial;
}
#endif
