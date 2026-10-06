#ifndef WSA_RAII_HPP
#define WSA_RAII_HPP
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <winevt.h>
#include <string>
#include <utility>
namespace wsa {
template<class T> class ComPtr {
    T* ptr_ = nullptr;
public:
    ComPtr() = default;
    ~ComPtr() { if (ptr_) ptr_->Release(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) { if (ptr_) ptr_->Release(); ptr_ = std::exchange(other.ptr_, nullptr); }
        return *this;
    }
    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T** put() { if (ptr_) ptr_->Release(); ptr_ = nullptr; return &ptr_; }
};
class EventHandle {
    EVT_HANDLE handle_;
public:
    explicit EventHandle(EVT_HANDLE handle = nullptr) : handle_(handle) {}
    ~EventHandle() { if (handle_) EvtClose(handle_); }
    EventHandle(const EventHandle&) = delete;
    EventHandle& operator=(const EventHandle&) = delete;
    EVT_HANDLE get() const { return handle_; }
};
class Bstr {
    BSTR value_ = nullptr;
public:
    ~Bstr() { SysFreeString(value_); }
    Bstr(const Bstr&) = delete;
    Bstr& operator=(const Bstr&) = delete;
    Bstr() = default;
    BSTR* put() { SysFreeString(value_); value_ = nullptr; return &value_; }
    BSTR get() const { return value_; }
    std::wstring str() const { return value_ ? std::wstring(value_, SysStringLen(value_)) : std::wstring(); }
};
}
#endif
