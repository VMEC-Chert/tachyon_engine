
#pragma once

namespace tyon
{

// Gurantee vectors are packed tightly because of OpenGL functions
#pragma pack(push, 1)

FORWARD struct v3f;
FORWARD template <typename T> struct vector3_t;
FORWARD template <typename T> struct vector4_t;
FORWARD struct matrix;

struct vec2
{
    f32 x; f32 y;
    CONSTRUCTOR vec2() {}
    CONSTRUCTOR vec2( f32 arg ) : x(arg), y(arg) {}
    CONSTRUCTOR vec2( f32 _x, f32 _y ) : x(_x), y(_y) {}
    explicit CONSTRUCTOR vec2( f64 _x, f64 _y ) : x(_x), y(_y) {}
    explicit CONSTRUCTOR vec2( u32 _x, u32 _y ) : x(_x), y(_y) {}
    explicit CONSTRUCTOR vec2( i32 _x, i32 _y ) : x(_x), y(_y) {}

};

struct v3f final
{
    f32 x, y, z;
    // union {
    // struct { f32 x, y, z; };
    // };

    /** Default Worldspace Normal Vectors
        This is based on the Unreal Engine coordinate system which is left handed **/
    static v3f
    FUNCTION up();
    static v3f
    FUNCTION forward();
    static v3f
    FUNCTION right();
    static v3f
    FUNCTION down();
    static v3f
    FUNCTION backward();
    static v3f
    FUNCTION left();

    CONSTRUCTOR v3f()
    { x = 0; y = 0; z = 0; }
    explicit
    CONSTRUCTOR v3f(f32 all) :
        x(all), y(all), z(all) {}
    CONSTRUCTOR v3f(f32 _x, f32 _y, f32 _z) :
        x(_x), y(_y), z(_z) {}
    CONSTRUCTOR v3f( vector4_t<float> v );

    CONSTRUCTOR v3f( const v3_f32 arg )
    {   x = arg.x; y = arg.y; z = arg.z; }

    v3f
    operator+ (const v3f rhs) const
    { return v3f(x + rhs.x, y + rhs.y, z + rhs.z); }

    v3f
    operator- (const v3f rhs) const
    { return v3f(x - rhs.x, y - rhs.y, z - rhs.z); }

    // Scalar product
    v3f
    operator* (const f32 rhs);

    /// Cross Product
    v3f
    operator* ( v3f rhs );

    /// Scalar multiplication
    v3f
    operator* (const f32 rhs) const;

    v3f
    operator/ (const v3f rhs) const
    { return v3f(x / rhs.x, y / rhs.y, z / rhs.z); }

    v3f
    operator/ (const f32 rhs) const
    { return v3f(x / rhs, y / rhs, z / rhs); }

    v3f
    operator= (f32 all)
    { x = all; y = all; z = all; return *this;}

    v3f&
    operator- ()
    { x = -x; y = -y; z = -z; return *this; }

    /// Compound Functions
    inline v3f
    operator+= (const v3f rhs)
    {
        this->x += rhs.x; this->y += rhs.y; this->z += rhs.z;
        return *this;
    }

    inline v3f
    FUNCTION normalize() const
    {
        f32 length = sqrtf( (x * x) + (y * y) + (z * z) );
        return *this/length;
    }

    // Non-Member Functions
    inline
    friend f32
    FUNCTION dot( const v3f lhs, const v3f rhs )
    { return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z); }

    inline
    friend v3f
    FUNCTION cross( const v3f lhs, const v3f rhs )
    {
        v3f out;
        out.x = ( lhs.y * rhs.z ) - (lhs.z * rhs.y);
        out.y = ( lhs.z * rhs.x ) - (lhs.x * rhs.z);
        out.z = ( lhs.x * rhs.y ) - (lhs.y * rhs.x);
        return out;
    }

    inline
    friend v3f
    normalize( const v3f v )
    {
        return v.normalize();
    }

    inline
    operator v3_f32()
    {   return v3_f32 { x, y, z }; }
};

template <typename t_calculable>
struct vector3_t final
{
    using t_vector = vector3_t<t_calculable>;
    t_calculable x, y, z;

