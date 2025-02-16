#pragma once

#include "describe/describe.hpp"
#include "boost/preprocessor/seq/for_each.hpp"
#include "boost/preprocessor/seq/for_each_i.hpp"
#include "boost/preprocessor/variadic/to_seq.hpp"
#include "boost/preprocessor/comma_if.hpp"
#include "boost/preprocessor/control/if.hpp"
#include "boost/preprocessor/stringize.hpp"

#define _VFIELD(this, idx) ((_VTBL**)this)[idx]

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

using describe::TypeList;
using describe::Tag;

template<typename T>
using IFaceOf = typename T::iface;

template<typename T>
using VTableOf = typename T::vtable;

template<typename T>
using ThunksOf = typename T::thunks;

template<typename T>
using ParentOf = typename T::parent;

struct IUnknown_VTable {
    const void* (*QueryInterface)(void* self, UUID iid) noexcept;
    void (*AddRef)(void* self) noexcept;
    void (*Release)(void* self) noexcept;
};

struct _IFaceIUnknown {
    using _VTBL = IUnknown_VTable;
    const void* QueryInterface(UUID iid) const noexcept {
        return _VFIELD(this, 0)->QueryInterface(_VFIELD(this, 1), iid);
    }
    void AddRef() noexcept {
        _VFIELD(this, 0)->AddRef(_VFIELD(this, 1));
    }
    void Release() noexcept {
        _VFIELD(this, 0)->Release(_VFIELD(this, 1));
    }
};

struct IUnknown {
    using parent = void;
    using vtable = IUnknown_VTable;
    using iface = _IFaceIUnknown;
    using thunks = void;
    static constexpr UUID IID = {0, 0xC000000000000046};
    static constexpr int offset = 0;
};

template<typename U>
void AddRef(void* self) noexcept {
    static_cast<U*>(self)->AddRef();
}

template<typename U>
void Release(void* self) noexcept {
    static_cast<U*>(self)->Release();
}

template<typename IFace, typename User>
constexpr void PopulateVTable(VTableOf<IFace>& result) {
    using Parent = ParentOf<IFace>;
    if constexpr (!std::is_void_v<Parent>) {
        PopulateVTable<Parent, User>(result);
    }
    using Thunks = ThunksOf<IFace>;
    if constexpr (!std::is_void_v<Thunks>) {
        Thunks::template PopulateFor<User, IFace::offset>(result);
    }
}

template<typename IFace>
const void* checkIID(const void* res, UUID iid) {
    if (IFace::IID == iid)
        return res;
    using Parent = ParentOf<IFace>;
    if constexpr (!std::is_void_v<Parent>) {
        return checkIID<Parent>(res, iid);
    } else {
        return nullptr;
    }
}

template<typename IFace, typename User, auto* query>
constexpr VTableOf<IFace> _MakeSingle() {
    VTableOf<IFace> result{};
    result.AddRef = AddRef<User>;
    result.Release = Release<User>;
    result.QueryInterface = query;
    PopulateVTable<IFace, User>(result);
    return result;
}

template<typename IFace, typename User, auto* query>
inline constexpr VTableOf<IFace> _SingleVTable = _MakeSingle<IFace, User, query>();

template<typename User, typename...IFaces>
const void* QueryInterface(void*, UUID iid) noexcept {
    constexpr auto* CurrentQuery = QueryInterface<User, IFaces...>;
    const void* res = nullptr;
    (void)((res = checkIID<IFaces>(&_SingleVTable<IFaces, User, CurrentQuery>, iid)) || ...);
    return res;
}

template<typename User, typename...IFaces>
constexpr IUnknown_VTable _GetVTableFor(TypeList<IFaces...>) {
    IUnknown_VTable result{};
    result.AddRef = AddRef<User>;
    result.Release = Release<User>;
    result.QueryInterface = QueryInterface<User, IFaces...>;
    return result;
}

template<typename User>
inline constexpr IUnknown_VTable VTableFor = _GetVTableFor<User>(typename User::FatInterfaces{});

template<typename T>
class InterfacePtr final: protected IFaceOf<T>
{
    template<typename> friend class InterfacePtr;

    const VTableOf<T>* vtbl;
    void* data;

    template<typename U>
    void copyFrom(InterfacePtr<U> const& other) {
        vtbl = other.vtbl;
        data = other.data;
        if (vtbl) this->AddRef();
    }
    template<typename U>
    void moveFrom(InterfacePtr<U>& other) {
        vtbl = std::exchange(other.vtbl, nullptr);
        data = std::exchange(other.data, nullptr);
    }
public:
    template<typename X, bool is = true>
    using if_compatible = std::enable_if_t<std::is_convertible_v<IFaceOf<X>*, IFaceOf<T>*> == is, int>;

