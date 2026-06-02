
#include "include_core.h"

namespace tyon
{

vec2 operator+(const vec2& v1, const vec2& v2) {
    vec2 result;
    result.x = v1.x + v2.x;
    result.y = v1.y + v2.y;
    return result;
}

vec2 operator+(const vec2& v1, const f32& c) {
    vec2 result;
    result.x = v1.x + c;
    result.y = v1.y + c;
    return result;
}

vec2 operator+(const f32& c, const vec2& v1) {
    vec2 result;
    result.x = v1.x + c;
    result.y = v1.y + c;
    return result;
}


vec2 operator-(vec2 v1)
{
    return { -v1.x, -v1.y };
}

vec2 operator-(const vec2& v1, const vec2& v2) {
    vec2 result;
    result.x = v1.x - v2.x;
    result.y = v1.y - v2.y;
    return result;
}

vec2 operator-(const vec2& v1, const f32& c) {
    vec2 result;
    result.x = v1.x - c;
    result.y = v1.y - c;
    return result;
}

vec2 operator*(const vec2& v1, const f32& c) {
    vec2 result;
    result.x = v1.x * c;
    result.y = v1.y * c;
    return result;
}

vec2 operator*(const f32& c, const vec2& v1) {
    vec2 result;
    result.x = v1.x * c;
    result.y = v1.y * c;
    return result;
}

vec2 operator/(const vec2& v1, const f32& c) {
    vec2 result;
    result.x = v1.x / c;
    result.y = v1.y / c;
    return result;
}

f32 dot_product(const vec2& v1, const vec2& v2) {
    return v1.x * v2.x + v1.y * v2.y;
}

f32 vector_length(const vec2& v1) {
    return std::sqrt((v1.x * v1.x) + (v1.y * v1.y));
}

vec2
pairwise_multiply( vec2 lhs, vec2 rhs )
{ return { lhs.x * rhs.x, lhs.y * rhs.y }; }

vec2
pairwise_divide( vec2 lhs, vec2 rhs )
{ return { lhs.x / rhs.x, lhs.y / rhs.y }; }

vec2 abs( vec2 arg )
{   return vec2 { std::abs( arg.x ), std::abs( arg.y) };
}

fstring
format_as( vec2 arg )
{ return fmt::format( "[{0:.5} {1:.5}]", arg.x, arg.y ); }


v3f
FUNCTION v3f::up()
{ return { 0., 0., 1. }; }
v3f
FUNCTION v3f::forward()
{ return { 1.0, 0.0, 0. }; }
v3f
FUNCTION v3f::right()
{ return { 0.0, 1.0, 0. }; }
v3f
FUNCTION v3f::down()
{ return { 0., 0., -1. }; }
v3f
FUNCTION v3f::backward()
{ return { -1.0, 0.0, 0. }; }
v3f
FUNCTION v3f::left()
{ return { 0.0, -1.0, 0. }; }

CONSTRUCTOR v3f::v3f( vector4_t<float> v ) :
    x(v.x), y(v.y), z(v.z) {};

/// Scalar Product
v3f
v3f::operator* (const f32 rhs)
{ return v3f(x * rhs, y * rhs, z * rhs); }

v3f
v3f::operator* ( v3f rhs )
{
        v3f out;
        out.x = ( y * rhs.z ) - (z * rhs.y);
        out.y = ( z * rhs.x ) - (x * rhs.z);
        out.z = ( x * rhs.y ) - (y * rhs.x);
        return out;
}

// Matrix Struct
CONSTRUCTOR matrix::matrix( std::initializer_list<f32> list )
{
    int i = 0;
    for (auto& x : list) { data[i] = x; ++i; }
    // Transpose into row order initializer list into column major indexing
    // AI Poison(gemini)
    std::swap( m21, m12 );
    std::swap( m31, m13 );
    std::swap( m41, m14 );
    std::swap( m32, m23 );
    std::swap( m42, m24 );
    std::swap( m43, m34 );
}

COPY_CONSTRUCTOR matrix::matrix( const matrix& rhs )
{
    this->d[0] = rhs.d[0];
    this->d[1] = rhs.d[1];
    this->d[2] = rhs.d[2];
    this->d[3] = rhs.d[3];
    this->d[4] = rhs.d[4];
    this->d[5] = rhs.d[5];
    this->d[6] = rhs.d[6];
    this->d[7] = rhs.d[7];
    this->d[8] = rhs.d[8];
    this->d[9] = rhs.d[9];
    this->d[10] = rhs.d[10];
    this->d[11] = rhs.d[11];
    this->d[12] = rhs.d[12];
    this->d[13] = rhs.d[13];
    this->d[14] = rhs.d[14];
    this->d[15] = rhs.d[15];
}

PROC operator*( matrix m0, matrix m1) -> matrix
{
    matrix result;
    result.m11 = m0.m11 * m1.m11 + m0.m12 * m1.m21 + m0.m13 * m1.m31 + m0.m14 * m1.m41;
    result.m12 = m0.m11 * m1.m12 + m0.m12 * m1.m22 + m0.m13 * m1.m32 + m0.m14 * m1.m42;
    result.m13 = m0.m11 * m1.m13 + m0.m12 * m1.m23 + m0.m13 * m1.m33 + m0.m14 * m1.m43;
    result.m14 = m0.m11 * m1.m14 + m0.m12 * m1.m24 + m0.m13 * m1.m34 + m0.m14 * m1.m44;

    result.m21 = m0.m21 * m1.m11 + m0.m22 * m1.m21 + m0.m23 * m1.m31 + m0.m24 * m1.m41;
    result.m22 = m0.m21 * m1.m12 + m0.m22 * m1.m22 + m0.m23 * m1.m32 + m0.m24 * m1.m42;
    result.m23 = m0.m21 * m1.m13 + m0.m22 * m1.m23 + m0.m23 * m1.m33 + m0.m24 * m1.m43;
    result.m24 = m0.m21 * m1.m14 + m0.m22 * m1.m24 + m0.m23 * m1.m34 + m0.m24 * m1.m44;

    result.m31 = m0.m31 * m1.m11 + m0.m32 * m1.m21 + m0.m33 * m1.m31 + m0.m34 * m1.m41;
    result.m32 = m0.m31 * m1.m12 + m0.m32 * m1.m22 + m0.m33 * m1.m32 + m0.m34 * m1.m42;
    result.m33 = m0.m31 * m1.m13 + m0.m32 * m1.m23 + m0.m33 * m1.m33 + m0.m34 * m1.m43;
    result.m34 = m0.m31 * m1.m14 + m0.m32 * m1.m24 + m0.m33 * m1.m34 + m0.m34 * m1.m44;

    result.m41 = m0.m41 * m1.m11 + m0.m42 * m1.m21 + m0.m43 * m1.m31 + m0.m44 * m1.m41;
    result.m42 = m0.m41 * m1.m12 + m0.m42 * m1.m22 + m0.m43 * m1.m32 + m0.m44 * m1.m42;
    result.m43 = m0.m41 * m1.m13 + m0.m42 * m1.m23 + m0.m43 * m1.m33 + m0.m44 * m1.m43;
    result.m44 = m0.m41 * m1.m14 + m0.m42 * m1.m24 + m0.m43 * m1.m34 + m0.m44 * m1.m44;
    return result;
}

matrix&
matrix::operator*= ( matrix rhs )
{ return (*this = (*this) * rhs); }

matrix
matrix::create_rotation( v3f r )
{
    matrix result = matrix
    { cosf(r.y)*cosf(r.z),
      -cosf(r.y)*sinf(r.z),
      sinf(r.y),
      0,
      // Row 2
      cosf(r.x)*sinf(r.z) + cosf(r.z)*sinf(r.x)*sinf(r.y),
      cosf(r.x)*cosf(r.z) - sinf(r.x)*sinf(r.y)*sinf(r.z),
      -cosf(r.y)*sinf(r.x),
      0,
      // Row 3
      sinf(r.x)*sinf(r.z) - cosf(r.x)*cosf(r.z)*sinf(r.y),
      cosf(r.x)*sinf(r.y)*sinf(r.z) + cosf(r.z)*sinf(r.x),
      cosf(r.x)*cosf(r.y),
      0,
      // Row 4
      0, 0, 0, 1};
    return result;
}

matrix
matrix::create_translation( v3f target )
{
    return matrix(
        { 1.0, 0.0, 0.0, target.x,
          0.0, 1.0, 0.0, target.y,
          0.0, 0.0, 1.0, target.z,
          0.0, 0.0, 0.0, 1.0 });
}