    CONSTRUCTOR vector3_t() = default;
    explicit
    CONSTRUCTOR vector3_t(t_calculable all) :
        x(all), y(all), z(all) {}
    CONSTRUCTOR vector3_t(t_calculable _x, t_calculable _y, t_calculable _z) :
        x(_x), y(_y), z(_z) {}

    t_vector
    operator+ (const t_vector rhs) const
    { return vector3_t(x + rhs.x, y + rhs.y, z + rhs.z); }

    t_vector
    operator- (const t_vector rhs) const
    { return vector3_t(x - rhs.x, y - rhs.y, z - rhs.z); }

    /// Cross Product
    t_vector
    operator* ( t_vector rhs )
    {
        t_vector out;
        out.x = ( y * rhs.z ) - (z * rhs.y);
        out.y = ( z * rhs.x ) - (x * rhs.z);
        out.z = ( x * rhs.y ) - (y * rhs.x);
        return out;
    }

    /// Scalar multiplication
    t_vector
    operator* (const t_calculable rhs) const
    { return vector3_t(x * rhs, y * rhs, z * rhs); }

    t_vector
    operator/ (const t_vector rhs) const
    { return vector3_t(x / rhs.x, y / rhs.y, z / rhs.z); }

    t_vector
    operator/ (const t_calculable rhs) const
    { return vector3_t(x / rhs, y / rhs, z / rhs); }

    t_vector
    operator= (t_calculable all)
    { x = all; y = all; z = all; }

    t_vector&
    operator- ()
    { x = -x; y = -y; z = -z; return *this; }

    /// Compound Functions
    t_vector
    operator+= (const t_vector rhs)
    {
        this->x += rhs.x; this->y += rhs.y; this->z += rhs.y;
        return *this;
    }

    t_vector
    FUNCTION normalize() const
    {
        f32 length = sqrtf( (x * x) + (y * y) + (z * z) );
        return *this/length;
    }

    // Non-Member Functions
    friend t_calculable
    FUNCTION dot( const t_vector lhs, const t_vector rhs )
    { return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z); }

    friend t_vector
    FUNCTION cross( const t_vector lhs, const t_vector rhs )
    {
        t_vector out;
        out.x = ( lhs.y * rhs.z ) - (lhs.z * rhs.y);
        out.y = ( lhs.z * rhs.x ) - (lhs.x * rhs.z);
        out.z = ( lhs.x * rhs.y ) - (lhs.y * rhs.x);
        return out;
    }

    friend t_vector
    normalize( const t_vector v )
    {
        return v.normalize();
    }
};

template <typename t_calculable>
struct vector4_t
{
    using t_vector = vector4_t<t_calculable>;
    t_calculable x, y, z, w;

    CONSTRUCTOR vector4_t() = default;
    explicit
    CONSTRUCTOR vector4_t( t_calculable all )  :
        x(all), y(all), z(all), w(all) {}
    CONSTRUCTOR vector4_t( t_calculable _x, t_calculable _y, t_calculable _z, t_calculable _w ) :
        x(_x), y(_y), z(_z), w(_w) {}
    COPY_CONSTRUCTOR vector4_t( v3f rhs) :
        x(rhs.x), y(rhs.y), z(rhs.z), w(1) {}

    t_vector
    operator* ( matrix rhs )
    {
        f32* d = rhs.d;
        auto result = t_vector {
            d[0]*x + d[1]*y + d[2]*z + d[3]*w,
            d[4]*x + d[5]*y + d[6]*z + d[7]*w,
            d[8]*x + d[9]*y + d[10]*z + d[11]*w,
            d[12]*x + d[13]*y + d[14]*z + d[5]*w
        };
        return result;
    }

    t_vector operator+ ( const t_vector& rhs ) const
    { return t_vector(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w); }

    t_vector
    operator- ( const t_vector& rhs ) const
    { return t_vector(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w); }

    t_vector
    operator* ( const t_vector& rhs ) const
    { return t_vector(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w); }

    t_vector
    operator* ( t_calculable rhs ) const
    { return t_vector(x * rhs, y * rhs, z * rhs, w * rhs); }

    t_vector
    operator/ ( const t_vector rhs ) const
    { return t_vector(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w); }

    t_vector operator+= ( const t_vector& rhs )
    { return *this = (*this + rhs); }
private:
};

