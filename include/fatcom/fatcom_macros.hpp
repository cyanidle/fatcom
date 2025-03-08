#pragma once
#ifndef _FATCOM_HPP
#include "fatcom.hpp"
#endif

#include "boost/preprocessor/seq/for_each.hpp"
#include "boost/preprocessor/seq/for_each_i.hpp"
#include "boost/preprocessor/variadic/to_seq.hpp"
#include "boost/preprocessor/comma_if.hpp"
#include "boost/preprocessor/control/if.hpp"
#include "boost/preprocessor/stringize.hpp"

#define FAT_UUID(T, uuid) \
struct T; \
constexpr fatcom::UUID T##_UUID = fatcom::ParseIID(uuid);

#define FAT_INTERFACE(T, ...) \
    struct T; \
    REGISTER_INFO(T, void); \
    struct T##_VTable : fatcom::IUnknown_VTable { MAKE_VTABLE(__VA_ARGS__)  }; \
    DESCRIBE(#T, T##_VTable, void) { MAKE_DESCRIBE(__VA_ARGS__)  } \
    struct T##_IFace : fatcom::IUnknown_IFace { IMPLEMENT_FAT_VTABLE(T##_VTable); MAKE_IFACE(__VA_ARGS__) }; \
    struct T##_Thunk { \
    using _IFACE = T##_IFace; \
    MAKE_CONCRETES(__VA_ARGS__) \
    template<typename _User, typename _offset> \
    static constexpr void PopulateFor(T##_VTable& _res) { \
        USE_CONCRETES(__VA_ARGS__) } \
    }; \
    using T##Ptr = fatcom::InterfacePtr<T>;


#define FAT_INTERFACE_INHERIT(Parent, T, ...) \
    struct T; struct Parent; \
    REGISTER_INFO(T, Parent); \
    struct T##_VTable : Parent##_VTable { MAKE_VTABLE(__VA_ARGS__)  }; \
    DESCRIBE(#T, T##_VTable, void) { PARENT(Parent##_VTable); MAKE_DESCRIBE(__VA_ARGS__)  } \
    struct T##_IFace : Parent##_IFace { IMPLEMENT_FAT_VTABLE(T##_VTable); MAKE_IFACE(__VA_ARGS__) }; \
    struct T##_Thunk { \
    using _IFACE = T##_IFace; \
    MAKE_CONCRETES(__VA_ARGS__) \
    template<typename _User, typename _offset> \
    static constexpr void PopulateFor(T##_VTable& _res) { \
        Parent##_Thunk::PopulateFor<_User, _offset>(_res); USE_CONCRETES(__VA_ARGS__) } \
    }; \
    using T##Ptr = fatcom::InterfacePtr<T>;

#define FAT_IMPLEMENTS(...) \
    using FatInterfaces = fatcom::TypeList<__VA_ARGS__>;






#define REGISTER_INFO(T, par) \
struct T##_VTable; struct T##_IFace; struct T##_Thunk; \
struct T { \
    using vtable = T##_VTable; \
    using parent = par; \
    using iface = T##_IFace; \
    using thunks = T##_Thunk; \
    static constexpr fatcom::UUID IID = T##_UUID;\
    using offset = fatcom::_self; \
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
    return vtable()->name(vdata()); \
}
#define IFACE_METHOD_DO_MORE(ret, name, ...) \
ret name(METHOD_SIG(__VA_ARGS__)) { \
    return vtable()->name(vdata(), METHOD_CALL(__VA_ARGS__)); \
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

#define MAKE_CONCRETE_DO_1(ret, name) \
template<typename _User, typename _offset> \
static ret name(void* _self){ _FAT_CHECK_COMPAT(name) \
    return _FAT_CAST()->name(); }
#define MAKE_CONCRETE_DO_MORE(ret, name, ...) \
template<typename _User, typename _offset> \
static ret name(void* _self, METHOD_SIG(__VA_ARGS__)){ _FAT_CHECK_COMPAT(name) \
    return _FAT_CAST()->name(METHOD_CALL(__VA_ARGS__)); }
#define MAKE_CONCRETE_DO(ret, name, ...) _CHOOSE_1_OR_MORE(MAKE_CONCRETE_DO_,##__VA_ARGS__)(ret, name,##__VA_ARGS__)
#define MAKE_CONCRETE_PTR(_, __, method) MAKE_CONCRETE_DO method
#define MAKE_CONCRETES(...) \
BOOST_PP_SEQ_FOR_EACH(MAKE_CONCRETE_PTR, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

#define USE_CONCRETE_PTR_DO(ret, name, ...) _res.name = name<_User, _offset>;
#define USE_CONCRETE_PTR(_, __, method) USE_CONCRETE_PTR_DO method
#define USE_CONCRETES(...) \
    BOOST_PP_SEQ_FOR_EACH(USE_CONCRETE_PTR, _, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))

/////////////