    InterfacePtr() noexcept : vtbl(nullptr), data(nullptr) {}

    static InterfacePtr Create(void* object, const VTableOf<T>* vtbl, bool ref = true) {
        InterfacePtr res;
        res.vtbl = vtbl;
        res.data = object;
        if (ref) res.AddRef();
        return res;
    }

    void* release() {
        auto res = data;
        vtbl = nullptr;
        data = nullptr;
        return res;
    }

    template<typename Other, if_compatible<Other, false> = 1>
    InterfacePtr(InterfacePtr<Other> const& other) {
        vtbl = (const VTableOf<T>*)other.QueryInterface(T::IID);
        data = vtbl ? other.get() : nullptr;
        if (vtbl) this->AddRef();
    }

    IFaceOf<T>* operator->() const noexcept {
        return (IFaceOf<T>*)this;
    }

    IFaceOf<T>& operator*() const noexcept {
        return (IFaceOf<T>&)*this;
    }

    void* get() const {
        return data;
    }

    template<typename U, if_compatible<U> = 1>
    InterfacePtr(InterfacePtr<U> const& other) noexcept { copyFrom(other); }

    template<typename U, if_compatible<U> = 1>
    InterfacePtr& operator=(InterfacePtr<U> const& other) noexcept {
        if (this != &other) {
            if (vtbl) this->Release();
            copyFrom(other);
        }
        return *this;
    }

    template<typename U, if_compatible<U> = 1>
    InterfacePtr(InterfacePtr<U>&& other) noexcept { moveFrom(other); }

    template<typename U, if_compatible<U> = 1>
    InterfacePtr& operator=(InterfacePtr<U>&& other) noexcept {
        if (this != &other) {
            if (vtbl) this->Release();
            moveFrom(other);
        }
        return *this;
    }

    template<typename Any>
    bool operator==(InterfacePtr<Any> const& other) const noexcept {
        return data == other.data;
    }

    explicit operator bool() const noexcept {
        return vtbl;
    }

    ~InterfacePtr() {
        if (vtbl) this->Release();
    }
};

using IUnknownPtr = InterfacePtr<IUnknown>;

template<typename IFace, auto member>
struct Aggregate {
    using parent = ParentOf<IFace>;
    using vtable = VTableOf<IFace>;
    using iface = IFaceOf<IFace>;
    using thunks = ThunksOf<IFace>;
    static constexpr UUID IID = IFace::IID;
    static constexpr auto offset = member;
};

#define ALLOW_THIN_PTR(T) \
using fatcom_thinnable = int; \
const void* _VIunk = &fatcom::VTableFor<T>; \
friend void __check_thinv(T*) {static_assert(offsetof(T, _VIunk) == 0, "ALLOW_THIN_PTR() must be first member"); }

// First member of referenced class must be a pointer to any VTable
class ThinPtr {
    void* data;

    IUnknownPtr unk(bool ref = false) const noexcept {
        return data ? IUnknownPtr::Create(data, *(IUnknown_VTable**)data, ref) : IUnknownPtr{};
    }
    void unref() {
        unk(false); //Release at end of line
    }
    void ref() {
        unk(true).release(); // AddRef but no Release
    }
public:
    IUnknownPtr GetUnknown() const noexcept {
        return unk(true);
    }
    ThinPtr() noexcept : data(nullptr) {}
    static ThinPtr CreateUnsafe(void* data) noexcept {
        ThinPtr res;
        res.data = data;
        res.ref();
        return res;
    }
    void* get() const noexcept {
        return data;
    }
    template<typename T, typename T::fatcom_thinnable = 1>
    ThinPtr(T* data) noexcept : data(data) {
        ref();
    }
    ThinPtr(ThinPtr const& other) noexcept : data(other.data) {
        ref();
    }
    ThinPtr& operator=(ThinPtr const& other) noexcept {
        if (this != &other) {
            unref(); //Release
            data = other.data;
            ref();
        }
        return *this;
    }
    ThinPtr(ThinPtr&& other) noexcept : data(std::exchange(other.data, nullptr)) {}
    ThinPtr& operator=(ThinPtr&& other) noexcept {
        std::swap(*this, other);
        return *this;
    }
    explicit operator bool() const noexcept {
        return data;
    }
    bool operator==(const ThinPtr& other) const noexcept {
        return data == other.data;
    }
    ~ThinPtr() {
        unref();
    }
};

}

