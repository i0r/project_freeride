/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "InputContext.h"

using namespace dk::input;

InputContext::InputContext()
    : rangeConverter()
{

}

InputContext::~InputContext()
{
    actionMap.clear();
    stateMap.clear();
    rangeMap.clear();
    sensitivityMap.clear();
}

void InputContext::bindActionToButton( eInputKey button, dkStringHash_t action )
{
    auto iter = actionMap.find( button );
    if ( iter == actionMap.end() ) {
        actionMap.emplace( button, action );
    } else {
        actionMap[button] = action;
    }
}

void InputContext::bindStateToButton( eInputKey button, dkStringHash_t state )
{
    auto iter = stateMap.find( button );
    if ( iter == stateMap.end() ) {
        stateMap.emplace( button, state );
    } else {
        stateMap[button] = state;
    }
}

void InputContext::bindRangeToAxis( eInputAxis axis, dkStringHash_t range )
{
    auto iter = rangeMap.find( axis );
    if ( iter == rangeMap.end() ) {
        rangeMap.emplace( axis, range );
    } else {
        rangeMap[axis] = range;
    }
}

void InputContext::bindSensitivityToRange( double sensitivity, dkStringHash_t range )
{
    auto iter = sensitivityMap.find( range );
    if ( iter == sensitivityMap.end() ) {
        sensitivityMap.emplace( range, sensitivity );
    } else {
        sensitivityMap[range] = sensitivity;
    }
}

void InputContext::setRangeDataRange( double inputMin, double inputMax, double outputMin, double outputMax, dkStringHash_t range )
{
    rangeConverter.addRangeToConverter( range, inputMin, inputMax, outputMin, outputMax );
}

bool InputContext::mapButtonToAction( eInputKey button, dkStringHash_t& out ) const
{
    auto iter = actionMap.find( button );
    if ( iter == actionMap.end() ) {
        return false;
    }

    out = iter->second;
    return true;
}

bool InputContext::mapButtonToState( eInputKey button, dkStringHash_t& out ) const
{
    auto iter = stateMap.find( button );
    if ( iter == stateMap.end() ) {
        return false;
    }

    out = iter->second;
    return true;
}

bool InputContext::mapAxisToRange( eInputAxis axis, dkStringHash_t& out ) const
{
    auto iter = rangeMap.find( axis );
    if ( iter == rangeMap.end() ) {
        return false;
    }

    out = iter->second;
    return true;
}

double InputContext::getSensitivity( dkStringHash_t range ) const
{
    auto iter = sensitivityMap.find( range );
    if ( iter == sensitivityMap.end() ) {
        return 1.0;
    }

    return iter->second;
}
