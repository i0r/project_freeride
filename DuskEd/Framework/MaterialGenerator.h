/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class Material;

class MaterialGenerator 
{
public:
                        MaterialGenerator();
                        ~MaterialGenerator();

    // Create an editable material instance from a previously generated material instance.
    EditableMaterial    createEditableMaterial( const Material* material );
};
