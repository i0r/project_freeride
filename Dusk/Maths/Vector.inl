/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
template<typename Precision>
const Vector<Precision, 1> Vector<Precision, 1>::Zero = Vector( ( Precision )0 );

template<typename Precision>
const Vector<Precision, 2> Vector<Precision, 2>::Zero = Vector( ( Precision )0, ( Precision )0 );

template<typename Precision>
const Vector<Precision, 3> Vector<Precision, 3>::Zero = Vector( ( Precision )0, ( Precision )0, ( Precision )0 );

template<typename Precision>
const Vector<Precision, 4> Vector<Precision, 4>::Zero = Vector( ( Precision )0, ( Precision )0, ( Precision )0, ( Precision )0 );

template<typename Precision>
const Vector<Precision, 1> Vector<Precision, 1>::Max = Vector( std::numeric_limits<Precision>::max() );

template<typename Precision>
const Vector<Precision, 2> Vector<Precision, 2>::Max = Vector( std::numeric_limits<Precision>::max(), std::numeric_limits<Precision>::max() );

template<typename Precision>
const Vector<Precision, 3> Vector<Precision, 3>::Max = Vector( std::numeric_limits<Precision>::max(), std::numeric_limits<Precision>::max(), std::numeric_limits<Precision>::max() );

template<typename Precision>
const Vector<Precision, 4> Vector<Precision, 4>::Max = Vector( std::numeric_limits<Precision>::max(), std::numeric_limits<Precision>::max(), std::numeric_limits<Precision>::max(), std::numeric_limits<Precision>::max() );
