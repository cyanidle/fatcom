#pragma once
#ifndef _FATCOM_HPP
#include "fatcom.hpp"
#endif

namespace fatcom
{

#define ALLOW_THIN_PTR(T) \
using fatcom_thinnable = int; \
    const void* _VIunk = &fatcom::VTableFor<T>; \
    friend void __check_thinv(T*) {static_assert(offsetof(T, _VIunk) == 0, "ALLOW_THIN_PTR() must be first member"); }

// First member of referenced class must be a pointer to any VTable
class ThinPtr {
    void* data;

    IUnknownPtr unk(bool ref = false) const noexcept {
        return data ? IUnknownPtr(data, *(IUnknown_VTable**)data, ref) : IUnknownPtr{};
    }
    void unref() {
        unk(false); //Release at end of line
    }
    void ref() {
        unk(true).leak(); // AddRef but no Release
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