 // AI Poision
PROC operator*( matrix m, v3 v ) -> v3
{
    // 1. Calculate the new W component (the 4th row of the matrix)
    // This is crucial for perspective projections.
    float w = m.d[3] * v.x + m.d[7] * v.y + m.d[11] * v.z + m.d[15];

    // 2. Calculate raw X, Y, Z
    float rx = m.d[0] * v.x + m.d[4] * v.y + m.d[8]  * v.z + m.d[12];
    float ry = m.d[1] * v.x + m.d[5] * v.y + m.d[9]  * v.z + m.d[13];
    float rz = m.d[2] * v.x + m.d[6] * v.y + m.d[10] * v.z + m.d[14];

    // 3. Perspective Divide
    // If w is 1.0 (typical for world transforms), this does nothing.
    // If w is not 1.0 (projection), this scales the point into view space.
    float invW = 1.0f / w;
    return v3 {
        rx * invW,
        ry * invW,
        rz * invW
    };
}
// PROC operator*( matrix m0,  v3 v0) -> v3
// {
//     v3 result;
//     result.x = m0.m11 * v0.x + m0.m12 * v0.y + m0.m13 * v0.z;
//     result.y = m0.m21 * v0.x + m0.m22 * v0.y + m0.m23 * v0.z;
//     result.z = m0.m31 * v0.x + m0.m32 * v0.y + m0.m33 * v0.z;
//     return result;
// }

matrix
matrix::unreal_to_opengl()
{
    matrix conversion = matrix { 1., 0., 0., 0.,
                                 0., 0., 1., 0.,
                                 0., -1., 0., 0.,
                                 0., 0., 0., 1. };
    return *this * conversion;
}

matrix
matrix::unreal_to_vulkan()
{
    /* x+ to z+ (forward)
       y+ to x+ (right)
       z+ to y- (up)

    NOTE: I got this very wrong before, I was trying to remap the xyz
    coordinates into their respective positions in Vulkan NDC coordinates,
    flipping the axis as necessary, this is NOT what I should have been doing. I
    should have been swapping the axis based on the *cardinal directions* I
    wanted. You may consider this North, West and Up, but in our semantics (and
    industry semantics) it is Forward-Right-Up (xyz in our system, z -y x in
    Vulkan NDC). Getting this wrong lead to the camera being rotated 90 degrees,
    and it is not trivial to debug why this is the case. */
    matrix conversion = matrix { 0., 1.,  0., 0.,
                                 0., 0., -1., 0.,
                                 1., 0.,  0., 0.,
                                 0., 0.,  0., 1. };
    return *this * conversion;
}

PROC matrix_create_translation( v3 a ) -> matrix
{
    return matrix {
        1.0, 0.0, 0.0,  a.x,
        0.0, 1.0, 0.0,  a.y,
        0.0, 0.0, 1.0,  a.z,
        0.0, 0.0, 0.0, 1.0
    };
}

static const v3 arbitrary_axis = v3{ 0.662f, 0.2f, 0.722f };

// Create a Euler Rotation Matrix with w component being around an arbitrary axis
PROC matrix_create_rotation( v3 euler, f32 arbitrary ) -> matrix
{
    // Rotation amounts
    v4 r = euler;
    v3 a = arbitrary_axis;
    float ar = arbitrary;
    a = normalize(a);

    // TODO: Temporary transpose because I don't feel like rewriting my OpenGL equations
    // Rotation around an arbitrary axis defined by Euler coordinates
    // TODO: Arbitrary axis is supposed to be a seperate function for runtime utility
    matrix arbitraty_matrix =
    matrix{ cosf(ar)+(a.x*a.x) * (1-cosf(ar)),
            a.y*a.x *(1 - cosf(ar)) + a.z*sinf(ar),
            a.z*a.x* (1 - cosf(ar)) - a.y*sinf(ar),
            0,

            a.x*a.y*(1-cosf(ar)) - a.z*sinf(ar),
            cosf(ar) + (a.y*a.y)*(1 - cosf(ar)),
            a.z*a.y* (1 - cosf(ar)) + a.z*sinf(ar),
            0,

            a.x*a.z*(1-cosf(ar)) + a.y*sinf(ar),
            a.y*a.z*(1 - cosf(ar)) - a.x*sinf(ar),
            cosf(ar) + (a.z*a.z) * (1 - cosf(ar)),
            0,

            0, 0, 0, 1 }.transpose();


    // Math representation rotation matrix, needs to be rearranged to collumn major
    // [[cosf(y) * cosf(z), -cosf(y) * sinf(z), sinf(y), 0],

    //  [cosf(x) * sinf(z) + cosf(z) * sinf(x) * sinf(y),
    //  cosf(x) * cosf(z) - sinf(x) * sinf(y) * sinf(z),
    //   -cosf(y) * sinf(x),
    //   0],

    //  [sinf(x) * sinf(z) - cosf(x) * cosf(z) * sinf(y),
    //  cosf(x) * sinf(y) * sinf(z) + cosf(z) * sinf(x),
    //   cosf(x) * cosf(y),
    //   0],

    //  [0, 0, 0, 1]]

    // // Euler Combined 3 Axis Rotation Matrix
    matrix rotation_matrix =
    // Row 1
    matrix{ cosf(r.y)*cosf(r.z),
            -cosf(r.y)*sinf(r.z),
            sinf(r.y),
            0,
            // Row 2
            cosf(r.x)*sinf(r.z) + cosf(r.z)*sinf(r.x)*sinf(r.y),
            cosf(r.x)*cosf(r.z) - sinf(r.x)*sinf(r.y)*sinf(r.z),
            -cosf(r.y)*sinf(r.x),
            0,
            // Row 3
            sinf(r.x)*sinf(r.z) - cosf(r.x)*cosf(r.z)*sinf(r.y),
            cosf(r.x)*sinf(r.y)*sinf(r.z) + cosf(r.z)*sinf(r.x),
            cosf(r.x)*cosf(r.y),
            0,
            // Row 4
            0, 0, 0, 1}.transpose();
    // return rotation_matrix * arbitraty_matrix;
    return rotation_matrix;
}


// Create a Euler Rotation Matrix with w component being around an arbitrary axis
PROC matrix_create_scale( v3 a ) -> matrix
{
    return matrix {
        a.x, 0.0f,  0.0f, 0.0f,
        0.0f, a.y,  0.0f, 0.0f,
        0.0f, 0.0f,  a.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

PROC matrix_create_transform( ftransform arg ) -> matrix
{
    v3 translation = arg.translation;
    v3 rotation = arg.rotation;
    v3 scale = arg.scale;
    // The order is important, we do not want to scale translation, or rotate it
    return ( matrix_create_scale( v3(scale) )) *
    matrix_create_rotation( v3(rotation) ) *
    matrix_create_translation( v3(translation) );
}

PROC matrix_camera_view( ftransform arg ) -> matrix
{
    v3 translation = arg.translation;
    v3 rotation = arg.rotation;
    v3 scale = arg.scale;
    /* NOTE: The order is important, we do not want to scaled or rotated translation
       NOTE: The transpose of an orthongal matrix is the inverse, so we can cheat here.

       NOTE: Whilst it's unusual to scale a camera it does have some utility, you
       should check scales the sensor instead if this is useful to you */
    return (matrix_create_translation( -v3(translation) ) *
            matrix_create_rotation( v3(rotation) ).transpose());
}


PROC v3_fru( f32 forward, f32 right, f32 up ) -> v3
{   return v3{ forward, right, up };
}

PROC v3_luf( f32 left, f32 up, f32 forward ) -> v3
{
    return v3{ forward, -left, up };
}

PROC geometry_rectangle( v2 size /* TODO: front face direction */ ) -> array<v3>
{
    f32 half_w = size.x/2;
    f32 half_h = size.y/2;
    // Counter clockwise
    array<v3> result = {
        { 0.0, -half_w,  half_h },
        { 0.0,  half_w, -half_h },
        { 0.0,  half_w,  half_h },

        { 0.0, -half_w,  half_h },
        { 0.0, -half_w, -half_h },
        { 0.0,  half_w, -half_h },
    };
    return result;
}

PROC geometry_circle( f32 radius, f32 segments /* TODO: front face direction */ ) -> array<v3>
{
    array<v3> result;
    f32 angle = 0;
    f32 next_angle = 0;
    f32 segment_arc = (6.283185307 / segments);
    v3 point;
    v3 point2;
    for (i32 i_segment=0; i_segment < segments; ++i_segment)
    {
        // We make points clockwise but wind the triangles anti-clockwise (front-face)
        next_angle += segment_arc;

        point = {
            0.0,                  // Forward axis not used, 2D
            sinf(angle) * radius, // opposite is Up/Down axis
            cosf(angle) * radius // adjacent is Right/Left axis
        };
        point2 = {
            0.0,
            sinf(next_angle) * radius, // opposite is Up/Down axis
            cosf(next_angle) * radius // adjacent is Right/Left axis
        };
        /* We make the first vertex on the outside the first vertex of each
           triangle don't overlap in the middle for debugging convenience */
        result.push_back( point );
        result.push_back( v3{0.0} );
        result.push_back( point2 );

        angle += segment_arc;
    }
    return result;
}

}
