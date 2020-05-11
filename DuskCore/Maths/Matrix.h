/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/

#pragma once

#include "Vector.h"

template <typename Precision, i32 RowCount, i32 ColumnCount>
struct Matrix;

template<typename Precision>
struct Matrix<Precision, 4, 4>
{
    static constexpr i32        ROW_COUNT       = 4;
    static constexpr i32        COLUMN_COUNT    = 4;
    static constexpr i32        SCALAR_COUNT    = ( ROW_COUNT * COLUMN_COUNT );

    static const Matrix         Zero;
    static const Matrix         Identity;

    union
    {
        struct {
            Precision   _00, _10, _20, _30,
                        _01, _11, _21, _31,
                        _02, _12, _22, _32,
                        _03, _13, _23, _33;
        };

        Vector<Vector<Precision, COLUMN_COUNT>, ROW_COUNT> rows;
    };

    // Constructors
    constexpr Matrix() = default;

    // Fill diagonal with x
    constexpr Matrix( const Precision x ) 
        : rows( { x, 0, 0, 0 }, 
                { 0, x, 0, 0 }, 
                { 0, 0, x, 0 }, 
                { 0, 0, 0, x } )
    {

    }

    constexpr Matrix(
        const Precision& m00, const Precision& m10, const Precision& m20, const Precision& m30,
        const Precision& m01, const Precision& m11, const Precision& m21, const Precision& m31,
        const Precision& m02, const Precision& m12, const Precision& m22, const Precision& m32,
        const Precision& m03, const Precision& m13, const Precision& m23, const Precision& m33 ) noexcept
        : rows( { m00, m10, m20, m30 }, 
                { m01, m11, m21, m31 }, 
                { m02, m12, m22, m32 }, 
                { m03, m13, m23, m33 } )
    {

    }

    constexpr Matrix( const Vector<Precision, COLUMN_COUNT>& c0,
                      const Vector<Precision, COLUMN_COUNT>& c1,
                      const Vector<Precision, COLUMN_COUNT>& c2,
                      const Vector<Precision, COLUMN_COUNT>& c3 ) 
        : rows( c0, c1, c2, c3 ) 
    {

    }

    constexpr Matrix( const Matrix<Precision, 3, 3>& m ) noexcept 
        : rows{ { m.rows[0][0], m.rows[0][1], m.rows[0][2], 0 },
             { m.rows[1][0], m.rows[1][1], m.rows[1][2], 0 },
             { m.rows[2][0], m.rows[2][1], m.rows[2][2], 0 },
             { (Precision)0, ( Precision )0, ( Precision )0, ( Precision )1 } } 
    {

    }

    template<typename PrecisionR>
    constexpr explicit Matrix( const Matrix<PrecisionR, ROW_COUNT, COLUMN_COUNT>& r ) noexcept 
        : rows( r.v ) 
    {

    }

    constexpr Precision* toArray()
    {
        return &_00;
    }

    constexpr const Vector<Precision, COLUMN_COUNT>& operator[] ( const u32 index ) const
    {
        return rows[index];
    }

    constexpr Vector<Precision, COLUMN_COUNT>& operator[] ( const u32 index )
    {
        return rows[index];
    }

    constexpr Precision& operator[] ( const Vector<i32, 2> index2d ) const
    {
        return rows[index2d.y][index2d.x];
    }

    constexpr Precision& operator[] ( const Vector<i32, 2> index2d )
    {
        return rows[index2d.y][index2d.x];
    }

    constexpr Matrix operator + () const
    {
        return *this;
    }

    constexpr Matrix operator - () const
    {
        return Matrix( -rows );
    }

    constexpr Matrix<Precision, 3, 3> toMat3x3() const noexcept 
    { 
        return Matrix<Precision, 3, 3>( rows[0][0], rows[0][1], rows[0][2], rows[1][0], rows[1][1], rows[1][2], rows[2][0], rows[2][1], rows[2][2] ); 
    }

