/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/

#pragma once

#include "Helpers.h"
#include "Vector.h"
#include "Matrix.h"

template<typename Precision>
struct Quaternion
{
    static constexpr i32                SCALAR_COUNT = 4;

    static const Quaternion             Zero;
    static const Quaternion             Identity;

    union
    {
        Precision                       scalars[SCALAR_COUNT];

        struct
        {
            union
            {
                Vector<Precision, 3>    axis;
                struct
                {
                    Precision           x;
                    Precision           y;
                    Precision           z;
                };
            };

            Precision                   w;
        };
    };

    // Constructors
    constexpr Quaternion() = default;

    constexpr Quaternion( const Precision x, const Precision y, const Precision z, const Precision w )
        : scalars{ x, y, z, w }
    {

    }

    constexpr Quaternion( const Vector<Precision, 3>& xyz, const Precision w )
        : scalars{ xyz[0], xyz[1], xyz[2], w }
    {

    }

    constexpr Quaternion( const Vector<Precision, 3>& eulerAngles )
    {
        Vector<Precision, 3> c = dkVec3f::cos( dk::maths::radians<Precision>( eulerAngles ) * Precision( 0.5 ) );
        Vector<Precision, 3> s = dkVec3f::sin( dk::maths::radians<Precision>( eulerAngles ) * Precision( 0.5 ) );

        w = c.x * c.y * c.z + s.x * s.y * s.z;
        x = s.x * c.y * c.z - c.x * s.y * s.z;
        y = c.x * s.y * c.z + s.x * c.y * s.z;
        z = c.x * c.y * s.z - s.x * s.y * c.z;
    }

    constexpr Quaternion( const Matrix<Precision, 4, 4>& rotationMatrix )
    {
        f32 r11 = rotationMatrix._00;
        f32 r12 = rotationMatrix._01;
        f32 r13 = rotationMatrix._02;
        f32 r21 = rotationMatrix._10;
        f32 r22 = rotationMatrix._11;
        f32 r23 = rotationMatrix._12;
        f32 r31 = rotationMatrix._20;
        f32 r32 = rotationMatrix._21;
        f32 r33 = rotationMatrix._22;

        f32 q0 = ( r11 + r22 + r33 + 1.0f ) / 4.0f;
        f32 q1 = ( r11 - r22 - r33 + 1.0f ) / 4.0f;
        f32 q2 = ( -r11 + r22 - r33 + 1.0f ) / 4.0f;
        f32 q3 = ( -r11 - r22 + r33 + 1.0f ) / 4.0f;

        if ( q0 < 0.0f ) {
            q0 = 0.0f;
        }
        if ( q1 < 0.0f ) {
            q1 = 0.0f;
        }
        if ( q2 < 0.0f ) {
            q2 = 0.0f;
        }
        if ( q3 < 0.0f ) {
            q3 = 0.0f;
        }
        q0 = sqrt( q0 );
        q1 = sqrt( q1 );
        q2 = sqrt( q2 );
        q3 = sqrt( q3 );
        if ( q0 >= q1 && q0 >= q2 && q0 >= q3 ) {
            q0 *= +1.0f;
            q1 *= dk::maths::sign( r32 - r23 );
            q2 *= dk::maths::sign( r13 - r31 );
            q3 *= dk::maths::sign( r21 - r12 );
        } else if ( q1 >= q0 && q1 >= q2 && q1 >= q3 ) {
            q0 *= dk::maths::sign( r32 - r23 );
            q1 *= +1.0f;
            q2 *= dk::maths::sign( r21 + r12 );
            q3 *= dk::maths::sign( r13 + r31 );
        } else if ( q2 >= q0 && q2 >= q1 && q2 >= q3 ) {
            q0 *= dk::maths::sign( r13 - r31 );
            q1 *= dk::maths::sign( r21 + r12 );
            q2 *= +1.0f;
            q3 *= dk::maths::sign( r32 + r23 );
        } else if ( q3 >= q0 && q3 >= q1 && q3 >= q2 ) {
            q0 *= dk::maths::sign( r21 - r12 );
            q1 *= dk::maths::sign( r31 + r13 );
            q2 *= dk::maths::sign( r32 + r23 );
            q3 *= +1.0f;
        }

        f32 r = sqrt( q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3 );
        q0 /= r;
        q1 /= r;
        q2 /= r;
        q3 /= r;

        w = q0;
        x = q1;
        y = q2;
        z = q3;
    }

