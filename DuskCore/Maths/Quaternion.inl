/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/


template<typename Precision>
const Quaternion<Precision> Quaternion<Precision>::Zero = Quaternion( ( Precision )0, ( Precision )0, ( Precision )0, ( Precision )0 );

template<typename Precision>
const Quaternion<Precision> Quaternion<Precision>::Identity = Quaternion( ( Precision )0, ( Precision )0, ( Precision )0, ( Precision )1 );