    constexpr Matrix transpose() const
    {
        return Matrix(
            rows[0][0], rows[1][0], rows[2][0], rows[3][0],
            rows[0][1], rows[1][1], rows[2][1], rows[3][1],
            rows[0][2], rows[1][2], rows[2][2], rows[3][2],
            rows[0][3], rows[1][3], rows[2][3], rows[3][3] );
    }

    constexpr Precision determinant() const noexcept
    {
        Precision m11 = rows[0][0], m21 = rows[1][0], m31 = rows[2][0], m41 = rows[3][0],
                  m12 = rows[0][1], m22 = rows[1][1], m32 = rows[2][1], m42 = rows[3][1],
                  m13 = rows[0][2], m23 = rows[1][2], m33 = rows[2][2], m43 = rows[3][2],
                  m14 = rows[0][3], m24 = rows[1][3], m34 = rows[2][3], m44 = rows[3][3];

        Precision _1122_2112 = m11 * m22 - m21 * m12,
                  _1132_3112 = m11 * m32 - m31 * m12,
                  _1142_4112 = m11 * m42 - m41 * m12,
                  _2132_3122 = m21 * m32 - m31 * m22,
                  _2142_4122 = m21 * m42 - m41 * m22,
                  _3142_4132 = m31 * m42 - m41 * m32,
                  _1324_2314 = m13 * m24 - m23 * m14,
                  _1334_3314 = m13 * m34 - m33 * m14,
                  _1344_4314 = m13 * m44 - m43 * m14,
                  _2334_3324 = m23 * m34 - m33 * m24,
                  _2344_4324 = m23 * m44 - m43 * m24,
                  _3344_4334 = m33 * m44 - m43 * m34;

        return ( + _1122_2112 * _3344_4334
                 - _1132_3112 * _2344_4324
                 + _1142_4112 * _2334_3324
                 + _2132_3122 * _1344_4314
                 - _2142_4122 * _1334_3314
                 + _3142_4132 * _1324_2314 );
    }

    constexpr Matrix inverse() const
    {
        Precision m11 = rows[0][0], m21 = rows[1][0], m31 = rows[2][0], m41 = rows[3][0],
                  m12 = rows[0][1], m22 = rows[1][1], m32 = rows[2][1], m42 = rows[3][1],
                  m13 = rows[0][2], m23 = rows[1][2], m33 = rows[2][2], m43 = rows[3][2],
                  m14 = rows[0][3], m24 = rows[1][3], m34 = rows[2][3], m44 = rows[3][3];

        Precision _1122_2112 = m11 * m22 - m21 * m12,
                  _1132_3112 = m11 * m32 - m31 * m12,
                  _1142_4112 = m11 * m42 - m41 * m12,
                  _2132_3122 = m21 * m32 - m31 * m22,
                  _2142_4122 = m21 * m42 - m41 * m22,
                  _3142_4132 = m31 * m42 - m41 * m32,
                  _1324_2314 = m13 * m24 - m23 * m14,
                  _1334_3314 = m13 * m34 - m33 * m14,
                  _1344_4314 = m13 * m44 - m43 * m14,
                  _2334_3324 = m23 * m34 - m33 * m24,
                  _2344_4324 = m23 * m44 - m43 * m24,
                  _3344_4334 = m33 * m44 - m43 * m34;

        Precision det = ( +_1122_2112 * _3344_4334
                        - _1132_3112 * _2344_4324
                        + _1142_4112 * _2334_3324
                        + _2132_3122 * _1344_4314
                        - _2142_4122 * _1334_3314
                        + _3142_4132 * _1324_2314 );

        if ( det == ( Precision )0 ) {
            return Matrix::Identity;
        }

        Precision invDet = (Precision)1 / det;

        return Matrix(
            ( m22 * _3344_4334 - m32 * _2344_4324 + m42 * _2334_3324 ) * invDet,
            ( -m21 * _3344_4334 + m31 * _2344_4324 - m41 * _2334_3324 ) * invDet,
            ( m24 * _3142_4132 - m34 * _2142_4122 + m44 * _2132_3122 ) * invDet,
            ( -m23 * _3142_4132 + m33 * _2142_4122 - m43 * _2132_3122 ) * invDet,
            ( -m12 * _3344_4334 + m32 * _1344_4314 - m42 * _1334_3314 ) * invDet,
            ( m11 * _3344_4334 - m31 * _1344_4314 + m41 * _1334_3314 ) * invDet,
            ( -m14 * _3142_4132 + m34 * _1142_4112 - m44 * _1132_3112 ) * invDet,
            ( m13 * _3142_4132 - m33 * _1142_4112 + m43 * _1132_3112 ) * invDet,
            ( m12 * _2344_4324 - m22 * _1344_4314 + m42 * _1324_2314 ) * invDet,
            ( -m11 * _2344_4324 + m21 * _1344_4314 - m41 * _1324_2314 ) * invDet,
            ( m14 * _2142_4122 - m24 * _1142_4112 + m44 * _1122_2112 ) * invDet,
            ( -m13 * _2142_4122 + m23 * _1142_4112 - m43 * _1122_2112 ) * invDet,
            ( -m12 * _2334_3324 + m22 * _1334_3314 - m32 * _1324_2314 ) * invDet,
            ( m11 * _2334_3324 - m21 * _1334_3314 + m31 * _1324_2314 ) * invDet,
            ( -m14 * _2132_3122 + m24 * _1132_3112 - m34 * _1122_2112 ) * invDet,
            ( m13 * _2132_3122 - m23 * _1132_3112 + m33 * _1122_2112 ) * invDet );
    }
};

