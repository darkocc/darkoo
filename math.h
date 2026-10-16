#pragma once

namespace math {
    // pi constants.
    constexpr float pi   = 3.1415926535897932384f; // pi
    constexpr float pi_2 = pi * 2.f;               // pi * 2

    // degrees to radians.
    __forceinline constexpr float deg_to_rad( float val ) {
        return val * ( pi / 180.f );
    }

    // radians to degrees.
    __forceinline constexpr float rad_to_deg( float val ) {
        return val * ( 180.f / pi );
    }

    // angle mod ( shitty normalize ).
    __forceinline float AngleMod( float angle ) {
        return ( 360.f / 65536 ) * ( ( int )( angle * ( 65536.f / 360.f ) ) & 65535 );
    }

    void AngleMatrix( const ang_t& ang, const vec3_t& pos, matrix3x4_t& out );

    // normalizes an angle.
    void NormalizeAngle( float &angle );

    __forceinline float NormalizedAngle( float angle ) {
        NormalizeAngle( angle );
        return angle;
    }

    float AngleDiff(float destAngle, float srcAngle);
    float ApproachAngle( float target, float value, float speed );
    void  VectorAngles( const vec3_t& forward, ang_t& angles, vec3_t* up = nullptr );
    void  AngleVectors( const ang_t& angles, vec3_t* forward, vec3_t* right = nullptr, vec3_t* up = nullptr );
    float GetFOV( const ang_t &view_angles, const vec3_t &start, const vec3_t &end );
    void  VectorTransform( const vec3_t& in, const matrix3x4_t& matrix, vec3_t& out );
    void  VectorITransform( const vec3_t& in, const matrix3x4_t& matrix, vec3_t& out );
    void  MatrixAngles( const matrix3x4_t& matrix, ang_t& angles );
    void  MatrixCopy( const matrix3x4_t &in, matrix3x4_t &out );
    void  ConcatTransforms( const matrix3x4_t &in1, const matrix3x4_t &in2, matrix3x4_t &out );

	vec3_t Interpolate( const vec3_t from, const vec3_t to, const float percent );

    template < typename t >
    __forceinline void clamp( t& n, const t& lower, const t& upper ) {
        n = std::max( lower, std::min( n, upper ) );
    }
}