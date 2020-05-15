/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <limits>
#include <cmath>

#include <Core/Types.h>
#include <Core/DebugHelpers.h>

namespace dk
{
    namespace maths
    {
        template< typename T, typename R > constexpr T max( const T a, const R b );
    }
}

template <typename Precision, i32 ScalarCount>
struct Vector;

template<typename Precision>
struct Vector<Precision, 1>
{
    static constexpr i32        SCALAR_COUNT = 1;
    static constexpr Precision  EPSILON = std::numeric_limits<Precision>::epsilon();

    static const Vector         Zero;
    static const Vector         Max;

    union {
        Precision               scalars[SCALAR_COUNT];

        struct {
            Precision           x;
        };
    };

    // Constructors
    constexpr Vector() = default;

    constexpr Vector( const Precision& x ) 
        : scalars{ x } 
    {

    }

    // Promotions
    template <typename PrecisionR>
    constexpr explicit Vector( const Vector<PrecisionR, SCALAR_COUNT>& r ) 
        : scalars{ ( Precision )r.scalars[0] }
    {

    }

    constexpr const Precision& operator[] ( const u32 index ) const
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index]; 
    }

    constexpr Precision& operator[] ( const u32 index ) 
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index]; 
    }

    constexpr Vector& operator+ () const 
    { 
        return *this; 
    }

    constexpr Vector operator- () const  
    { 
        return Vector( -scalars[0] ); 
    }

    constexpr Vector<Precision, 1>& operator+= ( const Vector<Precision, 1>& r )
    {
        scalars[0] += r.scalars[0];

        return *this;
    }

    constexpr Vector<Precision, 1>& operator-= ( const Vector<Precision, 1>& r )
    {
        scalars[0] -= r.scalars[0];

        return *this;
    }

    constexpr Vector<Precision, 1>& operator*= ( const Vector<Precision, 1>& r )
    {
        scalars[0] *= r.scalars[0];

        return *this;
    }

    constexpr Vector<Precision, 1>& operator*= ( const Precision& r )
    {
        scalars[0] *= r;

        return *this;
    }

    constexpr Vector<Precision, 1>& operator/= ( const Vector<Precision, 1>& r )
    {
        scalars[0] /= r.scalars[0];

        return *this;
    }

    constexpr Vector<Precision, 1>& operator/= ( const Precision& r )
    {
        scalars[0] /= r;

        return *this;
    }

    constexpr bool operator== ( const Vector<Precision, 1>& r )
    { 
        return scalars[0] == r.scalars[0]; 
    }

    constexpr bool operator!= ( const Vector<Precision, 1>& r )
    {
        return scalars[0] != r.scalars[0];
    }

    constexpr Vector<Precision, 1> normalize() const
    {
        Vector r = *this / length();

        return Vector<Precision, 1>( r.scalars[0] );
    }

    static constexpr Precision dot( const Vector<Precision, 1>& l, const Vector<Precision, 1>& r )
    {
        return l.scalars[0] * r.scalars[0];
    }

    constexpr Precision lengthSquared() const
    {
        return Vector::dot( *this, *this );
    }

    constexpr Precision length() const 
    { 
        return sqrt( lengthSquared() );
    }

    static constexpr Vector<Precision, 1> min( const Vector<Precision, 1>& l, const Vector<Precision, 1>& r )
    {
        Vector<Precision, 1> v;

        for ( i32 i = 0; i < 1; i++ )
            v[i] = ( l[i] < r[i] ) ? l[i] : r[i];

        return v;
    }

    static constexpr Vector<Precision, 1> max( const Vector<Precision, 1>& l, const Vector<Precision, 1>& r )
    {
        Vector<Precision, 1> v;

        for ( i32 i = 0; i < 1; i++ )
            v[i] = ( l[i] > r[i] ) ? l[i] : r[i];

        return v;
    }
};

