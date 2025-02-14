#pragma once

#include "describe/describe.hpp"
#include "boost/preprocessor/seq/for_each.hpp"
#include "boost/preprocessor/seq/for_each_i.hpp"
#include "boost/preprocessor/variadic/to_seq.hpp"
#include "boost/preprocessor/comma_if.hpp"
#include "boost/preprocessor/stringize.hpp"


namespace fatcom
{

struct UUID {
    uint64_t lo;
    uint64_t hi;
};

constexpr bool operator==(UUID l, UUID r) noexcept {
    return l.hi == r.hi && l.lo == r.lo;
}

constexpr uint64_t hex(char a) {
    uint64_t res =
        a < '0' ? -1 :
        a <= '9' ? a - '0' :
        a < 'A' ? -1 :
        a <= 'F' ? a - 'A' + 10 :
        a < 'a' ? -1 :
        a <= 'f' ? a - 'a' + 10 :
        -1;
    if (res == -1) throw "Cannot parse UUID: Invalid hex digit";
    return res;
}

constexpr void dash(char ch) {
    if (ch != '-') throw "Cannot parse UUID: missing '-'";
}

constexpr uint64_t hex_int(const char*& iid) {
    uint64_t res = hex(*iid++);
    return res | hex(*iid++) << 4;
}

constexpr UUID ParseIID(const char* iid) {
    UUID res{};
    res.lo |= hex_int(iid) << 56;
    res.lo |= hex_int(iid) << 48;
    res.lo |= hex_int(iid) << 40;
    res.lo |= hex_int(iid) << 32;
    dash(*iid++);
    res.lo |= hex_int(iid) << 24;
    res.lo |= hex_int(iid) << 16;
    dash(*iid++);
    res.lo |= hex_int(iid) << 8;
    res.lo |= hex_int(iid);
    dash(*iid++);
    res.hi |= hex_int(iid);
    res.hi |= hex_int(iid) << 8;
    dash(*iid++);
    res.hi |= hex_int(iid) << 16;
    res.hi |= hex_int(iid) << 24;
    res.hi |= hex_int(iid) << 32;
    res.hi |= hex_int(iid) << 40;
    res.hi |= hex_int(iid) << 48;
    res.hi |= hex_int(iid) << 56;
    return res;
}

template<typename V, typename I, typename VC>
struct FatInfo {
    using vtable = V;
    using iface = I;
    using vtable_creator = VC;
    UUID iid;
};

template<typename T>
inline constexpr int Info = 0;

template<typename T>
using IFaceFor = typename decltype(Info<T>)::iface;

template<typename T>
using VTableFor = typename decltype(Info<T>)::vtable;

template<typename T, typename User>
inline constexpr auto GetVTableFor = decltype(Info<T>)::vtable_creator::template CreateFor<User>();

struct IUnknown;
struct IUnknown_VTable {
    const void* (*QueryInterface)(const void* old, UUID iid);
    void (*AddRef)(void* self);
    void (*Release)(void* self);
};

template<typename T>
class InterfacePtr final: public IFaceFor<T>
{
    const VTableFor<T>* vtbl;
    void* data;

    void addRef() {
        if (vtbl) vtbl->_iunk.AddRef(data);
    }
    void unref() {
        if (vtbl) vtbl->_iunk.Release(data);
    }
public:
    template<typename U>
    InterfacePtr(U* object) {
        vtbl = &GetVTableFor<T, U>;
        data = object;
        addRef();
    }
    const void* QueryInterface(UUID iid) const {
        return vtbl->_iunk.QueryInterface(vtbl, iid);
    }
    void* get() const {
        return data;
    }
    template<typename Other>
    InterfacePtr(InterfacePtr<Other> const& other) {
        vtbl = (const VTableFor<T>*)other.QueryInterface(Info<T>.iid);
        data = other.get();
        addRef();
    }
    explicit operator bool() const noexcept {
        return vtbl;
    }
    ~InterfacePtr() {
        unref();
    }
};

template<typename I, typename Current, typename U>
constexpr const void* checkIID(UUID const& iid) {
    if constexpr (std::is_same_v<I, Current>) {
        return nullptr;
    } else {
        return iid == Info<I>.iid ? &GetVTableFor<I, U> : nullptr;
    }
}

template<typename U, typename Current, typename...Ifs>
constexpr void MakeIUnk(IUnknown_VTable& out, describe::TypeList<Ifs...>) {
    out.AddRef = [](void* self) {
        static_cast<U*>(self)->AddRef();
    };
    out.Release = [](void* self) {
        static_cast<U*>(self)->Release();
    };
    out.QueryInterface = [](const void* old, UUID iid) -> const void* {
        if (Info<Current>.iid == iid) return old;
        const void* res = nullptr;
        ((res = checkIID<Ifs, Current, U>(iid)) || ...);
        return res;
    };
}

}

