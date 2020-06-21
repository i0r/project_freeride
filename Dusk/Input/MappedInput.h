/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <map>
#include <set>

struct MappedInput
{
    std::set<dkStringHash_t>          Actions;
    std::set<dkStringHash_t>          States;
    std::map<dkStringHash_t, double>  Ranges;

    // Consumption helpers
    void eatAction( const dkStringHash_t action )
    {
        Actions.erase( action );
    }

    void eatState( const dkStringHash_t state )
    {
        States.erase( state );
    }

    void eatRange( const dkStringHash_t range )
    {
        auto iter = Ranges.find( range );

        if ( iter != Ranges.end() ) {
            Ranges.erase( iter );
        }
    }
};