    // Promotions
    template <typename PrecisionR>
    constexpr explicit Quaternion( const Quaternion<PrecisionR>& r )
        : scalars{ ( Precision )r.scalars[0],  ( Precision )r.scalars[1],  ( Precision )r.scalars[2],  ( Precision )r.scalars[3] }
    {

    }

    constexpr Precision& operator[] ( const u32 index ) const
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index];
    }

    constexpr Precision& operator[] ( const u32 index )
    {
        DUSK_DEV_ASSERT( index < SCALAR_COUNT, "Invalid scalar index provided! (%i >= %i)", index, SCALAR_COUNT );

        return scalars[index];
    }

    constexpr Quaternion& operator+ () const
    {
        return *this;
    }

    constexpr Quaternion operator- () const
    {
        return Quaternion( -scalars[0], -scalars[1], -scalars[2], -scalars[3] );
    }

    constexpr Quaternion<Precision>& operator+= ( const Quaternion<Precision>& r )
    {
        scalars[0] += r.scalars[0];
        scalars[1] += r.scalars[1];
        scalars[2] += r.scalars[2];
        scalars[3] += r.scalars[3];

        return *this;
    }

    constexpr Quaternion<Precision>& operator-= ( const Quaternion<Precision>& r )
    {
        scalars[0] -= r.scalars[0];
        scalars[1] -= r.scalars[1];
        scalars[2] -= r.scalars[2];
        scalars[3] -= r.scalars[3];

        return *this;
    }

    constexpr Quaternion<Precision>& operator*= ( const Quaternion<Precision>& r )
    {
        scalars[0] *= r.scalars[0];
        scalars[1] *= r.scalars[1];
        scalars[2] *= r.scalars[2];
        scalars[3] *= r.scalars[3];

        return *this;
    }

    constexpr Quaternion<Precision>& operator*= ( const Precision& r )
    {
        scalars[0] *= r;
        scalars[1] *= r;
        scalars[2] *= r;
        scalars[3] *= r;

        return *this;
    }

    constexpr Quaternion<Precision>& operator/= ( const Precision& r )
    {
        scalars[0] /= r;
        scalars[1] /= r;
        scalars[2] /= r;
        scalars[3] /= r;

        return *this;
    }

    constexpr bool operator== ( const Quaternion<Precision>& r )
    {
        return scalars[0] == r.scalars[0] && scalars[1] == r.scalars[1] && scalars[2] == r.scalars[2] && scalars[3] == r.scalars[3];
    }

    constexpr bool operator!= ( const Quaternion<Precision>& r )
    {
        return scalars[0] != r.scalars[0] || scalars[1] != r.scalars[1] || scalars[2] != r.scalars[2] || scalars[3] != r.scalars[3];
    }

    constexpr Quaternion<Precision> normalize() const
    {
        Quaternion r = *this / length();

        return Quaternion<Precision>( r.scalars[0], r.scalars[1], r.scalars[2], r.scalars[3] );
    }

    constexpr Quaternion<Precision> conjugate() const
    {
        return Quaternion<Precision>( -scalars[0], -scalars[1], -scalars[2], scalars[3] );
    }

    constexpr Quaternion<Precision> inverse() const
    {
        return Quaternion( conjugate() * ( (Precision)1 / lengthSquared() ) );
    }

    constexpr Precision lengthSquared() const
    {
        return scalars[0] * scalars[0] + scalars[1] * scalars[1] + scalars[2] * scalars[2] + scalars[3] * scalars[3];
    }

    constexpr Precision length() const
    {
        return sqrt( lengthSquared() );
    }

    constexpr Matrix<Precision, 3, 3> toMat3x3() const
    {
        Precision twoX( x + x );
        Precision twoY( y + y );
        Precision twoZ( z + z );

        Precision xx2( x * twoX );
        Precision xy2( x * twoY );
        Precision xz2( x * twoZ );
        Precision yy2( y * twoY );
        Precision yz2( y * twoZ );
        Precision zz2( z * twoZ );

        Precision wx2( w * twoX );
        Precision wy2( w * twoY );
        Precision wz2( w * twoZ );

        return Matrix<Precision, 3, 3>(
            ( Precision )1 - yy2 - zz2, xy2 + wz2, xz2 - wy2,
            xy2 - wz2, ( Precision )1 - xx2 - zz2, yz2 + wx2,
            xz2 + wy2, yz2 - wx2, ( Precision )1 - xx2 - yy2 );
    }

    constexpr Matrix<Precision, 4, 4> toMat4x4() const
    {
        Matrix<Precision, 3, 3> mat33 = toMat3x3();

        return Matrix<Precision, 4, 4>(
            mat33[0][0], mat33[0][1], mat33[0][2], ( Precision )0,
            mat33[1][0], mat33[1][1], mat33[1][2], ( Precision )0,
            mat33[2][0], mat33[2][1], mat33[2][2], ( Precision )0,
            (Precision)0, ( Precision )0, ( Precision )0, ( Precision )1 );
    }

    constexpr Vector<Precision, 3> toEulerAngles() const
    {
        return Vector <Precision, 3>(
            dk::maths::degrees<Precision>( atan2( static_cast<Precision>( 2 ) * ( y * z + w * x ), w * w - x * x - y * y + z * z ) ),
            dk::maths::degrees<Precision>( asin( dk::maths::clamp( static_cast<Precision>( -2 ) * ( x * z - w * y ), static_cast<Precision>( -1 ), static_cast<Precision>( 1 ) ) ) ),
            dk::maths::degrees<Precision>( static_cast<Precision>( atan2( static_cast<Precision>( 2 ) * ( x * y + w * z ), w * w + x * x - y * y - z * z ) ) )
        );
    }

    constexpr Quaternion<Precision> rotate( const Precision angleInRadians, const Vector<Precision, 3>& rotationAxis )
    {
        // TODO We could assume that the rotation axis is always normalize and save us a call...
        Vector<Precision, 3> normalizedRotationAxis = rotationAxis.normalize();
        normalizedRotationAxis *= sin( angleInRadians * static_cast< Precision >( 0.5 ) );

        return Quaternion<Precision>( normalizedRotationAxis, cos( angleInRadians * static_cast< Precision >( 0.5 ) ) ) * *this;
    }
};

