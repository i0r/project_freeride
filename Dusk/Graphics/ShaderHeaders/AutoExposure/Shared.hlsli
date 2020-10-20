#ifndef __AUTOEXPOSURE_H__
#define __AUTOEXPOSURE_H__ 1
static const float	BISOU_TO_WORLD_LUMINANCE = 139.26;
static const float	WORLD_TO_BISOU_LUMINANCE = 1.0 / BISOU_TO_WORLD_LUMINANCE;

static const float	MIN_ADAPTABLE_SCENE_LUMINANCE = 1e-2;
static const float	MAX_ADAPTABLE_SCENE_LUMINANCE = 1e4;
static const float	SCENE_LUMINANCE_RANGE_DB = 120.0;
static const float	MIN_ADAPTABLE_SCENE_LUMINANCE_DB = -40;
static const float	MAX_ADAPTABLE_SCENE_LUMINANCE_DB = 80;

static const uint	TARGET_MONITOR_BITS_PRECISION = 8;
static const float	TARGET_MONITOR_LUMINANCE_RANGE = ( 1 << TARGET_MONITOR_BITS_PRECISION ) - 1;
static const float	TARGET_MONITOR_LUMINANCE_RANGE_DB = 48.164799306236991234198223155919;

static const uint	HISTOGRAM_BUCKETS_COUNT = 128;
static const float	HISTOGRAM_BUCKET_RANGE_DB = SCENE_LUMINANCE_RANGE_DB / HISTOGRAM_BUCKETS_COUNT;
static const float	ABSOLUTE_EV_REFERENCE_LUMINANCE = 0.15;

float Luminance2dB( float _Luminance )
{
    return 8.6858896380650365530225783783321 * log( _Luminance );
}

float dB2Luminance( float _dB )
{
    return pow( 10.0, 0.05 * _dB );
}

float Luminance2HistogramBucketIndex( float _Luminance )
{
    float dB = Luminance2dB( _Luminance );
    return ( dB - MIN_ADAPTABLE_SCENE_LUMINANCE_DB ) * ( 1.0 / HISTOGRAM_BUCKET_RANGE_DB );
}

float HistogramBucketIndex2Luminance( float _BucketIndex )
{
    float dB = MIN_ADAPTABLE_SCENE_LUMINANCE_DB + _BucketIndex * HISTOGRAM_BUCKET_RANGE_DB;
    return dB2Luminance( dB );
}

struct AutoExposureInfos
{
    float	EngineLuminanceFactor;
    float	TargetLuminance;
    float	MinLuminanceLDR;
    float	MaxLuminanceLDR;
    float	MiddleGreyLuminanceLDR;
    float	EV;
    float	Fstop;
    uint	PeakHistogramValue;
};
#endif
