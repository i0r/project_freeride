#ifndef __MATERIAL_EDITOR_H__
#define __MATERIAL_EDITOR_H__ 1

#include <MaterialRuntimeEd.h>
#include <Material.hlsli>

#define BLEND_ADDITIVE 0
#define BLEND_MULTIPLICATIVE 1

#define ATTRIB_UNDEFINED 0
#define ATTRIB_FLOAT 1
#define ATTRIB_FLOAT2 2
#define ATTRIB_FLOAT3 3
#define ATTRIB_TEX2D 4

cbuffer MaterialEditorBuffer : register( b5 )
{
    MaterialEdData  MaterialEditor;
};

float FetchAttribute1D( MaterialEdAttribute attribute )
{
    if ( attribute.Type == ATTRIB_UNDEFINED ) {
        return 0.0f;
    } else if ( attribute.Type == ATTRIB_FLOAT ) {
        return attribute.Input.x;
    } else if ( attribute.Type == ATTRIB_FLOAT2 ) {
        return attribute.Input.x;
    } else if ( attribute.Type == ATTRIB_FLOAT3 ) {
        return attribute.Input.x;
    }
    
    return 0.0f;
}

float3 FetchAttribute3D( MaterialEdAttribute attribute )
{
    if ( attribute.Type == ATTRIB_UNDEFINED ) {
        return float3( 1, 1, 0 );
    } else if ( attribute.Type == ATTRIB_FLOAT ) {
        return attribute.Input.xxx;
    } else if ( attribute.Type == ATTRIB_FLOAT2 ) {
        return float3( attribute.Input.x, attribute.Input.y, 0 );
    } else if ( attribute.Type == ATTRIB_FLOAT3 ) {
        return attribute.Input.xyz;
    }
    
    return float3( 0, 0, 0 );
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

void BlendMaterials( inout Material blendedMaterial, Material otherMaterial, MaterialEdContribution blendInfos )
{
    if ( blendInfos.BlendMode == BLEND_ADDITIVE ) {
        blendedMaterial.BaseColor = BlendAdditive3D( blendedMaterial.BaseColor, otherMaterial.BaseColor, otherMaterial.BlendMask, blendInfos.DiffuseContribution ); 
        blendedMaterial.Reflectance = BlendAdditive1D( blendedMaterial.Reflectance, otherMaterial.Reflectance, otherMaterial.BlendMask, blendInfos.SpecularContribution ); 
        blendedMaterial.Roughness = BlendAdditive1D( blendedMaterial.Roughness, otherMaterial.Roughness, otherMaterial.BlendMask, blendInfos.SpecularContribution ); 
        blendedMaterial.Metalness = BlendAdditive1D( blendedMaterial.Metalness, otherMaterial.Metalness, otherMaterial.BlendMask, blendInfos.SpecularContribution );
    } else if ( blendInfos.BlendMode == BLEND_MULTIPLICATIVE ) {
        blendedMaterial.BaseColor = BlendMultiplicative3D( blendedMaterial.BaseColor, otherMaterial.BaseColor, otherMaterial.BlendMask, blendInfos.DiffuseContribution ); 
        blendedMaterial.Reflectance = BlendMultiplicative1D( blendedMaterial.Reflectance, otherMaterial.Reflectance, otherMaterial.BlendMask, blendInfos.SpecularContribution ); 
        blendedMaterial.Roughness = BlendMultiplicative1D( blendedMaterial.Roughness, otherMaterial.Roughness, otherMaterial.BlendMask, blendInfos.SpecularContribution ); 
        blendedMaterial.Metalness = BlendMultiplicative1D( blendedMaterial.Metalness, otherMaterial.Metalness, otherMaterial.BlendMask, blendInfos.SpecularContribution );
    }
}

Material FetchMaterialAttributes()
{
    Material generatedMaterial = FetchMaterialEdLayer( MaterialEditor.Layers[0] );
    
    for ( int layerIdx = 1; layerIdx < MaterialEditor.LayerCount; layerIdx++ ) {
        Material layerMaterial = FetchMaterialEdLayer( MaterialEditor.Layers[layerIdx] );
        BlendMaterials( generatedMaterial, layerMaterial, MaterialEditor.Layers[layerIdx].Contribution );
    }
    
    return generatedMaterial;
}
#endif
