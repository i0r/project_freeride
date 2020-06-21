/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

namespace dk
{
    namespace input
    {
        enum eInputLayout : uint32_t;
        enum eInputKey : uint32_t;
        enum eInputAxis : uint32_t;
    }
}

#include <unordered_map>

class RangeConverter
{
private:
    struct Converter
    {
        double MinimumInput;
        double MaximumInput;

        double MinimumOutput;
        double MaximumOutput;

        template <typename RangeType>
        RangeType convert( RangeType invalue ) const
        {
            double v = static_cast<double>( invalue );
            if ( v < MinimumInput )
                v = MinimumInput;
            else if ( v > MaximumInput )
                v = MaximumInput;

            double interpolationfactor = ( v - MinimumInput ) / ( MaximumInput - MinimumInput );
            return static_cast<RangeType>( ( interpolationfactor * ( MaximumOutput - MinimumOutput ) ) + MinimumOutput );
        }
    };

private:
    using ConversionMap_t = std::unordered_map<dkStringHash_t, Converter>;

    // Construction
public:
    RangeConverter() {}
    ~RangeConverter() { ConversionMap.clear(); }

    void addRangeToConverter( dkStringHash_t range, double inputMin, double inputMax, double outputMin, double outputMax ) {
        Converter converter;
        converter.MinimumInput = inputMin;
        converter.MaximumInput = inputMax;
        converter.MinimumOutput = outputMin;
        converter.MaximumOutput = outputMax;

        ConversionMap.insert( std::make_pair( range, converter ) );
    }

    // Conversion interface
public:
    template <typename RangeType>
    RangeType convert( dkStringHash_t rangeid, RangeType invalue ) const
    {
        ConversionMap_t::const_iterator iter = ConversionMap.find( rangeid );
        if ( iter == ConversionMap.end() )
            return invalue;

        return iter->second.convert<RangeType>( invalue );
    }

    // Internal tracking
private:
    ConversionMap_t ConversionMap;
};

class InputContext
{
public:
    const inline RangeConverter& getConversions() const { return rangeConverter; }

public:
            InputContext();
            InputContext( InputContext& ) = default;
            ~InputContext();

public:
    void    bindActionToButton( dk::input::eInputKey button, dkStringHash_t action );
    void    bindStateToButton( dk::input::eInputKey button, dkStringHash_t state );
    void    bindRangeToAxis( dk::input::eInputAxis axis, dkStringHash_t range );
    void    bindSensitivityToRange( double sensitivity, dkStringHash_t range );
    void    setRangeDataRange( double inputMin, double inputMax, double outputMin, double outputMax, dkStringHash_t range );

    bool    mapButtonToAction( dk::input::eInputKey button, dkStringHash_t& out ) const;
    bool    mapButtonToState( dk::input::eInputKey button, dkStringHash_t& out ) const;
    bool    mapAxisToRange( dk::input::eInputAxis axis, dkStringHash_t& out ) const;
    double  getSensitivity( dkStringHash_t range ) const;

private:
    std::unordered_map<dk::input::eInputKey, dkStringHash_t>     actionMap;
    std::unordered_map<dk::input::eInputKey, dkStringHash_t>     stateMap;
    std::unordered_map<dk::input::eInputAxis, dkStringHash_t>    rangeMap;
    std::unordered_map<dkStringHash_t, double>                   sensitivityMap;

    RangeConverter  rangeConverter;
};
