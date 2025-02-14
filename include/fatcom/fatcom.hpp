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

namespace detail {

template<typename Iface, typename...Result>
auto removeFromList(TypeList<>, TypeList<Result...>) -> TypeList<Result...>;

template<typename Iface, typename Head, typename...Tail, typename...Result>
auto removeFromList(TypeList<Head, Tail...>, TypeList<Result...>) {
    if constexpr (std::is_same_v<Iface, Head>) {
        return removeFromList<Iface>(TypeList<Tail...>{}, TypeList<Result...>{});
    } else {
        return removeFromList<Iface>(TypeList<Tail...>{}, TypeList<Result..., Head>{});
    }
}

template<typename T, typename List>
using list_without_t = decltype(removeFromList<T>(List{}, TypeList<>{}));

}

template<typename V, typename P, typename I, typename Populator>
struct FatInfo {
    using parent = P;
    using vtable = V;
    using iface = I;
    using vtable_populator = Populator;
    UUID iid;
};

template<typename T>
inline constexpr int InfoOf = 0;

template<typename T>
using IFaceOf = typename decltype(InfoOf<T>)::iface;

template<typename T>
using VTableOf = typename decltype(InfoOf<T>)::vtable;

template<typename T>
using VTablePopulatorOf = typename decltype(InfoOf<T>)::vtable_populator;

template<typename T>
using ParentOf = typename decltype(InfoOf<T>)::parent;

struct IUnknown;
struct IUnknown_VTable {
    const void* (*QueryInterface)(const void* old, UUID iid) noexcept;
    void (*AddRef)(void* self) noexcept;
    void (*Release)(void* self) noexcept;
};

struct _IFaceIUnknown {
    using _VTBL = IUnknown_VTable;
    const void* QueryInterface(UUID iid) const noexcept {
        // iface query does not involve data! both vfields are 0
        return _VFIELD(this, 0)->QueryInterface(_VFIELD(this, 0), iid);
    }
    void AddRef() noexcept {
        _VFIELD(this, 0)->AddRef(_VFIELD(this, 1));
    }
    void Release() noexcept {
        _VFIELD(this, 0)->Release(_VFIELD(this, 1));
    }
};

template<>
inline constexpr FatInfo<IUnknown_VTable, void, _IFaceIUnknown, void>
InfoOf<IUnknown>{UUID{0, 0xC000000000000046}};

template<typename IFace, typename U, typename...Other>
const void* QueryInterface(const void* old, UUID iid) noexcept;

template<typename U>
void AddRef(void* self) noexcept {
    static_cast<U*>(self)->AddRef();
}

template<typename U>
void Release(void* self) noexcept {
    static_cast<U*>(self)->Release();
}

template<typename U, typename IFace, typename...Other>
constexpr void MakeQueryIface(IUnknown_VTable& out, TypeList<Other...>) {
    out.QueryInterface = QueryInterface<U, IFace, Other...>;
}

template<typename IFace, typename User>
constexpr void PopulateVTable(VTableOf<IFace>& result) {
    using Parent = ParentOf<IFace>;
    if constexpr (!std::is_void_v<Parent>) {
        PopulateVTable<Parent, User>(result);
    }
    using Populator = VTablePopulatorOf<IFace>;
    if constexpr (!std::is_void_v<Populator>) {
        Populator::template PopulateFor<User>(result);
    }
}

template<typename IFace, typename User>
constexpr VTableOf<IFace> GetVTableFor() {
    VTableOf<IFace> result{};
    result.AddRef = AddRef<User>;
    result.Release = Release<User>;
    using AllIfaces = typename User::FatInterfaces;
    using OtherIFaces = detail::list_without_t<IFace, AllIfaces>;
    MakeQueryIface<User, IFace>(result, OtherIFaces{});
    PopulateVTable<IFace, User>(result);
    return result;
}

template<typename T, typename User>
inline constexpr auto VTableFor = GetVTableFor<T, User>();

template<typename IFace>
const void* checkIID(const void* res, UUID iid) {
    if (InfoOf<IFace>.iid == iid)
        return res;
    using Parent = ParentOf<IFace>;
    if constexpr (!std::is_void_v<Parent>) {
        return checkIID<Parent>(res, iid);
    } else {
        return nullptr;
    }
}

template<typename User, typename IFace, typename...Other>
const void* QueryInterface(const void* old, UUID iid) noexcept {
    const void* res = nullptr;
    ((res = checkIID<Other>(&VTableFor<Other, User>, iid)) || ...);
    return res ? res : checkIID<IFace>(old, iid);
}

