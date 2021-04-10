#pragma once
#include "il2cpp-functions.hpp"
#include "il2cpp-type-check.hpp"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

#if __has_feature(cxx_exceptions)
struct CreatedTooEarlyException : std::runtime_error {
    CreatedTooEarlyException() : std::runtime_error("A SafePtr<T> instance was created too early or a necessary GC function was not found!") {}
};
struct NullHandleException : std::runtime_error {
    NullHandleException() : std::runtime_error("A SafePtr<T> instance is holding a null handle!") {}
};
#define __SAFE_PTR_NULL_HANDLE_CHECK(handle, ...) \
if (handle) \
return __VA_ARGS__; \
throw NullHandleException()

#else
#include "utils.h"
#define __SAFE_PTR_NULL_HANDLE_CHECK(handle, ...) \
if (handle) \
return __VA_ARGS__; \
CRASH_UNLESS(false)
#endif

/// @brief A thread-safe, static type that holds a mapping from addresses to reference counts.
struct Counter {
    /// @brief Adds to the reference count of an address. If the address does not exist, initializes a new entry for it to 1.
    /// @param addr The address to add.
    static void add(void* addr) {
        std::unique_lock lock(mutex);
        auto itr = addrRefCount.find(addr);
        if (itr != addrRefCount.end()) {
            ++itr->second;
        } else {
            addrRefCount.emplace(addr, 1);
        }
    }
    /// @brief Decreases the reference count of an address. If the address has 1 or fewer references, erases it.
    /// @param addr The address to decrease.
    static void remove(void* addr) {
        std::unique_lock lock(mutex);
        auto itr = addrRefCount.find(addr);
        if (itr != addrRefCount.end() && itr->second > 1) {
            --itr->second;
        } else {
            addrRefCount.erase(itr);
        }
    }
    /// @brief Gets the reference count of an address, or 0 if no such address exists.
    /// @param addr The address to get the count of.
    /// @return The reference count of the provided address.
    static size_t get(void* addr) {
        std::shared_lock lock(mutex);
        auto itr = addrRefCount.find(addr);
        if (itr != addrRefCount.end()) {
            return itr->second;
        } else {
            return 0;
        }
    }
    private:
    static std::unordered_map<void*, size_t> addrRefCount;
    static std::shared_mutex mutex;
};

/// @brief Represents a smart pointer that has a reference count, which does NOT destroy the held instance on refcount reaching 0.
/// @tparam T The type to wrap as a pointer.
template<class T>
struct CountPointer {
    /// @brief Default constructor for Count Pointer, defaults to a nullptr, with 0 references.
    explicit CountPointer() : ptr(nullptr) {}
    /// @brief Construct a count pointer from the provided pointer, adding to the reference count (if non-null) for the provided pointer.
    /// @param p The pointer to provide. May be null, which does nothing.
    explicit CountPointer(T* p) : ptr(p) {
        if (p) {
            Counter::add(p);
        }
    }
    /// @brief Copy constructor, copies and adds to the reference count for the held non-null pointer.
    CountPointer(const CountPointer<T>& other) : ptr(other.ptr) {
        if (ptr) {
            Counter::add(ptr);
        }
    }
    /// @brief Move constructor is default, moves the pointer and keeps the reference count the same.
    CountPointer(CountPointer&& other) = default;
    /// @brief Destructor, decreases the ref count for the held non-null pointer.
    ~CountPointer() {
        if (ptr) {
            Counter::remove(ptr);
        }
    }
    /// @brief Gets the reference count held by this pointer.
    /// @return The reference count for this pointer, or 0 if the held pointer is null.
    size_t count() const {
        if (ptr) {
            return Counter::get(ptr);
        }
        return 0;
    }
    /// @brief Emplaces a new pointer into the shared pointer, decreasing the existing ref count as necessary.
    /// @param val The new pointer to replace the currently held one with.
    inline void emplace(T* val) {
        if (val != ptr) {
            if (ptr) {
                Counter::remove(ptr);
            }
            ptr = val;
            if (ptr) {
                Counter::add(ptr);
            }
        }
    }
    /// Assignment operator.
    CountPointer& operator=(T* val) {
        emplace(val);
        return *this;
    }
    /// Dereference operator.
    T& operator*() noexcept {
        if (ptr) {
            return *ptr;
        }
        SAFE_ABORT();
        return *ptr;
    }
    const T& operator*() const noexcept {
        if (ptr) {
            return *ptr;
        }
        SAFE_ABORT();
        return *ptr;
    }
    T* const operator->() const noexcept {
        if (ptr) {
            return ptr;
        }
        SAFE_ABORT();
        return nullptr;
    }
    constexpr operator bool() const noexcept {
        return ptr != nullptr;
    }

    /// @brief Get the raw pointer. Should ALMOST NEVER BE USED, UNLESS SCOPE GUARANTEES IT DIES BEFORE THIS INSTANCE DOES!
    /// @return The raw pointer saved by this instance.
    constexpr T* const __internal_get() const noexcept {
        return ptr;
    }
    private:
    T* ptr;
};

// TODO: Test to see if gc alloc works
// TODO: Make an overall Ptr interface type, virtual destructor and *, -> operators
// TODO: Remove all conversion operators? (Basically force people to guarantee lifetime of held instance?)

