#include "includes.h"

void math::AngleMatrix( const ang_t& ang, const vec3_t& pos, matrix3x4_t& out ) {
    g_csgo.AngleMatrix( ang, out );
    out.SetOrigin( pos );
}

float math::AngleDiff(float destAngle, float srcAngle) {
    float delta;

    delta = fmodf(destAngle - srcAngle, 360.0f);
    if (destAngle > srcAngle)
    {
        if (delta >= 180)
            delta -= 360;
    }
    else
    {
        if (delta <= -180)
            delta += 360;
    }

    return delta;
}

void math::NormalizeAngle( float &angle ) {
    float rot;

    // bad number.
    if( !std::isfinite( angle ) ) {
        angle = 0.f;
        return;
    }

    // no need to normalize this angle.
    if( angle >= -180.f && angle <= 180.f )
        return;

    // get amount of rotations needed.
    rot = std::round( std::abs( angle / 360.f ) );

    // normalize.
    angle = ( angle < 0.f ) ? angle + ( 360.f * rot ) : angle - ( 360.f * rot );
}

float math::ApproachAngle( float target, float value, float speed ) {
    float delta;

    target = AngleMod( target );
    value  = AngleMod( value );
    delta  = target - value;

    // speed is assumed to be positive.
    speed = std::abs( speed );

    math::NormalizeAngle( delta );

    if( delta > speed )
        value += speed;

    else if( delta < -speed )
        value -= speed;

    else
        value = target;

    return value;
}

void math::VectorAngles( const vec3_t& forward, ang_t& angles, vec3_t *up ) {
    vec3_t  left;
    float   len, up_z, pitch, yaw, roll;

    // get 2d length.
    len = forward.length_2d( );

    if( up && len > 0.001f ) {
        pitch = rad_to_deg( std::atan2( -forward.z, len ) );
        yaw   = rad_to_deg( std::atan2( forward.y, forward.x ) );

        // get left direction vector using cross product.
        left = ( *up ).cross( forward ).normalized( );

        // calculate up_z.
        up_z = ( left.y * forward.x ) - ( left.x * forward.y );

        // calculate roll.
        roll = rad_to_deg( std::atan2( left.z, up_z ) );
    }

    else {
        if( len > 0.f ) {
            // calculate pitch and yaw.
            pitch = rad_to_deg( std::atan2( -forward.z, len ) );
            yaw   = rad_to_deg( std::atan2( forward.y, forward.x ) );
            roll  = 0.f;
        }

        else {
            pitch = ( forward.z > 0.f ) ? -90.f : 90.f;
            yaw   = 0.f;
            roll  = 0.f;
        }
    }

    // set out angles.
    angles = { pitch, yaw, roll };
}

void math::AngleVectors( const ang_t& angles, vec3_t* forward, vec3_t* right, vec3_t* up ) {
    float cp = std::cos( deg_to_rad( angles.x ) ), sp = std::sin( deg_to_rad( angles.x ) );
    float cy = std::cos( deg_to_rad( angles.y ) ), sy = std::sin( deg_to_rad( angles.y ) );
    float cr = std::cos( deg_to_rad( angles.z ) ), sr = std::sin( deg_to_rad( angles.z ) );

	if( forward ) {
		forward->x = cp * cy;
		forward->y = cp * sy;
		forward->z = -sp;
	}

    if( right ) {
        right->x = -1.f * sr * sp * cy + -1.f * cr * -sy;
        right->y = -1.f * sr * sp * sy + -1.f * cr * cy;
        right->z = -1.f * sr * cp;
    }

    if( up ) {
        up->x = cr * sp * cy + -sr * -sy;
        up->y = cr * sp * sy + -sr * cy;
        up->z = cr * cp;
    }
}

float math::GetFOV( const ang_t &view_angles, const vec3_t &start, const vec3_t &end ) {
    vec3_t dir, fw;

    // get direction and normalize.
    dir = ( end - start ).normalized( );

    // get the forward direction vector of the view angles.
    AngleVectors( view_angles, &fw );

    // get the angle between the view angles forward directional vector and the target location.
    return std::max( rad_to_deg( std::acos( fw.dot( dir ) ) ), 0.f );
}