template<typename Precision>
struct Vector<Precision, 2>
{
    static constexpr i32        SCALAR_COUNT = 2;
    static constexpr Precision  EPSILON = std::numeric_limits<Precision>::epsilon();

    static const Vector         Zero;
    static const Vector         Max;

    union {
        Precision               scalars[SCALAR_COUNT];

        struct {
            Precision           x;
            Precision           y;
        };
    };

    // Constructors
    constexpr Vector() = default;

    constexpr Vector( const Precision& x )
        : scalars{ x, x }
    {

    }

    constexpr Vector( const Precision& x, const Precision& y )
        : scalars{ x, y }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x )
        : scalars{ x[0], x[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Precision& y )
        : scalars{ x[0], y }
    {

    }

    constexpr Vector( const Precision& x, const Vector<Precision, 1>& y )
        : scalars{ x, y[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Vector<Precision, 1>& y )
        : scalars{ x[0], y[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 3>& xyz )
        : scalars{ xyz[0], xyz[1] }
    {

    }

    constexpr Vector( const Vector<Precision, 4>& xyzw )
        : scalars{ xyzw[0], xyzw[1] }
    {

    }

    // Promotions
    template <typename PrecisionR>
    constexpr explicit Vector( const Vector<PrecisionR, SCALAR_COUNT>& r )
        : scalars{ (Precision)r.scalars[0], (Precision)r.scalars[1] }
    {

    }

    constexpr const Precision& operator[] ( const u32 index ) const
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index];
    }

    constexpr Precision& operator[] ( const u32 index )
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index];
    }

    constexpr Vector& operator+ () const
    {
        return *this;
    }

    constexpr Vector operator- () const
    {
        return Vector( -scalars[0], -scalars[1] );
    }

    constexpr Vector<Precision, 2>& operator+= ( const Vector<Precision, 2>& r )
    {
        scalars[0] += r.scalars[0];
        scalars[1] += r.scalars[1];

        return *this;
    }

    constexpr Vector<Precision, 2>& operator-= ( const Vector<Precision, 2>& r )
    {
        scalars[0] -= r.scalars[0];
        scalars[1] -= r.scalars[1];

        return *this;
    }

    constexpr Vector<Precision, 2>& operator*= ( const Vector<Precision, 2>& r )
    {
        scalars[0] *= r.scalars[0];
        scalars[1] *= r.scalars[1];

        return *this;
    }

    constexpr Vector<Precision, 2>& operator*= ( const Precision& r )
    {
        scalars[0] *= r;
        scalars[1] *= r;

        return *this;
    }

    constexpr Vector<Precision, 2>& operator/= ( const Vector<Precision, 2>& r )
    {
        scalars[0] /= r.scalars[0];
        scalars[1] /= r.scalars[1];

        return *this;
    }

    constexpr Vector<Precision, 2>& operator/= ( const Precision& r )
    {
        scalars[0] /= r;
        scalars[1] /= r;

        return *this;
    }

    constexpr bool operator== ( const Vector<Precision, 2>& r )
    {
        return scalars[0] == r.scalars[0] && scalars[1] == r.scalars[1];
    }

    constexpr bool operator!= ( const Vector<Precision, 1>& r )
    {
        return scalars[0] != r.scalars[0] || scalars[1] != r.scalars[1];
    }

    constexpr Vector<Precision, 2> normalize() const
    {
        Vector r = *this / length();

        return Vector<Precision, 2>( r.scalars[0], r.scalars[1] );
    }

    static constexpr Precision dot( const Vector<Precision, 2>& l, const Vector<Precision, 2>& r )
    {
        return l.scalars[0] * r.scalars[0] + l.scalars[1] * r.scalars[1];
    }

    constexpr Precision lengthSquared() const
    {
        return Vector::dot( *this, *this );
    }

    constexpr Precision length() const
    {
        return sqrt( lengthSquared() );
    }

    static constexpr Vector<Precision, 2> min( const Vector<Precision, 2>& l, const Vector<Precision, 2>& r )
    {
        Vector<Precision, 2> v;

        for ( i32 i = 0; i < 2; i++ )
            v[i] = ( l[i] < r[i] ) ? l[i] : r[i];

        return v;
    }

    static constexpr Vector<Precision, 2> max( const Vector<Precision, 2>& l, const Vector<Precision, 2>& r )
    {
        Vector<Precision, 2> v;

        for ( i32 i = 0; i < 2; i++ )
            v[i] = ( l[i] > r[i] ) ? l[i] : r[i];

        return v;
    }
};