#define REGISTER_INFO(uuid, T, v, iface, vc) \
template<> inline constexpr fatcom::FatInfo<v, iface, vc> fatcom::Info<T>{ParseIID(uuid)}

// struct Test_VTable {
//     void (*method1)(void* self, int);
// };

// struct Test_IFace {
//     void method1(int x) {return _vtbl->method1(*(&_vtbl + 1), x);} //child class should have a ptr as first arg
// }; + DESCRIBE()^

// + Creator of concrete func ptrs

/////////

#define _GET_VTBL(this) (_VTBL*)this
#define _GET_DATA(this) (void*)((uintptr_t)this + sizeof(void*))
#define _OPEN_TYPE(t) t
#define _CHOP_TYPE(t) std::move(
#define _METHOD_SIG_ARGS(_, __, n, arg) BOOST_PP_COMMA_IF(n) _OPEN_TYPE arg
#define METHOD_SIG(...) BOOST_PP_SEQ_FOR_EACH_I(_METHOD_SIG_ARGS, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define _METHOD_ARGS(_, __, n, arg) BOOST_PP_COMMA_IF(n) _CHOP_TYPE arg )
#define METHOD_CALL(...) BOOST_PP_SEQ_FOR_EACH_I(_METHOD_ARGS, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define IFACE_METHOD_DO(ret, name, ...) \
ret name(METHOD_SIG(__VA_ARGS__)) {\
return (*(_VTBL**)this)->name(((_VTBL**)this)[0], METHOD_CALL(__VA_ARGS__));\
}
#define IFACE_METHOD(_, __, method) IFACE_METHOD_DO method
#define MAKE_IFACE(...) \
    BOOST_PP_SEQ_FOR_EACH(IFACE_METHOD, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define VTABLE_METHODS_DO(ret, name, ...) auto (*name)(void* iface, METHOD_SIG(__VA_ARGS__)) -> ret;
#define VTABLE_METHODS(_, __, arg) VTABLE_METHODS_DO arg
#define MAKE_VTABLE(...) \
    BOOST_PP_SEQ_FOR_EACH(VTABLE_METHODS, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define ADD_METHOD_DESC_DO(ret, name, ...) MEMBER(BOOST_PP_STRINGIZE(name), &_::name);
#define ADD_METHOD_DESC(_, __, method) ADD_METHOD_DESC_DO method
#define MAKE_DESCRIBE(...) \
    BOOST_PP_SEQ_FOR_EACH(ADD_METHOD_DESC, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))


#define ADD_CONCRETE_PTR_DO(ret, name, ...) \
_res.name = [](void* self, METHOD_SIG(__VA_ARGS__)) -> ret { \
    return static_cast<_User*>(self)->name(METHOD_CALL(__VA_ARGS__)); \
};

#define ADD_CONCRETE_PTR(_, __, method) ADD_CONCRETE_PTR_DO method
#define MAKE_CONCRETES(...) \
    BOOST_PP_SEQ_FOR_EACH(ADD_CONCRETE_PTR, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

/////////////

#define FAT_INTERFACE(T, uuid, ...) \
struct T; \
struct T##_VTable; struct T##_IFace; struct T##_VTableCreator; \
REGISTER_INFO(uuid, T, T##_VTable, T##_IFace, T##_VTableCreator); \
struct T##_VTable { fatcom::IUnknown_VTable _iunk; MAKE_VTABLE(__VA_ARGS__)  }; \
struct T##_IFace { using _VTBL = T##_VTable; MAKE_IFACE(__VA_ARGS__) }; \
DESCRIBE(#T, T##_IFace, void) { MAKE_DESCRIBE(__VA_ARGS__)  } \
struct T##_VTableCreator { \
template<typename _User> \
static constexpr T##_VTable CreateFor() { \
    T##_VTable _res{}; \
    fatcom::MakeIUnk<_User, T>(_res._iunk, typename _User::FatInterfaces{}); \
    MAKE_CONCRETES(__VA_ARGS__) \
    return _res;} \
}; \
using T##Ptr = fatcom::InterfacePtr<T>;

#define FAT_IMPLEMENTS(...) \
using FatInterfaces = describe::TypeList<__VA_ARGS__>


FAT_INTERFACE(
    ITest, "00112233-4455-6677-8899-aabbccddeeff",
    (bool, method1, (int)x, (int)y)
)


FAT_INTERFACE(
    ITest2, "00212233-4455-6677-8899-aabbccddeeff",
    (void, method2, (int)x)
)
