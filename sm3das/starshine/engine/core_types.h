#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cassert>

// ─────────────────────────────────────────────
//  STARSHINE ENGINE — Core Types
//  All fundamental types used engine-wide.
// ─────────────────────────────────────────────

namespace Starshine {

// ── Scalar types ──────────────────────────────
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using f32 = float;
using f64 = double;

// ── Byte-order helpers (GameCube/Wii are big-endian) ──
inline u16 bswap16(u16 v) { return __builtin_bswap16(v); }
inline u32 bswap32(u32 v) { return __builtin_bswap32(v); }
inline u64 bswap64(u64 v) { return __builtin_bswap64(v); }

// Read big-endian values from a raw byte buffer
inline u16 readU16BE(const u8* p) { return (u16(p[0]) << 8) | p[1]; }
inline u32 readU32BE(const u8* p) {
    return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3];
}
inline s16 readS16BE(const u8* p) { return (s16)readU16BE(p); }
inline s32 readS32BE(const u8* p) { return (s32)readU32BE(p); }
inline f32 readF32BE(const u8* p) {
    u32 raw = readU32BE(p);
    f32 val;
    __builtin_memcpy(&val, &raw, 4);
    return val;
}

// ── Vec2 ──────────────────────────────────────
struct Vec2 {
    f32 x = 0, y = 0;
    Vec2() = default;
    Vec2(f32 x, f32 y) : x(x), y(y) {}
    Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(f32 s)         const { return {x*s, y*s}; }
    f32  length()                 const { return std::sqrt(x*x + y*y); }
    Vec2 normalized()             const { f32 l=length(); return l>0?(*this*(1/l)):Vec2{}; }
};

// ── Vec3 ──────────────────────────────────────
struct Vec3 {
    f32 x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(f32 s)         const { return {x*s, y*s, z*s}; }
    Vec3 operator/(f32 s)         const { return {x/s, y/s, z/s}; }
    Vec3 operator-()              const { return {-x,-y,-z}; }
    Vec3& operator+=(const Vec3& o){ x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(const Vec3& o){ x-=o.x; y-=o.y; z-=o.z; return *this; }
    Vec3& operator*=(f32 s)        { x*=s; y*=s; z*=s; return *this; }

    f32  dot(const Vec3& o)   const { return x*o.x + y*o.y + z*o.z; }
    Vec3 cross(const Vec3& o) const {
        return { y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x };
    }
    f32  lengthSq()  const { return dot(*this); }
    f32  length()    const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const {
        f32 l = length();
        return l > 1e-9f ? (*this * (1.f/l)) : Vec3{};
    }

    static Vec3 lerp(const Vec3& a, const Vec3& b, f32 t) {
        return a + (b - a) * t;
    }

    // Gravity helpers: project onto plane perpendicular to normal
    Vec3 projectOntoPlane(const Vec3& normal) const {
        return *this - normal * this->dot(normal);
    }
};

inline Vec3 operator*(f32 s, const Vec3& v) { return v * s; }

// ── Vec4 ──────────────────────────────────────
struct Vec4 {
    f32 x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(f32 x,f32 y,f32 z,f32 w) : x(x),y(y),z(z),w(w) {}
    Vec4(const Vec3& v, f32 w)     : x(v.x),y(v.y),z(v.z),w(w) {}
    Vec3 xyz() const { return {x,y,z}; }
};

// ── Matrix 4x4 (column-major, OpenGL convention) ──
struct Mat4 {
    f32 m[16] = {};

    static Mat4 identity() {
        Mat4 r;
        r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.f;
        return r;
    }

    static Mat4 translation(const Vec3& t) {
        Mat4 r = identity();
        r.m[12]=t.x; r.m[13]=t.y; r.m[14]=t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r = identity();
        r.m[0]=s.x; r.m[5]=s.y; r.m[10]=s.z;
        return r;
    }

    static Mat4 rotationX(f32 a) {
        Mat4 r = identity();
        r.m[5]=cosf(a);  r.m[9]=-sinf(a);
        r.m[6]=sinf(a);  r.m[10]=cosf(a);
        return r;
    }
    static Mat4 rotationY(f32 a) {
        Mat4 r = identity();
        r.m[0]=cosf(a);  r.m[8]=sinf(a);
        r.m[2]=-sinf(a); r.m[10]=cosf(a);
        return r;
    }
    static Mat4 rotationZ(f32 a) {
        Mat4 r = identity();
        r.m[0]=cosf(a);  r.m[4]=-sinf(a);
        r.m[1]=sinf(a);  r.m[5]=cosf(a);
        return r;
    }

    // Rotation from up vector — used for galaxy planet gravity alignment
    static Mat4 fromUpVector(const Vec3& up, const Vec3& forward_hint = {0,0,1}) {
        Vec3 u = up.normalized();
        Vec3 r = u.cross(forward_hint).normalized();
        Vec3 f = r.cross(u);
        Mat4 m = identity();
        m.m[0]=r.x; m.m[4]=r.y; m.m[8]=r.z;
        m.m[1]=u.x; m.m[5]=u.y; m.m[9]=u.z;
        m.m[2]=f.x; m.m[6]=f.y; m.m[10]=f.z;
        return m;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 c;
        for(int col=0;col<4;col++)
            for(int row=0;row<4;row++) {
                f32 s=0;
                for(int k=0;k<4;k++) s += m[k*4+row] * b.m[col*4+k];
                c.m[col*4+row]=s;
            }
        return c;
    }

    Vec4 operator*(const Vec4& v) const {
        return {
            m[0]*v.x+m[4]*v.y+m[8]*v.z +m[12]*v.w,
            m[1]*v.x+m[5]*v.y+m[9]*v.z +m[13]*v.w,
            m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]*v.w,
            m[3]*v.x+m[7]*v.y+m[11]*v.z+m[15]*v.w,
        };
    }