struct transform
{
    using v4 = vector4_t<f32>;
    using v3 = v3f;
    v3 translation = v3(0.0f);
    v3 rotation = v3(0.0f);
    v3 scale = v3(1.0f);

    PROC operator= ( const transform_3d& arg ) -> transform&
    {
        translation = arg.translation;
        rotation = arg.rotation;
        scale = arg.scale;
        return *this;
    }

    matrix
    translation_matrix()
    { return matrix::create_translation( translation ); }

    matrix
    transform_matrix()
    { return matrix::create_translation( translation ); }

    operator transform_3d()
    {
        return transform_3d { translation, rotation, scale };
    }

};

struct stl_facet
{
    vector3_t<f32> normal;
    vector3_t<f32> v1;
    vector3_t<f32> v2;
    vector3_t<f32> v3;
    fuint16 attribute_width = 0; // Never use, not common STL format
};

struct stl_triangle
{
    float normal_x;
    float normal_y;
    float normal_z;

    float v1_x;
    float v1_y;
    float v1_z;

    float v2_x;
    float v2_y;
    float v2_z;

    float v3_x;
    float v3_y;
    float v3_z;

    fuint16 attribute_width;

};

#pragma pack(pop)


// Type definitions have to go above usage points
// using v3_f32 = vector3_t<f32>;
// using v3_i32 = vector3_t<fint32>;
// using v3_i64 = vector3_t<fint64>;

using v4     = vector4_t<f32>;
using v4_i32 = vector4_t<fint32>;
using f4_i64 = vector4_t<fint64>;

using v2 = vec2;
using v3 = v3f;
using v4 = vector4_t<f32>;

// using rgba  = v4;
// using srgba = v4;

using ftransform = transform;

vec2 operator+(const vec2& v1, const vec2& v2);
vec2 operator+(const vec2& v1, const f32& c);
vec2 operator+(const f32& c, const vec2& v1);
vec2 operator-( vec2 v1);
vec2 operator-(const vec2& v1, const vec2& v2);
vec2 operator-(const vec2& v1, const f32& c);
vec2 operator*(const vec2& v1, const f32& c);
vec2 operator*(const f32& c, const vec2& v1);
vec2 operator/(const vec2& v1, const f32& c);
f32 dot_product(const vec2& v1, const vec2& v2);
f32 vector_length(const vec2& v1);

vec2 pair_multiply( vec2 lhs, vec2 rhs );
vec2 pair_divide( vec2 lhs, vec2 rhs );
vec2 pair_half( vec2 lhs );
vec2 abs( vec2 arg );

fstring
format_as( vec2 arg );

PROC operator*( matrix m, v3 v ) -> v3;
// PROC operator*( matrix m0,  v3 v0) -> v3;

// PROC matrix_projection_create() -> matrix;

PROC matrix_create_translation( v3 a ) -> matrix;

// Create a Euler Rotation Matrix with w component being around an arbitrary axis
PROC matrix_create_rotation( v3 euler, f32 arbitrary = 0 ) -> matrix;

// Create a Euler Rotation Matrix with w component being around an arbitrary axis
PROC matrix_create_scale( v3 a ) -> matrix;

PROC matrix_create_transform( ftransform arg ) -> matrix;

/** Specialized form of the transform matrix which applies to the whole world rather than an object.

    This means the rotation is inversed and the translation negated such that
    world transforms such that the subject of the camera (where it's pointing)
    arrives forward of the origin of clip space. Because we're moving the world
    in front of the camera, instead of the camera towards the world- it is
    inversed. */
PROC matrix_camera_view( ftransform arg ) -> matrix;

/** Create a 3D coordinate using Forward Right Up (FRU) cardinal directions.

    NOTE: This is valid for UI oriented cameras too. */
PROC v3_fru( f32 forward, f32 right, f32 up ) -> v3;

/** Create a 3D coordinate using Left Up Right (LUF) cardinal directions */
PROC v3_luf( f32 left, f32 up, f32 forward ) -> v3;

/** Generate a 'back' facing 2 triangle rectangle with counter clockwise winding order */
PROC geometry_rectangle( v2 size /* TODO: front face direction */ ) -> array<v3>;

/** Generate a 'back' facing fan-arranged circle with counter clockwise winding order */
PROC geometry_circle( f32 radius, f32 segments /* TODO: front face direction */ ) -> array<v3>;

}