/// @brief Represents a C++ type that wraps a C# pointer that will be valid for the entire lifetime of this instance.
/// This instance must be created at a time such that il2cpp_functions::Init is valid, or else it will throw a CreatedTooEarlyException
/// @tparam T The type of the instance to wrap.
template<class T>
struct SafePtr {
    /// @brief Default constructor. Should be paired with emplace or = to ensure validity.
    SafePtr() {}
    /// @brief Construct a SafePtr<T> with the provided instance pointer (which may be nullptr).
    /// If you wish to wrap a non-existent pointer (ex, use as a default constructor) see the 0 arg constructor instead.
    SafePtr(T* wrappableInstance) : internalHandle(SafePointerWrapper::New(wrappableInstance)) {}
    /// @brief Construct a SafePtr<T> with the provided reference
    SafePtr(T& wrappableInstance) : internalHandle(SafePointerWrapper::New(std::addressof(wrappableInstance))) {}
    /// @brief Move constructor is default, moves the internal handle and keeps reference count the same.
    SafePtr(SafePtr&& other) = default;
    /// @brief Copy constructor copies the HANDLE, that is, the held pointer remains the same.
    /// Note that this means if you modify one SafePtr's held instance, all others that point to the same location will also reflect this change.
    /// In order to avoid a (small) performance overhead, consider using a reference type instead of a value type, or the move constructor instead.
    SafePtr(const SafePtr& other) : internalHandle(other.internalHandle) {}
    /// @brief Destructor. Destroys the internal wrapper type, if necessary.
    /// Aborts if a wrapper type exists and must be freed, yet GC_free does not exist.
    ~SafePtr() {
        if (!internalHandle) {
            // Destructor without an internal handle is trivial
            return;
        }
        // If our internal handle has 1 instance, we need to clean up the instance it points to.
        // Otherwise, some other SafePtr is currently holding a reference to this instance, so keep it around.
        if (internalHandle.count() <= 1) {
            il2cpp_functions::Init();
            if (!il2cpp_functions::GC_free) {
                SAFE_ABORT();
            }
            il2cpp_functions::GC_free(internalHandle.__internal_get());
        }
    }

    /// @brief Emplace a new value into this SafePtr, freeing an existing one, if it exists.
    /// @param other The instance to emplace.
    inline void emplace(T& other) {
        this->~SafePtr();
        internalHandle = SafePointerWrapper::New(std::addressof(other));
    }

    /// @brief Emplace a new value into this SafePtr, freeing an existing one, if it exists.
    /// @param other The instance to emplace.
    inline void emplace(T* other) {
        this->~SafePtr();
        internalHandle = SafePointerWrapper::New(other);
    }

    /// @brief Emplace a new pointer into this SafePtr, managing the existing one, if it exists.
    /// @param other The CountPointer to copy during the emplace.
    inline void emplace(CountPointer<T>& other) {
        // Clear existing instance as necessary
        this->~SafePtr();
        // Copy other into handle
        internalHandle = other;
    }

    /// @brief Move an existing CountPointer<T> into this SafePtr, deleting the existing one, if necessary.
    /// @param other The CountPointer to move during this call.
    inline void move(CountPointer<T>& other) {
        // Clear existing instance as necessary
        this->~SafePtr();
        // Move into handle
        internalHandle = std::move(other);
    }

    inline SafePtr<T>& operator=(T* other) {
        emplace(other);
        return *this;
    }

    inline SafePtr<T>& operator=(T& other) {
        emplace(other);
        return *this;
    }

    /// @brief Returns true if this instance's internal handle holds a pointer of ANY value (including nullptr)
    /// false otherwise.
    operator bool() const noexcept {
        return *internalHandle != nullptr;
    }

    /// @brief Dereferences the instance pointer to a reference type of the held instance.
    /// Throws a NullHandleException if there is no internal handle.
    T& operator *() {
        __SAFE_PTR_NULL_HANDLE_CHECK(internalHandle, *internalHandle->instancePointer);
    }

    const T& operator *() const {
        __SAFE_PTR_NULL_HANDLE_CHECK(internalHandle, *internalHandle->instancePointer);
    }

    T* const operator ->() const {
        __SAFE_PTR_NULL_HANDLE_CHECK(internalHandle, internalHandle->instancePointer);
    }

    /// @brief Explicitly cast this instance to a T*.
    /// Note, however, that the lifetime of this returned T* is not longer than the lifetime of this instance.
    /// Consider passing a SafePtr reference or copy instead.
    explicit operator T* const() const {
        __SAFE_PTR_NULL_HANDLE_CHECK(internalHandle, internalHandle->instancePointer);
    }

    private:
    struct SafePointerWrapper {
        static SafePointerWrapper* New(T* instance) {
            il2cpp_functions::Init();
            if (!il2cpp_functions::GarbageCollector_AllocateFixed) {
                #if __has_feature(cxx_exceptions)
                throw CreatedTooEarlyException();
                #else
                CRASH_UNLESS(false);
                #endif
            }
            // It should be safe to assume that GC_AllocateFixed returns a non-null pointer. If it does return null, we have a pretty big issue.
            auto* wrapper = reinterpret_cast<SafePointerWrapper*>(il2cpp_functions::GarbageCollector_AllocateFixed(sizeof(SafePointerWrapper), nullptr));
            CRASH_UNLESS(wrapper);
            wrapper->instancePointer = instance;
            return wrapper;
        }
        // Must be explicitly GC freed and allocated
        SafePointerWrapper() = delete;
        ~SafePointerWrapper() = delete;
        T* instancePointer;
    };
    CountPointer<SafePointerWrapper> internalHandle;
};

/// @brief Represents a pointer that may be GC'd, but will notify you when it has.
/// Currently unimplemented, requires a hook into all GC frees/collections
template<class T>
struct WeakPtr {

};