void math::VectorTransform( const vec3_t& in, const matrix3x4_t& matrix, vec3_t& out ) {
    out = {
        in.dot( vec3_t( matrix[ 0 ][ 0 ], matrix[ 0 ][ 1 ], matrix[ 0 ][ 2 ] ) ) + matrix[ 0 ][ 3 ],
        in.dot( vec3_t( matrix[ 1 ][ 0 ], matrix[ 1 ][ 1 ], matrix[ 1 ][ 2 ] ) ) + matrix[ 1 ][ 3 ],
        in.dot( vec3_t( matrix[ 2 ][ 0 ], matrix[ 2 ][ 1 ], matrix[ 2 ][ 2 ] ) ) + matrix[ 2 ][ 3 ]
    };
}

void math::VectorITransform( const vec3_t &in, const matrix3x4_t &matrix, vec3_t &out ) {
    vec3_t diff;

    diff = { 
        in.x - matrix[ 0 ][ 3 ], 
        in.y - matrix[ 1 ][ 3 ], 
        in.z - matrix[ 2 ][ 3 ] 
    };

    out = {
        diff.x * matrix[ 0 ][ 0 ] + diff.y * matrix[ 1 ][ 0 ] + diff.z * matrix[ 2 ][ 0 ],
        diff.x * matrix[ 0 ][ 1 ] + diff.y * matrix[ 1 ][ 1 ] + diff.z * matrix[ 2 ][ 1 ],
        diff.x * matrix[ 0 ][ 2 ] + diff.y * matrix[ 1 ][ 2 ] + diff.z * matrix[ 2 ][ 2 ]
    };
}

void math::MatrixAngles( const matrix3x4_t& matrix, ang_t& angles ) {
    vec3_t forward, left, up;

    // extract the basis vectors from the matrix. since we only need the z
    // component of the up vector, we don't get x and y.
    forward = { matrix[ 0 ][ 0 ], matrix[ 1 ][ 0 ], matrix[ 2 ][ 0 ] };
    left    = { matrix[ 0 ][ 1 ], matrix[ 1 ][ 1 ], matrix[ 2 ][ 1 ] };
    up      = { 0.f, 0.f, matrix[ 2 ][ 2 ] };

    float len = forward.length_2d( );

    // enough here to get angles?
    if( len > 0.001f ) {
        angles.x = rad_to_deg( std::atan2( -forward.z, len ) );
        angles.y = rad_to_deg( std::atan2( forward.y, forward.x ) );
        angles.z = rad_to_deg( std::atan2( left.z, up.z ) );
    }

    else {
        angles.x = rad_to_deg( std::atan2( -forward.z, len ) );
        angles.y = rad_to_deg( std::atan2( -left.x, left.y ) );
        angles.z = 0.f;
    }
}

void math::MatrixCopy( const matrix3x4_t &in, matrix3x4_t &out ) {
    std::memcpy( out.Base( ), in.Base( ), sizeof( matrix3x4_t ) );
}

void math::ConcatTransforms( const matrix3x4_t &in1, const matrix3x4_t &in2, matrix3x4_t &out ) {
    if( &in1 == &out ) {
        matrix3x4_t in1b;
        MatrixCopy( in1, in1b );
        ConcatTransforms( in1b, in2, out );
        return;
    }

    if( &in2 == &out ) {
        matrix3x4_t in2b;
        MatrixCopy( in2, in2b );
        ConcatTransforms( in1, in2b, out );
        return;
    }

    out[ 0 ][ 0 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 0 ];
    out[ 0 ][ 1 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 1 ];
    out[ 0 ][ 2 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 2 ];
    out[ 0 ][ 3 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 3 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 0 ][ 3 ];

    out[ 1 ][ 0 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 0 ];
    out[ 1 ][ 1 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 1 ];
    out[ 1 ][ 2 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 2 ];
    out[ 1 ][ 3 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 3 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 1 ][ 3 ];

    out[ 2 ][ 0 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 0 ];
    out[ 2 ][ 1 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 1 ];
    out[ 2 ][ 2 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 2 ];
    out[ 2 ][ 3 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 3 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 2 ][ 3 ];
}


vec3_t math::Interpolate( const vec3_t from, const vec3_t to, const float percent ) {
    return to * percent + from * ( 1.f - percent );
}