template<typename Precision>
static constexpr Quaternion<Precision> operator / ( const Quaternion<Precision>& l, const Precision& r )
{
    Quaternion<Precision> q = l;
    q /= r;

    return q;
}

template<typename Precision>
static constexpr Quaternion<Precision> operator * ( const Quaternion<Precision>& l, const Precision& r )
{
    Quaternion<Precision> q = l;
    q *= r;

    return q;
}

template<typename Precision>
static constexpr Quaternion<Precision> operator * ( const Precision& l, const Quaternion<Precision>& r )
{
    Quaternion<Precision> q = r;
    q *= l;

    return q;
}

template<typename Precision>
static constexpr Quaternion<Precision> operator * ( const Quaternion<Precision>& l, const Quaternion<Precision>& r )
{
    return Quaternion<Precision>(
        l.scalars[3] * r.scalars[0] + l.scalars[0] * r.scalars[3] + l.scalars[1] * r.scalars[2] - l.scalars[2] * r.scalars[1],
        l.scalars[3] * r.scalars[1] + l.scalars[1] * r.scalars[3] + l.scalars[2] * r.scalars[0] - l.scalars[0] * r.scalars[2],
        l.scalars[3] * r.scalars[2] + l.scalars[2] * r.scalars[3] + l.scalars[0] * r.scalars[1] - l.scalars[1] * r.scalars[0],
        l.scalars[3] * r.scalars[3] - l.scalars[0] * r.scalars[0] - l.scalars[1] * r.scalars[1] - l.scalars[2] * r.scalars[2] );
}

template<typename Precision>
static constexpr Quaternion<Precision> operator + ( const Quaternion<Precision>& l, const Quaternion<Precision>& r )
{
    Quaternion<Precision> q = l;
    q += r;

    return q;
}

template<typename Precision>
static constexpr Quaternion<Precision> operator - ( const Quaternion<Precision>& l, const Quaternion<Precision>& r )
{
    Quaternion<Precision> q = l;
    q -= r;

    return q;
}

#include "Quaternion.inl"

using dkQuatf = Quaternion<f32>;
using dkQuatd = Quaternion<f64>;
