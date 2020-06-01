/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Graphics/Material.h>
#include <Maths/Vector.h>

#include "EditableMaterial.h"

class BaseAllocator;
class GraphicsAssetCache;
class Material;
class MaterialGenerator;
class VirtualFileSystem;

class MaterialEditor
{
public:
                        MaterialEditor( BaseAllocator* allocator, GraphicsAssetCache* gfxCache, VirtualFileSystem* vfs );
                        ~MaterialEditor();

#if DUSK_USE_IMGUI
    // Display MaterialEditor panel (as a ImGui window).
    void                displayEditorWindow();
#endif

    // Open the Material Editor window.
    void                openEditorWindow();

    // Set the material to edit.
    void                setActiveMaterial( Material* material );
    
    // Return a pointer to the material instance being edited (null if no material is being edited).
    Material*           getActiveMaterial() const;

private:
    // True if the editor window is opened in the editor workspace.
    bool                isOpened;

    // Editable instance of the active material.
    EditableMaterial    editedMaterial;

    // Pointer to the material instance being edited.
    Material*           activeMaterial;

    // Memory allocator owning this instance.
    BaseAllocator*      memoryAllocator;

    // Pointer to the active instance of the Graphics Assets cache.
    GraphicsAssetCache* graphicsAssetCache;

    // Pointer to the active instance of the Virtual Filesystem.
    VirtualFileSystem* virtualFileSystem;

    // Instance of the material generator.
    MaterialGenerator*  materialGenerator;

private:
    // The maximum length of a code piece attribute (in characters count).
    static constexpr u32 MAX_CODE_PIECE_LENGTH = 1024;

private:
    // Display GUI for a given Material Attribute.
    // SaturateInput can be used to clamp the input between 0 and 1 (for scalar and integer inputs).
    template<bool SaturateInput>
    void                displayMaterialAttribute( const char* displayName, MaterialAttribute& attribute );
};
