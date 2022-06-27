#pragma once

namespace mu {

template<class T>
struct tvec2
{
    using scalar_t = T;
    static const int vector_length = 2;

    T x, y;
    T& operator[](int i) { return ((T*)this)[i]; }
    const T& operator[](int i) const { return ((T*)this)[i]; }
    bool operator==(const tvec2& v) const { return x == v.x && y == v.y; }
    bool operator!=(const tvec2& v) const { return !((*this)==v); }

    template<class U> void assign(const U *v) { *this = { (T)v[0], (T)v[1] }; }
    template<class U> void assign(const tvec2<U>& v) { assign((const U*)&v); }

    static constexpr tvec2 zero() { return{ (T)0, (T)0 }; }
    static constexpr tvec2 one() { return{ (T)1, (T)1 }; }
    static constexpr tvec2 set(T v) { return{ v, v }; }
};

template<class T>
struct tvec3
{
    using scalar_t = T;
    static const int vector_length = 3;

    T x, y, z;
    T& operator[](int i) { return ((T*)this)[i]; }
    const T& operator[](int i) const { return ((T*)this)[i]; }
    bool operator==(const tvec3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const tvec3& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v) { *this = { (T)v[0], (T)v[1], (T)v[2] }; }
    template<class U> void assign(const tvec3<U>& v) { assign((const U*)&v); }

    static constexpr tvec3 zero() { return{ (T)0, (T)0, (T)0 }; }
    static constexpr tvec3 one() { return{ (T)1, (T)1, (T)1 }; }
    static constexpr tvec3 set(T v) { return{ v, v, v }; }
};

template<class T>
struct tvec4
{
    using scalar_t = T;
    static const int vector_length = 4;

    T x, y, z, w;
    T& operator[](int i) { return ((T*)this)[i]; }
    const T& operator[](int i) const { return ((T*)this)[i]; }
    bool operator==(const tvec4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
    bool operator!=(const tvec4& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v) { *this = { (T)v[0], (T)v[1], (T)v[2], (T)v[3] }; }
    template<class U> void assign(const tvec4<U>& v) { assign((const U*)&v); }

    static constexpr tvec4 zero() { return{ (T)0, (T)0, (T)0, (T)0 }; }
    static constexpr tvec4 one() { return{ (T)1, (T)1, (T)1, (T)1 }; }
    static constexpr tvec4 set(T v) { return{ v, v, v, v }; }
};

template<class T>
struct tquat
{
    using scalar_t = T;
    static const int vector_length = 4;

    T x, y, z, w;
    T& operator[](int i) { return ((T*)this)[i]; }
    const T& operator[](int i) const { return ((T*)this)[i]; }
    bool operator==(const tquat& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
    bool operator!=(const tquat& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v) { *this = { (T)v[0], (T)v[1], (T)v[2], (T)v[3] }; }
    template<class U> void assign(const tquat<U>& v) { assign((const U*)&v); }

    static constexpr tquat identity() { return{ (T)0, (T)0, (T)0, (T)1 }; }
};

template<class T>
struct tmat2x2
{
    using scalar_t = T;
    using vector_t = tvec2<T>;
    static const int vector_length = 4;

    tvec2<T> m[2];
    tvec2<T>& operator[](int i) { return m[i]; }
    const tvec2<T>& operator[](int i) const { return m[i]; }
    bool operator==(const tmat2x2& v) const { return memcmp(m, v.m, sizeof(*this)) == 0; }
    bool operator!=(const tmat2x2& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v)
    {
        *this = { {
            { (T)v[0], (T)v[1] },
            { (T)v[2], (T)v[3] },
        } };
    }
    template<class U> void assign(const tmat2x2<U>& v) { assign((U*)&v); }

    static constexpr tmat2x2 identity()
    {
        return{ {
            { T(1.0), T(0.0) },
            { T(0.0), T(1.0) },
        } };
    }
};

template<class T>
struct tmat3x3
{
    using scalar_t = T;
    using vector_t = tvec3<T>;
    static const int vector_length = 9;

    tvec3<T> m[3];
    tvec3<T>& operator[](int i) { return m[i]; }
    const tvec3<T>& operator[](int i) const { return m[i]; }
    bool operator==(const tmat3x3& v) const { return memcmp(m, v.m, sizeof(*this)) == 0; }
    bool operator!=(const tmat3x3& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v)
    {
        *this = { {
            { (T)v[0], (T)v[1], (T)v[2] },
            { (T)v[3], (T)v[4], (T)v[5] },
            { (T)v[6], (T)v[7], (T)v[8] }
        } };
    }
    template<class U> void assign(const tmat3x3<U>& v) { assign((U*)&v); }

    static constexpr tmat3x3 zero()
    {
        return{ {
            { T(0), T(0), T(0) },
            { T(0), T(0), T(0) },
            { T(0), T(0), T(0) },
        } };
    }
    static constexpr tmat3x3 identity()
    {
        return{ {
            { T(1), T(0), T(0) },
            { T(0), T(1), T(0) },
            { T(0), T(0), T(1) },
        } };
    }
};

template<class T>
struct tmat4x4
{
    using scalar_t = T;
    using vector_t = tvec4<T>;
    static const int vector_length = 16;

    tvec4<T> m[4];
    tvec4<T>& operator[](int i) { return m[i]; }
    const tvec4<T>& operator[](int i) const { return m[i]; }
    bool operator==(const tmat4x4& v) const { return memcmp(m, v.m, sizeof(*this)) == 0; }
    bool operator!=(const tmat4x4& v) const { return !((*this) == v); }