template<typename Precision>
struct Vector<Precision, 3>
{
    static constexpr i32        SCALAR_COUNT = 3;
    static constexpr Precision  EPSILON = std::numeric_limits<Precision>::epsilon();

    static const Vector         Zero;
    static const Vector         Max;

    union
    {
        Precision               scalars[SCALAR_COUNT];

        struct
        {
            Precision           x;
            Precision           y;
            Precision           z;
        };
    };

    // Constructors
    constexpr Vector() = default;

    constexpr Vector( const Precision& x )
        : scalars{ x, x, x }
    {

    }

    constexpr Vector( const Precision& x, const Precision& y, const Precision& z )
        : scalars{ x, y, z }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Precision& y, const Precision& z )
        : scalars{ x[0], y, z }
    {

    }

    constexpr Vector( const Precision& x, const Vector<Precision, 1>& y, const Precision& z )
        : scalars{ x, y[0], z }
    {

    }

    constexpr Vector( const Precision& x, const Precision& y, const Vector<Precision, 1>& z )
        : scalars{ x, y, z[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Precision& y, const Vector<Precision, 1>& z )
        : scalars{ x[0], y, z[0] }
    {

    }

    constexpr Vector( const Precision& x, const Vector<Precision, 1>& y, const Vector<Precision, 1>& z )
        : scalars{ x, y[0], z[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Vector<Precision, 1>& y, const Precision& z )
        : scalars{ x[0], y[0], z }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Vector<Precision, 1>& y, const Vector<Precision, 1>& z )
        : scalars{ x[0], y[0], z[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 2>& xy, const Precision& z )
        : scalars{ xy[0], xy[1], z }
    {

    }

    constexpr Vector( const Precision& x, const Vector<Precision, 2>& yz )
        : scalars{ x, yz[0], yz[1] }
    {

    }

    constexpr Vector( const Vector<Precision, 4>& xyzw )
        : scalars{ xyzw[0], xyzw[1], xyzw[2] }
    {

    }

    // Promotions
    template <typename PrecisionR>
    constexpr explicit Vector( const Vector<PrecisionR, SCALAR_COUNT>& r )
        : scalars{ (Precision)r.scalars[0], (Precision)r.scalars[1], (Precision)r.scalars[2] }
    {

    }

    constexpr const Precision& operator[] ( const u32 index ) const
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index];
    }

    constexpr Precision& operator[] ( const u32 index )
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index];
    }

    constexpr Vector& operator+ () const
    {
        return *this;
    }

    constexpr Vector operator- () const
    {
        return Vector( -scalars[0], -scalars[1], -scalars[2] );
    }

    constexpr Vector<Precision, 3>& operator+= ( const Vector<Precision, 3>& r )
    {
        scalars[0] += r.scalars[0];
        scalars[1] += r.scalars[1];
        scalars[2] += r.scalars[2];

        return *this;
    }

    constexpr Vector<Precision, 3>& operator-= ( const Vector<Precision, 3>& r )
    {
        scalars[0] -= r.scalars[0];
        scalars[1] -= r.scalars[1];
        scalars[2] -= r.scalars[2];

        return *this;
    }

    constexpr Vector<Precision, 3>& operator*= ( const Vector<Precision, 3>& r )
    {
        scalars[0] *= r.scalars[0];
        scalars[1] *= r.scalars[1];
        scalars[2] *= r.scalars[2];

        return *this;
    }

    constexpr Vector<Precision, 3>& operator*= ( const Precision& r )
    {
        scalars[0] *= r;
        scalars[1] *= r;
        scalars[2] *= r;

        return *this;
    }

    constexpr Vector<Precision, 3>& operator/= ( const Vector<Precision, 3>& r )
    {
        scalars[0] /= r.scalars[0];
        scalars[1] /= r.scalars[1];
        scalars[2] /= r.scalars[2];

        return *this;
    }

    constexpr Vector<Precision, 3>& operator/= ( const Precision& r )
    {
        scalars[0] /= r;
        scalars[1] /= r;
        scalars[2] /= r;

        return *this;
    }

    constexpr bool operator== ( const Vector<Precision, 3>& r ) const
    {
        return scalars[0] == r.scalars[0] && scalars[1] == r.scalars[1] && scalars[2] == r.scalars[2];
    }

    constexpr bool operator!= ( const Vector<Precision, 3>& r ) const
    {
        return scalars[0] != r.scalars[0] || scalars[1] != r.scalars[1] || scalars[2] != r.scalars[2];
    }

    constexpr Vector<Precision, 3> normalize() const
    {
        Vector r = *this / length();

        return Vector<Precision, 3>( r.scalars[0], r.scalars[1], r.scalars[2] );
    }

    static constexpr Precision dot( const Vector<Precision, 3>& l, const Vector<Precision, 3>& r )
    {
        return l.scalars[0] * r.scalars[0] + l.scalars[1] * r.scalars[1] + l.scalars[2] * r.scalars[2];
    }

    static constexpr Vector<Precision, 3> cross( const Vector<Precision, 3>& l, const Vector<Precision, 3>& r )
    {
        return Vector<Precision, 3>( l.scalars[1] * r.scalars[2] - l.scalars[2] * r.scalars[1], l.scalars[2] * r.scalars[0] - l.scalars[0] * r.scalars[2], l.scalars[0] * r.scalars[1] - l.scalars[1] * r.scalars[0] );
    }

    constexpr Precision lengthSquared() const
    {
        return Vector::dot( *this, *this );
    }

    constexpr Precision length() const
    {
        return sqrt( lengthSquared() );
    }

    static constexpr Precision distanceSquared( const Vector<Precision, 3>& l, const Vector<Precision, 3>& r )
    {
        const Vector<Precision, 3> dist = ( r - l );

        return ( dist.x * dist.x + dist.y * dist.y + dist.z * dist.z );
    }

    static constexpr Vector<Precision, 3> cos( const Vector<Precision, 3>& r )
    {
        Vector<Precision, 3> v = r;

        for ( i32 i = 0; i < 3; i++ )
            v[i] = ::cos( r[i] );

        return v;
    }

    static constexpr Vector<Precision, 3> sin( const Vector<Precision, 3>& r )
    {
        Vector<Precision, 3> v = r;

        for ( i32 i = 0; i < 3; i++ )
            v[i] = ::sin( r[i] );

        return v;
    }

    static constexpr Vector<Precision, 3> min( const Vector<Precision, 3>& l, const Vector<Precision, 3>& r )
    {
        Vector<Precision, 3> v;

        for ( i32 i = 0; i < 3; i++ )
            v[i] = ( l[i] < r[i] ) ? l[i] : r[i];

        return v;
    }

    static constexpr Vector<Precision, 3> max( const Vector<Precision, 3>& l, const Vector<Precision, 3>& r )
    {
        Vector<Precision, 3> v;

        for ( i32 i = 0; i < 3; i++ )
            v[i] = ( l[i] > r[i] ) ? l[i] : r[i];

        return v;
    }
};

