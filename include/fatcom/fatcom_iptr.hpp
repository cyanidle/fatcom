#pragma once
#ifndef _FATCOM_HPP
#include "fatcom.hpp"
#endif

namespace fatcom
{

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

    template<typename U, typename = typename U::FatInterfaces>
    InterfacePtr(U* object) noexcept : InterfacePtr(object, &VTableFor<U>) {}

    InterfacePtr(void* object, const VTableOf<T>* vtbl, bool ref = true) noexcept :
        vtbl(vtbl),
        data(object)
    {
        if (ref) this->AddRef();
    }

    const VTableOf<T>* vtable() const noexcept {
        return vtbl;
    }

    void* leak() noexcept {
        auto res = data;
        vtbl = nullptr;
        data = nullptr;
        return res;
    }

    template<typename Other, if_compatible<Other, false> = 1>
    InterfacePtr(InterfacePtr<Other> const& other) noexcept {
        if (other.data) {
            vtbl = (const VTableOf<T>*)other.QueryInterface(T::IID, true);
            data = vtbl ? other.get() : nullptr;
        }
    }
    template<typename Other, if_compatible<Other, false> = 1>
    InterfacePtr(InterfacePtr<Other>&& other) noexcept {
        if (other.data) {
            vtbl = (const VTableOf<T>*)other.QueryInterface(T::IID, false);
            data = vtbl ? other.get() : nullptr;
            other.vtbl = nullptr;
            other.data = nullptr;
        }
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

}
