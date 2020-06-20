/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class World 
{
public:
                            World( BaseAllocator* allocator );
                            ~World();

    void                    create();

    // Allocate and return a component less entity.
    Entity                  allocateEmptyEntity();

private:
    BaseAllocator*          memoryAllocator;

    EntityNameRegister*         entityDatabase;

    TransformDatabase*      transformDatabase;
};