template<typename Precision>
struct Vector<Precision, 4>
{
    static constexpr i32        SCALAR_COUNT = 4;
    static constexpr Precision  EPSILON = std::numeric_limits<Precision>::epsilon();

    static const Vector         Zero;
    static const Vector         Max;

    union
    {
        Precision               scalars[SCALAR_COUNT];

        struct
        {
            Precision           x;
            Precision           y;
            Precision           z;
            Precision           w;
        };
    };

    // Constructors
    constexpr Vector() = default;

    constexpr Vector( const Precision& x )
        : scalars{ x, x, x, x }
    {

    }

    constexpr Vector( const Precision& x, const Precision& y, const Precision& z, const Precision& w )
        : scalars{ x, y, z, w }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Precision& y, const Precision& z, const Precision& w )
        : scalars{ x[0], y, z, w }
    {

    }

    constexpr Vector( const Precision& x, const Vector<Precision, 1>& y, const Precision& z, const Precision& w )
        : scalars{ x, y[0], z, w }
    {

    }

    constexpr Vector( const Precision& x, const Precision& y, const Vector<Precision, 1>& z, const Precision& w )
        : scalars{ x, y, z[0], w }
    {

    }

    constexpr Vector( const Precision& x, const Precision& y, const Precision& z, const Vector<Precision, 1>& w )
        : scalars{ x, y, z, w[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Vector<Precision, 1>& y, const Precision& z, const Precision& w )
        : scalars{ x[0], y[0], z, w }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Precision& y, const Vector<Precision, 1>& z, const Precision& w )
        : scalars{ x[0], y, z[0], w }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Precision& y, const Precision& z, const Vector<Precision, 1>& w )
        : scalars{ x[0], y, z, w[0] }
    {

    }

    constexpr Vector( const Precision& x, const Vector<Precision, 1>& y, const Vector<Precision, 1>& z, const Precision& w )
        : scalars{ x, y[0], z[0], w }
    {

    }

    constexpr Vector( const Precision& x, const Vector<Precision, 1>& y, const Precision& z, const Vector<Precision, 1>& w )
        : scalars{ x, y[0], z, w[0] }
    {

    }


    constexpr Vector( const Precision& x, const Precision& y, const Vector<Precision, 1>& z, const Vector<Precision, 1>& w )
        : scalars{ x, y, z[0], w[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Vector<Precision, 1>& y, const Vector<Precision, 1>& z, const Precision& w )
        : scalars{ x[0], y[0], z[0], w }
    {

    }

    constexpr Vector( const Precision x, const Vector<Precision, 1>& y, const Vector<Precision, 1>& z, const Vector<Precision, 1>& w )
        : scalars{ x, y[0], z[0], w[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 1>& x, const Vector<Precision, 1>& y, const Vector<Precision, 1>& z, const Vector<Precision, 1>& w )
        : scalars{ x[0], y[0], z[0], w[0] }
    {

    }

    constexpr Vector( const Vector<Precision, 2>& xy, const Precision& z, const Precision& w )
        : scalars{ xy[0], xy[1], z, w }
    {

    }

    constexpr Vector( const Precision& x, const Precision& y, const Vector<Precision, 2>& zw )
        : scalars{ x, y, zw[0], zw[1] }
    {

    }

    constexpr Vector( const Precision& x, const Vector<Precision, 2>& yz, const Precision w )
        : scalars{ x, yz[0], yz[1], w }
    {

    }

    constexpr Vector( const Vector<Precision, 3>& xyz, const Precision w )
        : scalars{ xyz[0], xyz[1], xyz[2], w }
    {

    }

    constexpr Vector( const Precision x, const Vector<Precision, 3>& yzw )
        : scalars{ x, yzw[0], yzw[1], yzw[2] }
    {

    }

    // Promotions
    template <typename PrecisionR>
    constexpr explicit Vector( const Vector<PrecisionR, SCALAR_COUNT>& r )
        : scalars{ (Precision)r.scalars[0],  ( Precision )r.scalars[1],  ( Precision )r.scalars[2],  ( Precision )r.scalars[3] }
    {

    }

    constexpr const Precision& operator[] ( const u32 index ) const
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index];
    }

    constexpr Precision& operator[] ( const u32 index )
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index];
    }

    constexpr Vector& operator+ () const
    {
        return *this;
    }

    constexpr Vector operator- () const
    {
        return Vector( -scalars[0], -scalars[1], -scalars[2], -scalars[3] );
    }

    constexpr Vector<Precision, 4>& operator+= ( const Vector<Precision, 4>& r )
    {
        scalars[0] += r.scalars[0];
        scalars[1] += r.scalars[1];
        scalars[2] += r.scalars[2];
        scalars[3] += r.scalars[3];

        return *this;
    }

    constexpr Vector<Precision, 4>& operator-= ( const Vector<Precision, 4>& r )
    {
        scalars[0] -= r.scalars[0];
        scalars[1] -= r.scalars[1];
        scalars[2] -= r.scalars[2];
        scalars[3] -= r.scalars[3];

        return *this;
    }

    constexpr Vector<Precision, 4>& operator*= ( const Vector<Precision, 4>& r )
    {
        scalars[0] *= r.scalars[0];
        scalars[1] *= r.scalars[1];
        scalars[2] *= r.scalars[2];
        scalars[3] *= r.scalars[3];

        return *this;
    }

    constexpr Vector<Precision, 4>& operator*= ( const Precision& r )
    {
        scalars[0] *= r;
        scalars[1] *= r;
        scalars[2] *= r;
        scalars[3] *= r;

        return *this;
    }

    constexpr Vector<Precision, 4>& operator/= ( const Vector<Precision, 4>& r )
    {
        scalars[0] /= r.scalars[0];
        scalars[1] /= r.scalars[1];
        scalars[2] /= r.scalars[2];
        scalars[3] /= r.scalars[3];

        return *this;
    }

    constexpr Vector<Precision, 4>& operator/= ( const Precision& r )
    {
        scalars[0] /= r;
        scalars[1] /= r;
        scalars[2] /= r;
        scalars[3] /= r;

        return *this;
    }

    constexpr bool operator== ( const Vector<Precision, 4>& r )
    {
        return scalars[0] == r.scalars[0] && scalars[1] == r.scalars[1] && scalars[2] == r.scalars[2] && scalars[3] == r.scalars[3];
    }

    constexpr bool operator!= ( const Vector<Precision, 4>& r )
    {
        return scalars[0] != r.scalars[0] || scalars[1] != r.scalars[1] || scalars[2] != r.scalars[2] || scalars[3] != r.scalars[3];
    }

    constexpr Vector<Precision, 4> normalize() const
    {
        Vector r = *this / length();

        return Vector<Precision, 4>( r.scalars[0], r.scalars[1], r.scalars[2], r.scalars[3] );
    }

    static constexpr Precision dot( const Vector<Precision, 4>& l, const Vector<Precision, 4>& r )
    {
        return l.scalars[0] * r.scalars[0] + l.scalars[1] * r.scalars[1] + l.scalars[2] * r.scalars[2] + l.scalars[3] * r.scalars[3];
    }

    constexpr Precision lengthSquared() const
    {
        return Vector::dot( *this, *this );
    }

    constexpr Precision length() const
    {
        return sqrt( lengthSquared() );
    }

    static constexpr Vector<Precision, 4> min( const Vector<Precision, 4>& l, const Vector<Precision, 4>& r )
    {
        Vector<Precision, 4> v;

        for ( i32 i = 0; i < 4; i++ )
            v[i] = ( l[i] < r[i] ) ? l[i] : r[i];

        return v;
    }

    static constexpr Vector<Precision, 4> max( const Vector<Precision, 4>& l, const Vector<Precision, 4>& r )
    {
        Vector<Precision, 4> v;

        for ( i32 i = 0; i < 4; i++ )
            v[i] = ( l[i] > r[i] ) ? l[i] : r[i];

        return v;
    }
};

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator + ( const Vector<Precision, ScalarCount>& l, Precision r )
{
    Vector<Precision, ScalarCount> v = l;
    v += r;
    return v;
}

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator + ( const Vector<Precision, ScalarCount>& l, const Vector<Precision, ScalarCount>& r )
{
    Vector<Precision, ScalarCount> v = l;
    v += r;
    return v;
}

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator - ( const Vector<Precision, ScalarCount>& l, Precision r )
{
    Vector<Precision, ScalarCount> v = l;
    v -= r;
    return v;
}

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator - ( const Vector<Precision, ScalarCount>& l, const Vector<Precision, ScalarCount>& r )
{
    Vector<Precision, ScalarCount> v = l;
    v -= r;
    return v;
}

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator * ( const Vector<Precision, ScalarCount>& l, Precision r )
{
    Vector<Precision, ScalarCount> v = l;
    v *= r;
    return v;
}

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator * ( const Vector<Precision, ScalarCount>& l, const Vector<Precision, ScalarCount>& r )
{
    Vector<Precision, ScalarCount> v = l;
    v *= r;
    return v;
}

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator / ( const Vector<Precision, ScalarCount>& l, Precision r )
{
    Vector<Precision, ScalarCount> v = l;
    v /= r;
    return v;
}

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator / ( Precision l, const Vector<Precision, ScalarCount>& r )
{
    Vector<Precision, ScalarCount> v = r;

    for ( i32 i = 0; i < ScalarCount; i++ )
        v[i] = l / r[i];

    return v;
}