    Vec3 transformPoint(const Vec3& p) const {
        Vec4 r = *this * Vec4(p,1.f);
        return r.xyz() * (1.f/r.w);
    }
    Vec3 transformDir(const Vec3& d) const {
        return (*this * Vec4(d,0.f)).xyz();
    }

    static Mat4 perspective(f32 fovY, f32 aspect, f32 near, f32 far) {
        f32 tanHalf = tanf(fovY * 0.5f);
        Mat4 r;
        r.m[0]  = 1.f / (aspect * tanHalf);
        r.m[5]  = 1.f / tanHalf;
        r.m[10] = -(far + near) / (far - near);
        r.m[11] = -1.f;
        r.m[14] = -(2.f * far * near) / (far - near);
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 r = f.cross(up).normalized();
        Vec3 u = r.cross(f);
        Mat4 m = identity();
        m.m[0]=r.x; m.m[4]=r.y; m.m[8]=r.z;
        m.m[1]=u.x; m.m[5]=u.y; m.m[9]=u.z;
        m.m[2]=-f.x;m.m[6]=-f.y;m.m[10]=-f.z;
        m.m[12]=-(r.dot(eye));
        m.m[13]=-(u.dot(eye));
        m.m[14]=  f.dot(eye);
        return m;
    }
};

// ── Quaternion ────────────────────────────────
struct Quat {
    f32 x=0,y=0,z=0,w=1;
    Quat() = default;
    Quat(f32 x,f32 y,f32 z,f32 w):x(x),y(y),z(z),w(w){}

    static Quat fromAxisAngle(const Vec3& axis, f32 angle) {
        f32 s = sinf(angle*0.5f);
        return {axis.x*s, axis.y*s, axis.z*s, cosf(angle*0.5f)};
    }

    Quat operator*(const Quat& o) const {
        return {
            w*o.x + x*o.w + y*o.z - z*o.y,
            w*o.y - x*o.z + y*o.w + z*o.x,
            w*o.z + x*o.y - y*o.x + z*o.w,
            w*o.w - x*o.x - y*o.y - z*o.z
        };
    }

    Vec3 rotate(const Vec3& v) const {
        Vec3 qv{x,y,z};
        return v + qv.cross(qv.cross(v) + v*w) * 2.f;
    }

    Quat normalized() const {
        f32 l = sqrtf(x*x+y*y+z*z+w*w);
        return {x/l,y/l,z/l,w/l};
    }

    static Quat slerp(const Quat& a, const Quat& b, f32 t) {
        f32 dot = a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
        Quat bb = dot < 0 ? Quat{-b.x,-b.y,-b.z,-b.w} : b;
        dot = fabsf(dot);
        if(dot > 0.9995f) {
            Quat r{a.x+(bb.x-a.x)*t, a.y+(bb.y-a.y)*t,
                   a.z+(bb.z-a.z)*t, a.w+(bb.w-a.w)*t};
            return r.normalized();
        }
        f32 theta0 = acosf(dot);
        f32 theta  = theta0 * t;
        f32 s0 = cosf(theta) - dot*sinf(theta)/sinf(theta0);
        f32 s1 = sinf(theta) / sinf(theta0);
        return {s0*a.x+s1*bb.x, s0*a.y+s1*bb.y,
                s0*a.z+s1*bb.z, s0*a.w+s1*bb.w};
    }

    Mat4 toMat4() const {
        Mat4 m = Mat4::identity();
        f32 xx=x*x,yy=y*y,zz=z*z,xy=x*y,xz=x*z,yz=y*z,wx=w*x,wy=w*y,wz=w*z;
        m.m[0]=1-2*(yy+zz); m.m[4]=2*(xy-wz);   m.m[8] =2*(xz+wy);
        m.m[1]=2*(xy+wz);   m.m[5]=1-2*(xx+zz); m.m[9] =2*(yz-wx);
        m.m[2]=2*(xz-wy);   m.m[6]=2*(yz+wx);   m.m[10]=1-2*(xx+yy);
        return m;
    }
};

// ── AABB ──────────────────────────────────────
struct AABB {
    Vec3 min, max;
    bool contains(const Vec3& p) const {
        return p.x>=min.x && p.x<=max.x &&
               p.y>=min.y && p.y<=max.y &&
               p.z>=min.z && p.z<=max.z;
    }
    bool intersects(const AABB& o) const {
        return min.x<=o.max.x && max.x>=o.min.x &&
               min.y<=o.max.y && max.y>=o.min.y &&
               min.z<=o.max.z && max.z>=o.min.z;
    }
    Vec3 center()  const { return (min+max)*0.5f; }
    Vec3 extents() const { return (max-min)*0.5f; }
};

// ── Constants ─────────────────────────────────
constexpr f32 kPI       = 3.14159265358979f;
constexpr f32 k2PI      = 6.28318530717958f;
constexpr f32 kHalfPI   = 1.57079632679489f;
constexpr f32 kDegToRad = kPI / 180.f;
constexpr f32 kRadToDeg = 180.f / kPI;
// Sunshine/Galaxy use these gravity/speed constants
constexpr f32 kGravityDefault  = -25.f;    // units/s²
constexpr f32 kGalaxyGravity   = -20.f;    // slightly weaker per planet
constexpr f32 kMarioJumpSpeed  =  28.f;    // initial Y velocity on jump
constexpr f32 kMarioRunSpeed   =  17.f;    // flat-ground max speed
constexpr f32 kMarioWalkAccel  =   8.f;

// ── Game enum ─────────────────────────────────
enum class Game : u8 {
    Sunshine = 0,
    Galaxy1  = 1,
    Galaxy2  = 2,
};

} // namespace Starshine