    void assign(const T *v)
    {
        memcpy(this, v, sizeof(*this));
    }
    template<class U> void assign(const U *v)
    {
        *this = { {
            { (T)v[0], (T)v[1], (T)v[2], (T)v[3] },
            { (T)v[4], (T)v[5], (T)v[6], (T)v[7] },
            { (T)v[8], (T)v[9], (T)v[10],(T)v[11]},
            { (T)v[12],(T)v[13],(T)v[14],(T)v[15]}
        } };
    }
    template<class U> void assign(const tmat4x4<U>& v) { assign((U*)&v); }

    static constexpr tmat4x4 zero()
    {
        return{ {
            { (T)0, (T)0, (T)0, (T)0 },
            { (T)0, (T)0, (T)0, (T)0 },
            { (T)0, (T)0, (T)0, (T)0 },
            { (T)0, (T)0, (T)0, (T)0 },
        } };
    }
    static constexpr tmat4x4 identity()
    {
        return{ {
            { (T)1, (T)0, (T)0, (T)0 },
            { (T)0, (T)1, (T)0, (T)0 },
            { (T)0, (T)0, (T)1, (T)0 },
            { (T)0, (T)0, (T)0, (T)1 },
        } };
    }
};


using float2 = tvec2<float>;
using float3 = tvec3<float>;
using float4 = tvec4<float>;
using quatf = tquat<float>;
using float2x2 = tmat2x2<float>;
using float3x3 = tmat3x3<float>;
using float4x4 = tmat4x4<float>;

using double2 = tvec2<double>;
using double3 = tvec3<double>;
using double4 = tvec4<double>;
using quatd = tquat<double>;
using double2x2 = tmat2x2<double>;
using double3x3 = tmat3x3<double>;
using double4x4 = tmat4x4<double>;


struct Weights1
{
    float   weight;
    int     index;

    // assume 'this' is an element of array
    void copy_to(Weights1* dst, int n)
    {
        // avoid std::copy() because it is way slower than memcpy on some compilers...
        memcpy(dst, this, sizeof(Weights1) * n);
    }

    // assume 'this' is an element of array
    float normalize(int n)
    {
        float total = 0.0f;

        auto* weights = this;
        for (int i = 0; i < n; ++i)
            total += weights[i].weight;

        if (total != 0.0f) {
            float rcp_total = 1.0f / total;
            for (int i = 0; i < n; ++i)
                weights[i].weight *= rcp_total;
        }
        return total;
    }
};

template<int N>
struct Weights
{
    float   weights[N] = {};
    int     indices[N] = {};

    float normalize()
    {
        float total = 0.0f;
        for (auto w : weights)
            total += w;

        if (total != 0.0f) {
            float rcp_total = 1.0f / total;
            for (auto& w : weights)
                w *= rcp_total;
        }
        return total;
    }
};
using Weights4 = Weights<4>;

} // namespace mu


namespace ms {

class Server;
class Message
{
public:
    enum class Type
    {
        Unknown,
        Get,
        Set,
        Delete,
        Fence,
        Text,
        Screenshot,
        Query,
        Response,
    };
};
class GetMessage : public Message {};
class SetMessage : public Message {};
class DeleteMessage : public Message {};
class FenceMessage : public Message
{
public:
    enum class FenceType
    {
        Unknown,
        SceneBegin,
        SceneEnd,
    };
};
class TextMessage : public Message
{
public:
    enum class Type
    {
        Normal,
        Warning,
        Error,
    };
};
class QueryMessage : public Message
{
public:
    enum class QueryType
    {
        Unknown,
        PluginVersion,
        ProtocolVersion,
        HostName,
        RootNodes,
        AllNodes,
    };
};
class PollMessage : public Message
{
public:
    enum class PollType
    {
        Unknown,
        SceneUpdate,
    };
};

class Asset;
class Variant;
class SubmeshData;
class BlendShapeData;
class Constraint;
class InstanceInfo;
class Material;
class Texture;

enum class EntityType : int
{
    Unknown,
    Transform,
    Camera,
    Light,
    Mesh,
    Points,
};

class Entity {};
class Transform : public Entity {};
class Camera : public Transform {};
class Light : public Transform
{
public:
    enum class LightType
    {
        Unknown = -1,
        Spot,
        Directional,
        Point,
        Area,
    };
    enum class ShadowType
    {
        Unknown = -1,
        None,
        Hard,
        Soft,
    };
};
class Mesh : public Transform {};
class Points : public Transform {};
class Scene;

enum class Topology : int
{
    Points,
    Lines,
    Triangles,
    Quads,
};

enum class ZUpCorrectionMode {
    FlipYZ,
    RotateX,
};

struct GetFlags {
    uint32_t m_bitFlags = 0;
};

const int InvalidID = -1;

struct Identifier
{
    std::string name;
    int id = InvalidID;
};

struct Bounds
{
    mu::float3 center;
    mu::float3 extents;

    bool operator==(const Bounds& v) const { return center == v.center && extents == v.extents; }
    bool operator!=(const Bounds& v) const { return !(*this == v); }
};

struct SceneImportSettings {
    uint32_t flags = 0; // reserved
    uint32_t mesh_split_unit = 0xffffffff;
    int mesh_max_bone_influence = 4; // 4 or 255 (variable up to 255)
    ZUpCorrectionMode zup_correction_mode = ZUpCorrectionMode::FlipYZ;
};

struct ServerSettings
{
    int max_queue = 256;
    int max_threads = 8;
    uint16_t port = 8080;

    SceneImportSettings import_settings;
};

} // namespace ms

using msMessageHandler = void(*)(ms::Message::Type type, void* data);

#define msAPI(Func, Ret, ...) extern Ret (*Func)(__VA_ARGS__)
#include "mscoreAPI.inl"
#undef msAPI