template<typename T>
class InterfacePtr final: public IFaceOf<T>
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

    template<typename U>
    InterfacePtr(U* object) {
        vtbl = &VTableFor<T, U>;
        data = object;
        if (vtbl) this->AddRef();
    }

    template<typename Other, if_compatible<Other, false> = 1>
    InterfacePtr(InterfacePtr<Other> const& other) {
        vtbl = (const VTableOf<T>*)other.QueryInterface(InfoOf<T>.iid);
        data = other.get();
        if (vtbl) this->AddRef();
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

    explicit operator bool() const noexcept {
        return vtbl;
    }

    ~InterfacePtr() {
        if (vtbl) this->Release();
    }
};

using IUnknownPtr = InterfacePtr<IUnknown>;

}

#define FAT_UUID(T, uuid) \
struct T; \
constexpr fatcom::UUID T##_UUID = fatcom::ParseIID(uuid);

#define FAT_INTERFACE(T, ...) \
    struct T; \
    struct T##_VTable; struct _IFace##T; struct _VTablePopulator##T; \
    REGISTER_INFO(T, void, T##_VTable, _IFace##T, _VTablePopulator##T); \
    struct T##_VTable : fatcom::IUnknown_VTable { MAKE_VTABLE(__VA_ARGS__)  }; \
    DESCRIBE(#T, T##_VTable, void) { MAKE_DESCRIBE(__VA_ARGS__)  } \
    struct _IFace##T : fatcom::_IFaceIUnknown { using _VTBL = T##_VTable; MAKE_IFACE(__VA_ARGS__) }; \
    struct _VTablePopulator##T { \
    template<typename _User> \
    static constexpr void PopulateFor(T##_VTable& _res) { \
        MAKE_CONCRETES(__VA_ARGS__) } \
    }; \
    using T##Ptr = fatcom::InterfacePtr<T>;


#define FAT_INTERFACE_INHERIT(Parent, T, ...) \
    struct T; struct Parent; \
    struct T##_VTable; struct _IFace##T; struct _VTablePopulator##T; \
    REGISTER_INFO(T, Parent, T##_VTable, _IFace##T, _VTablePopulator##T); \
    struct T##_VTable : Parent##_VTable { MAKE_VTABLE(__VA_ARGS__)  }; \
    DESCRIBE(#T, T##_VTable, void) { PARENT(Parent##_VTable); MAKE_DESCRIBE(__VA_ARGS__)  } \
    struct _IFace##T : _IFace##Parent { using _VTBL = T##_VTable; MAKE_IFACE(__VA_ARGS__) }; \
    struct _VTablePopulator##T { \
    template<typename _User> \
    static constexpr void PopulateFor(T##_VTable& _res) { \
        _VTablePopulator##Parent::PopulateFor<_User>(_res); MAKE_CONCRETES(__VA_ARGS__) } \
    }; \
    using T##Ptr = fatcom::InterfacePtr<T>;

#define FAT_IMPLEMENTS(...) \
    using FatInterfaces = fatcom::TypeList<__VA_ARGS__>





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

#define REGISTER_INFO(T, Parent, v, iface, vc) \
template<> \
inline constexpr fatcom::FatInfo<v, Parent, iface, vc> \
fatcom::InfoOf<T>{T##_UUID}


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
#define _CHOP_TYPE(t) std::move(
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

template<auto method, typename R, typename C, typename...Args>
R Impl(void* self, Args...args) {
    return (static_cast<C*>(self)->*method)(std::forward<Args>(args)...);
}

template<auto method, typename R, typename C, typename...Args>
constexpr auto* MakeImpl(R(C::*)(Args...)) {
    return Impl<method, R, C, Args...>;
}

}

#define ADD_CONCRETE_PTR_DO_1(ret, name) \
    (void)[](){ (void)((_User*)42)->name(); }; \
    _res.name = fatcom::detail::MakeImpl<&_User::name>(&_User::name);
// check method exists and correct signature
#define ADD_CONCRETE_PTR_DO_MORE(ret, name, ...) \
    (void)[](METHOD_SIG(__VA_ARGS__)){ (void)((_User*)42)->name(METHOD_CALL(__VA_ARGS__)); }; \
    _res.name = fatcom::detail::MakeImpl<&_User::name>(&_User::name);
#define ADD_CONCRETE_PTR_DO(ret, name, ...) _CHOOSE_1_OR_MORE(ADD_CONCRETE_PTR_DO_,##__VA_ARGS__)(ret, name,##__VA_ARGS__)
#define ADD_CONCRETE_PTR(_, __, method) ADD_CONCRETE_PTR_DO method
#define MAKE_CONCRETES(...) \
    BOOST_PP_SEQ_FOR_EACH(ADD_CONCRETE_PTR, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

/////////////
