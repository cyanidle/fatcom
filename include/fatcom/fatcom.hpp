#pragma once
#ifndef _FATCOM_HPP
#define _FATCOM_HPP

#include "describe/describe.hpp"
#include "fatcom_uuid.hpp"

namespace fatcom
{

using describe::TypeList;
using describe::Tag;

//can be overriden to define info
//for iface non-intrusively
template<typename T>
struct InfoOf : T {};

template<typename T>
using IFaceOf = typename InfoOf<T>::iface;

template<typename T>
using VTableOf = typename InfoOf<T>::vtable;

template<typename T>
using ParentOf = typename InfoOf<T>::parent;

template<typename T, typename User>
using ThunksOf = typename InfoOf<T>::template thunks<User, typename InfoOf<T>::offset>;

#ifdef _MSC_VER
#define _FAT_ALWAYS_INLINE __forceinline
#else
#define _FAT_ALWAYS_INLINE __attribute__((always_inline))
#endif

#define IMPLEMENT_FAT_VTABLE(V) \
using _VTBL = V; \
_FAT_ALWAYS_INLINE V* vtable() const noexcept {return ((_VTBL**)this)[0]; } \
_FAT_ALWAYS_INLINE void* vdata() const noexcept {return ((_VTBL**)this)[1]; }


namespace detail {

template<typename T, typename U> struct compat : std::false_type {};
template<typename R, typename C, typename I, typename...Args>
struct compat<R(C::*)(Args...), R(I::*)(Args...)> : std::true_type {};

template<typename _User>
_User* cast(void* self, bool) {return (_User*)self;}
template<typename _User, typename T>
T* cast(void* self, T _User::*member) {return &(((_User*)self)->*member);}
template<typename _User, typename T>
T* cast(void* self, T* _User::*member) {return ((_User*)self)->*member;}

#define _FAT_CAST() fatcom::detail::cast<_User>(_self, _offset::value)
#define _FAT_CAST_T() std::remove_pointer_t<decltype(_FAT_CAST())>
#define _FAT_CHECK_COMPAT(name) static_assert( \
fatcom::detail::compat<decltype(&_FAT_CAST_T()::name), decltype(&_IFACE::name)>::value, \
"Method incompatible with interface: " #name);

}

#define FAT_IMPLEMENTS(...) \
using FatInterfaces = fatcom::TypeList<__VA_ARGS__>;

struct IUnknown_VTable {
    // MUST: increment refcount on self if cast is OK (and addRef == true)
    const IUnknown_VTable* (*QueryInterface)(void* self, UUID iid, bool addRef) noexcept;
    void (*AddRef)(void* self) noexcept;
    void (*Release)(void* self) noexcept;
};

struct IUnknown_IFace {
    IMPLEMENT_FAT_VTABLE(IUnknown_VTable)
    const IUnknown_VTable* QueryInterface(UUID iid, bool addRef) const noexcept {
        return vtable()->QueryInterface(vdata(), iid, addRef);
    }
    void AddRef() noexcept {
        vtable()->AddRef(vdata());
    }
    void Release() noexcept {
        vtable()->Release(vdata());
    }
};

struct _self {
    static constexpr bool value = false;
};

struct IUnknown {
    using parent = void;
    using vtable = IUnknown_VTable;
    using iface = IUnknown_IFace;
    template<typename User, typename Offset>
    using thunks = void;
    static constexpr UUID IID = {0, 0xC000000000000046};
    using offset = _self;
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
    using Thunks = ThunksOf<IFace, User>;
    if constexpr (!std::is_void_v<Thunks>) {
        Thunks::PopulateThunks(result);
    }
}

template<typename IFace>
_FAT_ALWAYS_INLINE const IUnknown_VTable* checkIID(const IUnknown_VTable* res, UUID iid) {
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
const IUnknown_VTable* QueryInterface(void* self, UUID iid, bool addRef) noexcept {
    constexpr auto* CurrentQuery = QueryInterface<User, IFaces...>;
    const IUnknown_VTable* res = nullptr;
    (void)((res = checkIID<IFaces>(&_SingleVTable<IFaces, User, CurrentQuery>, iid)) || ...);
    if (res && addRef) AddRef<User>(self);
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

}

#include "fatcom_iptr.hpp"

namespace fatcom
{

namespace detail
{
template<typename _User, typename T>
IFaceOf<T>* cast(void* self, InterfacePtr<T> _User::*member) {return (((_User*)self)->*member).operator->();}
}

using IUnknownPtr = InterfacePtr<IUnknown>;

template<typename IFace, auto member>
struct Aggregate : IFace {
    static constexpr auto value = member;
    using offset = Aggregate;
};

}

#include "fatcom_thin.hpp"

#ifndef FATCOM_NO_MACROS
#include "fatcom_macros.hpp"
#endif // FATCOM_NO_MACROS

#endif // _FATCOM_HPP
