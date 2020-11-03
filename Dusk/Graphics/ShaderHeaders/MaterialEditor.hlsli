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

float FetchAttribute1D( MaterialEdAttribute attribute, float2 uvMapTexCoords, int layerIndex, int attributeIndex, float2 scale, float2 offset )
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
    
    return FetchBakedTextureSampler( uvMapTexCoords, layerIndex, attributeIndex, scale, offset ).r;
}

float3 FetchAttribute3D( MaterialEdAttribute attribute, float2 uvMapTexCoords, int layerIndex, int attributeIndex, float2 scale, float2 offset )
{
    if ( attribute.Type == ATTRIB_UNDEFINED ) {
        return float3( 1, 0, 1 );
    } else if ( attribute.Type == ATTRIB_FLOAT ) {
        return attribute.Input.xxx;
    } else if ( attribute.Type == ATTRIB_FLOAT2 ) {
        return float3( attribute.Input.x, attribute.Input.y, 0 );
    } else if ( attribute.Type == ATTRIB_FLOAT3 ) {
        return attribute.Input.xyz;
    }
    
    return FetchBakedTextureSampler( uvMapTexCoords, layerIndex, attributeIndex, scale, offset ).rgb;
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

Material FetchMaterialEdLayer( MaterialEdLayer layer, int layerIndex, float2 uvMapTexCoords )
{
    Material builtLayer;
    builtLayer.BaseColor = FetchAttribute3D( layer.BaseColor, uvMapTexCoords, layerIndex, 0, layer.LayerScale, layer.LayerOffset );
    builtLayer.Reflectance = FetchAttribute1D( layer.Reflectance, uvMapTexCoords, layerIndex, 1, layer.LayerScale, layer.LayerOffset );
    builtLayer.Roughness = FetchAttribute1D( layer.Roughness, uvMapTexCoords, layerIndex, 2, layer.LayerScale, layer.LayerOffset );
    builtLayer.Metalness = FetchAttribute1D( layer.Metalness, uvMapTexCoords, layerIndex, 3, layer.LayerScale, layer.LayerOffset );
    builtLayer.AmbientOcclusion = FetchAttribute1D( layer.AmbientOcclusion, uvMapTexCoords, layerIndex, 4, layer.LayerScale, layer.LayerOffset );
    builtLayer.Emissivity = FetchAttribute1D( layer.Emissivity, uvMapTexCoords, layerIndex, 5, layer.LayerScale, layer.LayerOffset );
    builtLayer.BlendMask = FetchAttribute1D( layer.BlendMask, uvMapTexCoords, layerIndex, 6, layer.LayerScale, layer.LayerOffset );
    builtLayer.ClearCoat = FetchAttribute1D( layer.ClearCoat, uvMapTexCoords, layerIndex, 7, layer.LayerScale, layer.LayerOffset );
    builtLayer.ClearCoatGlossiness = FetchAttribute1D( layer.ClearCoatGlossiness, uvMapTexCoords, layerIndex, 8, layer.LayerScale, layer.LayerOffset );
    return builtLayer;
}

void BlendMaterials( inout Material blendedMaterial, Material otherMaterial, MaterialEdContribution blendInfos )
{
    if ( blendInfos.BlendMode == BLEND_ADDITIVE ) {
        blendedMaterial.BaseColor = BlendAdditive3D( blendedMaterial.BaseColor, otherMaterial.BaseColor, otherMaterial.BlendMask, blendInfos.DiffuseContribution ); 
        blendedMaterial.Reflectance = BlendAdditive1D( blendedMaterial.Reflectance, otherMaterial.Reflectance, otherMaterial.BlendMask, blendInfos.SpecularContribution ); 
        blendedMaterial.Roughness = BlendAdditive1D( blendedMaterial.Roughness, otherMaterial.Roughness, otherMaterial.BlendMask, blendInfos.SpecularContribution ); 
        blendedMaterial.Metalness = BlendAdditive1D( blendedMaterial.Metalness, otherMaterial.Metalness, otherMaterial.BlendMask, blendInfos.SpecularContribution );
        blendedMaterial.ClearCoat = BlendAdditive1D( blendedMaterial.ClearCoat, otherMaterial.ClearCoat, otherMaterial.BlendMask, blendInfos.SpecularContribution );
        blendedMaterial.ClearCoatGlossiness = BlendAdditive1D( blendedMaterial.ClearCoatGlossiness, otherMaterial.ClearCoatGlossiness, otherMaterial.BlendMask, blendInfos.SpecularContribution );
    } else if ( blendInfos.BlendMode == BLEND_MULTIPLICATIVE ) {
        blendedMaterial.BaseColor = BlendMultiplicative3D( blendedMaterial.BaseColor, otherMaterial.BaseColor, otherMaterial.BlendMask, blendInfos.DiffuseContribution ); 
        blendedMaterial.Reflectance = BlendMultiplicative1D( blendedMaterial.Reflectance, otherMaterial.Reflectance, otherMaterial.BlendMask, blendInfos.SpecularContribution ); 
        blendedMaterial.Roughness = BlendMultiplicative1D( blendedMaterial.Roughness, otherMaterial.Roughness, otherMaterial.BlendMask, blendInfos.SpecularContribution ); 
        blendedMaterial.Metalness = BlendMultiplicative1D( blendedMaterial.Metalness, otherMaterial.Metalness, otherMaterial.BlendMask, blendInfos.SpecularContribution );
        blendedMaterial.ClearCoat = BlendMultiplicative1D( blendedMaterial.ClearCoat, otherMaterial.ClearCoat, otherMaterial.BlendMask, blendInfos.SpecularContribution );
        blendedMaterial.ClearCoatGlossiness = BlendMultiplicative1D( blendedMaterial.ClearCoatGlossiness, otherMaterial.ClearCoatGlossiness, otherMaterial.BlendMask, blendInfos.SpecularContribution );
    }
}

Material FetchMaterialAttributes( float2 uvMapTexCoords )
{
    Material generatedMaterial = FetchMaterialEdLayer( MaterialEditor.Layers[0], 0, uvMapTexCoords );
    
    for ( int layerIdx = 1; layerIdx < MaterialEditor.LayerCount; layerIdx++ ) {
        Material layerMaterial = FetchMaterialEdLayer( MaterialEditor.Layers[layerIdx], layerIdx, uvMapTexCoords );
        BlendMaterials( generatedMaterial, layerMaterial, MaterialEditor.Layers[layerIdx].Contribution );
    }
    
    return generatedMaterial;
}
#endif