#define FAT_UUID(T, uuid) \
struct T; \
constexpr fatcom::UUID T##_UUID = fatcom::ParseIID(uuid);

#define FAT_INTERFACE(T, ...) \
    struct T; \
    struct T##_VTable; struct _IFace##T; struct _Thunk_##T; \
    REGISTER_INFO(T, void, T##_VTable, _IFace##T, _Thunk_##T); \
    struct T##_VTable : fatcom::IUnknown_VTable { MAKE_VTABLE(__VA_ARGS__)  }; \
    DESCRIBE(#T, T##_VTable, void) { MAKE_DESCRIBE(__VA_ARGS__)  } \
    struct _IFace##T : fatcom::_IFaceIUnknown { using _VTBL = T##_VTable; MAKE_IFACE(__VA_ARGS__) }; \
    struct _Thunk_##T { \
    using _IFACE = _IFace##T; \
    MAKE_CONCRETES(__VA_ARGS__) \
    template<typename _User, auto _offset> \
    static constexpr void PopulateFor(T##_VTable& _res) { \
        USE_CONCRETES(__VA_ARGS__) } \
    }; \
    using T##Ptr = fatcom::InterfacePtr<T>;


#define FAT_INTERFACE_INHERIT(Parent, T, ...) \
    struct T; struct Parent; \
    struct T##_VTable; struct _IFace##T; struct _Thunk_##T; \
    REGISTER_INFO(T, Parent, T##_VTable, _IFace##T, _Thunk_##T); \
    struct T##_VTable : Parent##_VTable { MAKE_VTABLE(__VA_ARGS__)  }; \
    DESCRIBE(#T, T##_VTable, void) { PARENT(Parent##_VTable); MAKE_DESCRIBE(__VA_ARGS__)  } \
    struct _IFace##T : _IFace##Parent { using _VTBL = T##_VTable; MAKE_IFACE(__VA_ARGS__) }; \
    struct _Thunk_##T { \
    using _IFACE = _IFace##T; \
    MAKE_CONCRETES(__VA_ARGS__) \
    template<typename _User, auto _offset> \
    static constexpr void PopulateFor(T##_VTable& _res) { \
        _Thunk_##Parent::PopulateFor<_User, _offset>(_res); USE_CONCRETES(__VA_ARGS__) } \
    }; \
    using T##Ptr = fatcom::InterfacePtr<T>;

#define FAT_IMPLEMENTS(...) \
    using FatInterfaces = fatcom::TypeList<__VA_ARGS__>;





///// Macro magic part:

// struct Test_VTable {
//     void (*method1)(void* self, int);
// };

// + DESCRIBE() fot VTable

// struct Test_IFace {
//     void method1(int x) { return (_vtbl)->method1(_data, x); }
// };

// + Creator of concrete func ptrs

/////////

#define REGISTER_INFO(T, p, v, i, vp) \
struct T { \
    using vtable = v; \
    using parent = p; \
    using iface = i; \
    using thunks = vp; \
    static constexpr fatcom::UUID IID = T##_UUID;\
    static constexpr int offset = 0; \
};

#define _DOCHOOSE_1_OR_1(prefix) BOOST_PP_CAT(prefix, 1)
#define _DOCHOOSE_1_OR_MORE(prefix) BOOST_PP_CAT(prefix, MORE)
#define _DOCHOOSE_1_OR_2 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_3 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_4 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_5 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_6 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_7 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_8 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_9 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_10 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_11 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_12 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_13 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_14 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_15 _DOCHOOSE_1_OR_MORE
#define _DOCHOOSE_1_OR_16 _DOCHOOSE_1_OR_MORE
#define _CHOOSE_1_OR_MORE(prefix, ...) BOOST_PP_OVERLOAD(_DOCHOOSE_1_OR_, 1,##__VA_ARGS__)(prefix)

#define _OPEN_TYPE(t) t
#define _CHOP_TYPE(t) std::forward<t>(
#define _METHOD_SIG_ARGS(_, __, n, arg) BOOST_PP_COMMA_IF(n) _OPEN_TYPE arg
#define METHOD_SIG(...) BOOST_PP_SEQ_FOR_EACH_I(_METHOD_SIG_ARGS, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define _METHOD_ARGS(_, __, n, arg) BOOST_PP_COMMA_IF(n) _CHOP_TYPE arg)
#define METHOD_CALL(...) BOOST_PP_SEQ_FOR_EACH_I(_METHOD_ARGS, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define IFACE_METHOD_DO_1(ret, name) \
ret name() { \
    return _VFIELD(this, 0)->name(_VFIELD(this, 1)); \
}
#define IFACE_METHOD_DO_MORE(ret, name, ...) \
ret name(METHOD_SIG(__VA_ARGS__)) { \
    return _VFIELD(this, 0)->name(_VFIELD(this, 1), METHOD_CALL(__VA_ARGS__)); \
}
#define IFACE_METHOD_DO(ret, name, ...) _CHOOSE_1_OR_MORE(IFACE_METHOD_DO_,##__VA_ARGS__)(ret, name,##__VA_ARGS__)
#define IFACE_METHOD(_, __, method) IFACE_METHOD_DO method
#define MAKE_IFACE(...) \
    BOOST_PP_SEQ_FOR_EACH(IFACE_METHOD, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define _VTABLE_METHOD_1(ret, name) auto (*name)(void* iface) -> ret;
#define _VTABLE_METHOD_MORE(ret, name, ...) auto (*name)(void* iface, METHOD_SIG(__VA_ARGS__)) -> ret;
#define _VTABLE_METHOD(ret, name, ...) _CHOOSE_1_OR_MORE(_VTABLE_METHOD_,##__VA_ARGS__)(ret, name,##__VA_ARGS__)
#define VTABLE_METHOD(_, __, arg) _VTABLE_METHOD arg
#define MAKE_VTABLE(...) \
    BOOST_PP_SEQ_FOR_EACH(VTABLE_METHOD, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define ADD_METHOD_DESC_DO(ret, name, ...) MEMBER(BOOST_PP_STRINGIZE(name), &_::name);
#define ADD_METHOD_DESC(_, __, method) ADD_METHOD_DESC_DO method
#define MAKE_DESCRIBE(...) \
    BOOST_PP_SEQ_FOR_EACH(ADD_METHOD_DESC, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

namespace fatcom::detail {

template<typename T, typename U> struct compat : std::false_type {};
template<typename R, typename C, typename I, typename...Args>
struct compat<R(C::*)(Args...), R(I::*)(Args...)> : std::true_type {};

template<typename _User>
_User* cast(void* self, int) {return (_User*)self;}
template<typename _User, typename T>
T* cast(void* self, T _User::*member) {return &(((_User*)self)->*member);}
template<typename _User, typename T>
T* cast(void* self, T* _User::*member) {return ((_User*)self)->*member;}
template<typename _User, typename T>
IFaceOf<T>* cast(void* self, InterfacePtr<T> _User::*member) {return (((_User*)self)->*member).operator->();}

}

#define _FAT_CAST() fatcom::detail::cast<_User>(_self, _offset)
#define _FAT_CAST_T() std::remove_pointer_t<decltype(_FAT_CAST())>
#define _FAT_CHECK_COMPAT(name) \
static_assert(\
fatcom::detail::compat<decltype(&_FAT_CAST_T()::name), decltype(&_IFACE::name)>::value, \
"Method incompatible with interface: " #name \
);

#define MAKE_CONCRETE_PTR_DO_1(ret, name) \
template<typename _User, auto _offset> \
static ret name(void* _self){ _FAT_CHECK_COMPAT(name) \
    return _FAT_CAST()->name(); }
#define MAKE_CONCRETE_PTR_DO_MORE(ret, name, ...) \
template<typename _User, auto _offset> \
static ret name(void* _self, METHOD_SIG(__VA_ARGS__)){ _FAT_CHECK_COMPAT(name) \
    return _FAT_CAST()->name(METHOD_CALL(__VA_ARGS__)); }
#define MAKE_CONCRETE_PTR_DO(ret, name, ...) _CHOOSE_1_OR_MORE(MAKE_CONCRETE_PTR_DO_,##__VA_ARGS__)(ret, name,##__VA_ARGS__)
#define MAKE_CONCRETE_PTR(_, __, method) MAKE_CONCRETE_PTR_DO method
#define MAKE_CONCRETES(...) \
BOOST_PP_SEQ_FOR_EACH(MAKE_CONCRETE_PTR, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define USE_CONCRETE_PTR_DO(ret, name, ...) _res.name = name<_User, _offset>;
#define USE_CONCRETE_PTR(_, __, method) USE_CONCRETE_PTR_DO method
#define USE_CONCRETES(...) \
    BOOST_PP_SEQ_FOR_EACH(USE_CONCRETE_PTR, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

/////////////