template <typename Precision, i32 ScalarCount>
static constexpr Vector<Precision, ScalarCount> operator / ( const Vector<Precision, ScalarCount>& l, const Vector<Precision, ScalarCount>& r )
{
    Vector<Precision, ScalarCount> v = l;
    v /= r;
    return v;
}

namespace dk
{
    namespace maths
    {
        template <typename Precision, i32 ScalarCount>
        static constexpr Precision GetBiggestScalar( const Vector<Precision, ScalarCount>& r )
        {
            Precision biggestScalar = r[0];

            for ( i32 i = 1; i < ScalarCount; i++ )
                biggestScalar = Max( biggestScalar, r[i] );

            return biggestScalar;
        }

        template <typename Precision, i32 ScalarCount>
        static constexpr Vector<Precision, ScalarCount> degrees( const Vector<Precision, ScalarCount>& r )
        {
            Vector<Precision, ScalarCount> v = r;

            for ( i32 i = 0; i < ScalarCount; i++ )
                v[i] = dk::maths::degrees( r[i] );

            return v;
        }

        template <typename Precision, i32 ScalarCount>
        static constexpr Vector<Precision, ScalarCount> radians( const Vector<Precision, ScalarCount>& r )
        {
            Vector<Precision, ScalarCount> v = r;

            for ( i32 i = 0; i < ScalarCount; i++ )
                v[i] = dk::maths::radians( r[i] );

            return v;
        }
    }
}

#include "Vector.inl"

using dkVec1f = Vector<f32, 1>;
using dkVec2f = Vector<f32, 2>;
using dkVec3f = Vector<f32, 3>;
using dkVec4f = Vector<f32, 4>;

using dkVec1u = Vector<u32, 1>;
using dkVec2u = Vector<u32, 2>;
using dkVec3u = Vector<u32, 3>;
using dkVec4u = Vector<u32, 4>;

using dkVec1i = Vector<i32, 1>;
using dkVec2i = Vector<i32, 2>;
using dkVec3i = Vector<i32, 3>;
using dkVec4i = Vector<i32, 4>;

using dkVec1d = Vector<f64, 1>;
using dkVec2d = Vector<f64, 2>;
using dkVec3d = Vector<f64, 3>;
using dkVec4d = Vector<f64, 4>;