template<typename Precision>
static constexpr Matrix<Precision, 4, 4> operator * ( const Matrix<Precision, 4, 4>& l, const Matrix<Precision, 4, 4>& r )
{
    return Matrix<Precision, 4, 4>(
        r[0][0] * l[0][0] + r[0][1] * l[1][0] + r[0][2] * l[2][0] + r[0][3] * l[3][0],
        r[0][0] * l[0][1] + r[0][1] * l[1][1] + r[0][2] * l[2][1] + r[0][3] * l[3][1],
        r[0][0] * l[0][2] + r[0][1] * l[1][2] + r[0][2] * l[2][2] + r[0][3] * l[3][2],
        r[0][0] * l[0][3] + r[0][1] * l[1][3] + r[0][2] * l[2][3] + r[0][3] * l[3][3],
        r[1][0] * l[0][0] + r[1][1] * l[1][0] + r[1][2] * l[2][0] + r[1][3] * l[3][0],
        r[1][0] * l[0][1] + r[1][1] * l[1][1] + r[1][2] * l[2][1] + r[1][3] * l[3][1],
        r[1][0] * l[0][2] + r[1][1] * l[1][2] + r[1][2] * l[2][2] + r[1][3] * l[3][2],
        r[1][0] * l[0][3] + r[1][1] * l[1][3] + r[1][2] * l[2][3] + r[1][3] * l[3][3],
        r[2][0] * l[0][0] + r[2][1] * l[1][0] + r[2][2] * l[2][0] + r[2][3] * l[3][0],
        r[2][0] * l[0][1] + r[2][1] * l[1][1] + r[2][2] * l[2][1] + r[2][3] * l[3][1],
        r[2][0] * l[0][2] + r[2][1] * l[1][2] + r[2][2] * l[2][2] + r[2][3] * l[3][2],
        r[2][0] * l[0][3] + r[2][1] * l[1][3] + r[2][2] * l[2][3] + r[2][3] * l[3][3],
        r[3][0] * l[0][0] + r[3][1] * l[1][0] + r[3][2] * l[2][0] + r[3][3] * l[3][0],
        r[3][0] * l[0][1] + r[3][1] * l[1][1] + r[3][2] * l[2][1] + r[3][3] * l[3][1],
        r[3][0] * l[0][2] + r[3][1] * l[1][2] + r[3][2] * l[2][2] + r[3][3] * l[3][2],
        r[3][0] * l[0][3] + r[3][1] * l[1][3] + r[3][2] * l[2][3] + r[3][3] * l[3][3] );
}

namespace dk
{
    namespace maths
    {
        template<typename Precision>
        static constexpr Vector<Precision, 3> ExtractTranslation( const Matrix<Precision, 4, 4>& l )
        {
            return Vector<Precision, 3>( l[3].x, l[3].y, l[3].z );
        }

