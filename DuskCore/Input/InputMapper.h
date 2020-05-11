/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "InputKeys.h"
#include "InputAxis.h"
#include "MappedInput.h"

#include <map>
#include <functional>
#include <list>

using InputCallback_t = std::function<void( MappedInput&, float )>;

class InputContext;
class FileSystemObject;

class InputMapper
{
public:
                                            InputMapper();
                                            InputMapper( InputMapper& ) = delete;
                                            ~InputMapper();

    void                                    update( const float frameTime );
    void                                    clear();
    void                                    setRawButtonState( dk::input::eInputKey button, bool pressed, bool previouslypressed );
    void                                    setRawAxisValue( dk::input::eInputAxis axis, double value );
    void                                    addCallback( InputCallback_t callback, int priority );
    void                                    addContext( const dkStringHash_t name, InputContext* context );

    void                                    pushContext( const dkStringHash_t name );
    void                                    popContext();

    void                                    deserialize( FileSystemObject* file );

private:
    std::map<dkStringHash_t, InputContext*>    inputContexts;
    std::list<InputContext*>                    activeContexts;
    std::multimap<int, InputCallback_t>         callbackTable;
    MappedInput                                 currentMappedInput;

private:
    bool                                    mapButtonToAction( dk::input::eInputKey button, dkStringHash_t& action ) const;
    bool                                    mapButtonToState( dk::input::eInputKey button, dkStringHash_t& state ) const;
    void                                    mapAndEatButton( dk::input::eInputKey button );
};