        template<typename Precision>
        static constexpr Vector<Precision, 3> ExtractScale( const Matrix<Precision, 4, 4>& l )
        {
            Precision xScale = Vector<Precision, 3>( l[0] ).length();
            Precision yScale = Vector<Precision, 3>( l[1] ).length();
            Precision zScale = Vector<Precision, 3>( l[2] ).length();

            return Vector<Precision, 3>( xScale, yScale, zScale );
        }

        template<typename Precision>
        static constexpr Matrix<Precision,4,4> ExtractRotation( const Matrix<Precision, 4, 4>& l, const Vector<Precision, 3>& scale )
        {
            Matrix<Precision, 4, 4> rotationMatrix = Matrix<Precision, 4, 4>::Identity;
            rotationMatrix._00 = l._00 / scale.x;
            rotationMatrix._10 = l._10 / scale.x;
            rotationMatrix._20 = l._20 / scale.x;

            rotationMatrix._01 = l._01 / scale.y;
            rotationMatrix._11 = l._11 / scale.y;
            rotationMatrix._21 = l._21 / scale.y;

            rotationMatrix._02 = l._02 / scale.z;
            rotationMatrix._12 = l._12 / scale.z;
            rotationMatrix._22 = l._22 / scale.z;

            return rotationMatrix;
        }
    }
}

template <typename Precision, i32 RowCount, i32 ColumnCount>
static constexpr Matrix<Precision, RowCount, ColumnCount> operator + ( const Matrix<Precision, RowCount, ColumnCount>& l, const Matrix<Precision, RowCount, ColumnCount>& r )
{
    return Matrix<Precision, RowCount, ColumnCount>( l.rows + r.rows );
}

template <typename Precision, i32 RowCount, i32 ColumnCount>
static constexpr Matrix<Precision, RowCount, ColumnCount> operator * ( const Precision l, const Matrix<Precision, RowCount, ColumnCount>& r )
{
    return Matrix<Precision, RowCount, ColumnCount>( Vector<Precision, ColumnCount>( l ) * r.rows );
}

template <typename Precision, i32 RowCount, i32 ColumnCount>
static constexpr Matrix<Precision, RowCount, ColumnCount> operator * ( const Matrix<Precision, RowCount, ColumnCount>& l, Precision r )
{
    return Matrix<Precision, RowCount, ColumnCount>( l.rows * Vector<Precision, ColumnCount>( r ) );
}

template <typename Precision, i32 RowCount, i32 ColumnCount>
static constexpr Matrix<Precision, RowCount, ColumnCount> operator / ( const Matrix<Precision, RowCount, ColumnCount>& l, Precision r )
{
    return Matrix<Precision, RowCount, ColumnCount>( l.rows * Vector<Precision, ColumnCount>( (Precision)1 / r ) );
}

template <typename Precision, i32 RowCount, i32 ColumnCount>
static constexpr Matrix<Precision, RowCount, ColumnCount> operator - ( const Matrix<Precision, RowCount, ColumnCount>& l, const Matrix<Precision, RowCount, ColumnCount>& r )
{
    return Matrix<Precision, RowCount, ColumnCount>( l.rows - r.rows );
}

template <typename Precision, i32 RowCount, i32 ColumnCount>
static constexpr Matrix<Precision, RowCount, ColumnCount> operator == ( const Matrix<Precision, RowCount, ColumnCount>& l, const Matrix<Precision, RowCount, ColumnCount>& r )
{
    return l.rows == r.rows;
}

template <typename Precision, i32 RowCount, i32 ColumnCount>
static constexpr Matrix<Precision, RowCount, ColumnCount> operator != ( const Matrix<Precision, RowCount, ColumnCount>& l, const Matrix<Precision, RowCount, ColumnCount>& r )
{
    return l.rows != r.rows;
}

#include "Matrix.inl"

using dkMat4x4f = Matrix<f32, 4, 4>;
using dkMat3x3f = Matrix<f32, 3, 3>;
