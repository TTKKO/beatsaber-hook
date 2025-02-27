#ifndef IL2CPP_UTILS_H
#define IL2CPP_UTILS_H

#pragma pack(push)

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <optional>
#include <vector>

#include "gc-alloc.hpp"

#include "il2cpp-functions.hpp"
#include "logging.hpp"
#include "utils.h"
#include "il2cpp-type-check.hpp"
#include "typedefs.h"
#include "il2cpp-utils-methods.hpp"
#include "il2cpp-utils-classes.hpp"
#include "il2cpp-utils-exceptions.hpp"
#include "il2cpp-utils-properties.hpp"
#include "il2cpp-utils-fields.hpp"
#include <string>
#include <string_view>
#include <optional>
#include <functional>
#include <type_traits>

template<>
struct std::hash<std::pair<Il2CppMethodPointer, bool>> {
    size_t operator()(const std::pair<Il2CppMethodPointer, bool>& p) const {
        return std::hash<Il2CppMethodPointer>{}(p.first) ^ std::hash<bool>{}(p.first);
    }
};

namespace il2cpp_utils {
    // Seriously, don't un-const the returned Type
    const Il2CppType* MakeRef(const Il2CppType* type);

    // Generally, it's better to just use class_from_type!
    const Il2CppType* UnRef(const Il2CppType* type);

    ::std::vector<const Il2CppType*> ClassVecToTypes(::std::vector<const Il2CppClass*> seq);

    bool IsInterface(const Il2CppClass* klass);

    ::std::vector<Il2CppClass*> ClassesFrom(::std::vector<Il2CppClass*> classes);
    ::std::vector<Il2CppClass*> ClassesFrom(::std::vector<::std::string_view> strings);

    ::std::vector<const Il2CppType*> TypesFrom(::std::vector<const Il2CppType*> types);
    ::std::vector<const Il2CppType*> TypesFrom(::std::vector<const Il2CppClass*> classes);
    ::std::vector<const Il2CppType*> TypesFrom(::std::vector<::std::string_view> strings);

    Il2CppClass* GetParamClass(const MethodInfo* method, int paramIdx);

    /// @brief Clears all allocated delegates.
    /// THIS SHOULD NOT BE CALLED UNLESS YOU ARE CERTAIN ALL ALLOCATED DELEGATES NO LONGER EXIST IN IL2CPP!
    void ClearDelegates();

    /// @brief Clears the specified delegate.
    /// @param delegate The Delegate* to clear the allocated MethodInfo* from, if it exists
    void ClearDelegate(std::pair<Il2CppMethodPointer, bool> delegate);

    /// @brief Adds the allocated Delegate* to the set of mapped delegates.
    /// @param delegate The Delegate* to add
    /// @remarks See ClearDelegates() and ClearDelegate(Delegate* delegate)
    void AddAllocatedDelegate(std::pair<Il2CppMethodPointer, bool> delegate, MethodInfo* inf);

    // Holds a mapping from method pointers and whether it is static or not to method infos.
    extern std::unordered_map<std::pair<Il2CppMethodPointer, bool>, MethodInfo*> delegateMethodInfoMap;

    struct __InternalCSStr {
        Il2CppObject object;
        int32_t length;
        Il2CppChar chars[IL2CPP_ZERO_LEN_ARRAY];
    };

    /// @brief Create a new csstr from a UTF16 string view.
    /// @tparam creationType The creation type for the string.
    /// @param inp The input string to create.
    /// @return The returned string.
    template<CreationType creationType = CreationType::Temporary>
    Il2CppString* newcsstr(std::u16string_view inp) {
        il2cpp_functions::Init();
        if constexpr (creationType == CreationType::Manual) {
            auto len = inp.length();
            auto mallocSize = sizeof(Il2CppString) + sizeof(Il2CppChar) * (len + 1);
            // String never has any references anyways, malloc is safe here because the string gets copied over anyways.
            auto* str = reinterpret_cast<__InternalCSStr*>(malloc(mallocSize));
            str->object.klass = il2cpp_functions::defaults->string_class;
            str->object.monitor = nullptr;
            str->length = len;
            for (size_t i = 0; i < len; i++) {
                str->chars[i] = inp[i];
            }
            str->chars[len] = '\0';
            return reinterpret_cast<Il2CppString*>(str);
        } else {
            return il2cpp_functions::string_new_utf16(reinterpret_cast<const Il2CppChar*>(inp.data()), inp.length());
        }
    }

    /// @brief Create a new csstr from a UTF8 string view.
    /// @tparam creationType The creation type for the string.
    /// @param inp The input string to create.
    /// @return The returned string.
    template<CreationType creationType = CreationType::Temporary>
    Il2CppString* newcsstr(std::string_view inp) {
        il2cpp_functions::Init();
        if constexpr (creationType == CreationType::Manual) {
            // TODO: Perhaps manually call createManual instead
            auto len = inp.length();
            auto mallocSize = sizeof(Il2CppString) + sizeof(Il2CppChar) * (len + 1);
            // String never has any references anyways, malloc is safe here because the string gets copied over anyways.
            auto* str = reinterpret_cast<__InternalCSStr*>(malloc(mallocSize));
            str->object.klass = il2cpp_functions::defaults->string_class;
            str->object.monitor = nullptr;
            str->length = len;
            for (size_t i = 0; i < len; i++) {
                str->chars[i] = inp[i];
            }
            str->chars[len] = '\0';
            return reinterpret_cast<Il2CppString*>(str);
        } else {
            return il2cpp_functions::string_new_len(inp.data(), inp.size());
        }
    }

    template<class T>
    concept what_able = requires (T t) {
        {t.what()} -> std::same_as<const char*>;
    };

    /// @brief Attempts to raise the provided type as if it were an Il2CppException* in the il2cpp domain.
    /// @tparam T The exception type to throw
    /// @param arg The exception instance to throw
    template<class T>
    requires (!std::is_convertible_v<std::remove_cvref_t<T>, Il2CppException*>)
    [[noreturn]] void raise(T&& arg) {
        // Already cached in defaults, no need to re-cache
        Il2CppException* allocEx = CRASH_UNLESS(New<Il2CppException*>(classof(Il2CppException*)));
        #if __has_feature(cxx_rtti)
        const char* tName = typeid(T).name();
        int status;
        char *demangled_name = abi::__cxa_demangle(tName, NULL, NULL, &status);
        if (!status) {
            allocEx->className = newcsstr(demangled_name);
            std::free(demangled_name);
        } else {
            allocEx->className = newcsstr(tName);
        }
        #else
        #warning "Do not raise C++ exceptions without rtti!"
        #endif
        if constexpr (what_able<T>) {
            allocEx->message = newcsstr(arg.what());
        }
        #ifdef UNITY_2019
        raise(allocEx);
        #else
        #warning "Raising C++ exceptions without il2cpp_functions::raise is undefined behavior!"
        throw Il2CppExceptionWrapper(allocEx);
        #endif
    }

    /// @brief Creates a delegate of return type T, with target TObj, using the provided Il2CppClass*
    /// @tparam T The type to return
    /// @tparam TObj The type of the target object
    /// @param delegateClass The delegate Il2CppClass* to use
    /// @param obj The target instance
    /// @param callback The callback function_ptr_t
    /// @returns The created delegate
    template<typename T = MulticastDelegate*, typename TObj = Il2CppObject*, typename R, typename... TArgs>
    T MakeDelegate(const Il2CppClass* delegateClass, TObj obj, function_ptr_t<R, TArgs...> callback) {
        static_assert(sizeof(TObj) == sizeof(void*), "TObj must have the same size as a pointer!");
        static_assert(sizeof(T) == sizeof(void*), "T must have the same size as a pointer!");
        static auto& logger = getLogger();
        auto callbackPtr = reinterpret_cast<Il2CppMethodPointer>(callback);
        /*
        * TODO: call PlatformInvoke::MarshalFunctionPointerToDelegate directly instead of copying code from it,
        * or at least use a cache like utils::NativeDelegateMethodCache::GetNativeDelegate(nativeFunctionPointer);
        */
        // Lets cache this method. Well formed delegates have only one Invoke method, so ignore param count.
        auto itr = delegateMethodInfoMap.find({callbackPtr, obj == nullptr});
        MethodInfo* method;
        if (itr != delegateMethodInfoMap.end()) {
            method = itr->second;
        } else {
            auto* invoke = il2cpp_utils::FindMethodUnsafe(delegateClass, "Invoke", -1);
            method = reinterpret_cast<MethodInfo*>(calloc(1, sizeof(MethodInfo)));
            // Add the allocated delegate so we can free it later.
            method->methodPointer = callbackPtr;
            method->invoker_method = invoke->invoker_method;
            method->name = "NativeDelegateMethod";
            method->klass = il2cpp_functions::defaults->object_class;
            method->parameters = invoke->parameters;
            method->return_type = invoke->return_type;
            method->parameters_count = invoke->parameters_count;
            method->slot = kInvalidIl2CppMethodSlot;
            method->is_marshaled_from_native = true;  // "a fake MethodInfo wrapping a native function pointer"
            if (obj == nullptr) method->flags |= METHOD_ATTRIBUTE_STATIC;
            AddAllocatedDelegate({callbackPtr, obj == nullptr}, method);
        }        
        // In the event that a function is static, this will behave as normal
        // Yes, we mutate the held one as well. This is okay because we will ALWAYS mutate it.
        auto* delegate = RET_DEFAULT_UNLESS(logger, il2cpp_utils::NewUnsafe<T>(delegateClass, obj, &method));
        auto* asDelegate = reinterpret_cast<Il2CppDelegate*>(delegate);
        if ((void*)asDelegate->method_ptr != (void*)callback) {
            logger.error("Created Delegate's method_ptr (%p) is incorrect (should be %p)!", (void*)asDelegate->method_ptr, callback);
            return nullptr;
        }
        return delegate;
    }

    /// @brief Creates a delegate of return type T, with target TObj, using the provided Il2CppClass*
    /// @tparam T The type to return
    /// @tparam TObj The type of the target object
    /// @param obj The target instance
    /// @param callback The callback function_ptr_t
    /// @returns The created delegate
    template<typename T = MulticastDelegate*, typename TObj = Il2CppObject*, typename R, typename... TArgs>
    T MakeDelegate(TObj obj, function_ptr_t<R, TArgs...> callback) {
        return MakeDelegate<T, TObj>(classof(T), obj, callback);
    }

    /// @brief Creates a delegate of return type T, with target TObj, using the provided Il2CppClass*.
    /// Assumes the callback has no parameters and a void return.
    /// @tparam T The type to return
    /// @tparam TObj The type of the target object
    /// @param delegateClass The delegate Il2CppClass* to use
    /// @param obj The target instance
    /// @param callback The callback function_ptr_t
    /// @returns The created delegate
    template<typename T = MulticastDelegate*, typename TObj = Il2CppObject*>
    T MakeDelegate(const Il2CppClass* delegateClass, TObj obj, void* callback) {
        auto tmp = reinterpret_cast<function_ptr_t<void>>(callback);
        return MakeDelegate<T>(delegateClass, obj, tmp);
    }

    /// @brief Creates a delegate of return type T, with target TObj, using the provided Il2CppType*
    /// PLEASE!!! use the FieldInfo*, MethodInfo*, or Il2CppClass* versions instead if you can.
    /// @tparam T The type to return
    /// @tparam TObj The type of the target object
    /// @param delegateType The delegate Il2CppType* to use
    /// @param obj The target instance
    /// @param callback The callback function_ptr_t
    /// @returns The created delegate
    template<typename T = MulticastDelegate*, typename TObj = Il2CppObject*, typename R, typename... TArgs>
    T MakeDelegate(const Il2CppType* actionType, TObj obj, function_ptr_t<R, TArgs...> callback) {
        il2cpp_functions::Init();
        Il2CppClass* delegateClass = il2cpp_functions::class_from_il2cpp_type(actionType);
        return MakeDelegate<T>(delegateClass, obj, callback);
    }

    /// @brief Creates a delegate of return type T, with target TObj, using the provided Il2CppType* and void* callback.
    /// Assumes the callback has no parameters and a void return.
    /// @tparam T The type to return
    /// @tparam TObj The type of the target object
    /// @param delegateType The delegate Il2CppType* to use
    /// @param obj The target instance
    /// @param callback The callback function
    /// @returns The created delegate
    template<typename T = MulticastDelegate*, typename TObj = Il2CppObject*>
    T MakeDelegate(const Il2CppType* delegateType, TObj obj, void* callback) {
        auto tmp = reinterpret_cast<function_ptr_t<void>>(callback);
        return MakeDelegate<T>(delegateType, obj, tmp);
    }

    /// @brief Creates a delegate fit to be passed in the given parameter position to the given method.
    /// @tparam T The type to return
    /// @tparam T1 The type to forward to another call of MakeDelegate
    /// @tparam T2 The type to forward to another call of MakeDelegate
    /// @param method The MethodInfo* to grab the parameter from
    /// @param paramIdx The parameter to grab the type from
    /// @param arg1 Forwarded to another MakeDelegate
    /// @param arg2 Forwarded to another MakeDelegate
    /// @returns The created delegate
    template<typename T = MulticastDelegate*, typename T1, typename T2>
    T MakeDelegate(const MethodInfo* method, int paramIdx, T1&& arg1, T2&& arg2) {
        il2cpp_functions::Init();
        static auto& logger = getLogger();
        auto* delegateType = RET_0_UNLESS(logger, il2cpp_functions::method_get_param(method, paramIdx));
        return MakeDelegate<T>(delegateType, arg1, arg2);
    }

    /// @brief Creates a delegate fit to be assigned to the given field.
    /// @tparam T The type to return
    /// @tparam T1 The type to forward to another call of MakeDelegate
    /// @tparam T2 The type to forward to another call of MakeDelegate
    /// @param field The FieldInfo* to grab the parameter from
    /// @param arg1 Forwarded to another MakeDelegate
    /// @param arg2 Forwarded to another MakeDelegate
    /// @returns The created delegate
    template<typename T = MulticastDelegate*, typename T1, typename T2>
    T MakeDelegate(FieldInfo* field, T1&& arg1, T2&& arg2) {
        il2cpp_functions::Init();
        static auto& logger = getLogger();
        auto* delegateType = RET_0_UNLESS(logger, il2cpp_functions::field_get_type(field));
        return MakeDelegate<T>(delegateType, arg1, arg2);
    }

    void RemoveDelegate(Il2CppDelegate* delegateInstance, Il2CppDelegate* comparePointer) noexcept;

    /// @brief The wrapper for an invokable delegate with a context.
    /// @tparam I The instance type, which must be move-constructible.
    /// @tparam R The return type of the function being called.
    /// @tparam TArgs The argument types of the function being called.
    template<class I, class R, class... TArgs>
    struct WrapperInstance {
        I rawInstance;
        std::function<R(I*, TArgs...)> wrappedFunc;
    };

    /// @brief The wrapper for an invokable delegate without an existing context.
    /// @tparam R The return type of the function being called.
    /// @tparam TArgs The argument types of the function being called.
    template<class R, class... TArgs>
    struct WrapperStatic : Il2CppObject {
        std::function<R(TArgs...)> wrappedFunc;
    };

    /// @brief The invoker function for a delegate that has a non-trivial context.
    /// @tparam I The wrapped instance type.
    /// @tparam R The return type of the function.
    /// @tparam TArgs The argument types of the function.
    /// @param instance The wrapped instance of this context function.
    /// @param args The arguments to pass to this function.
    /// @return The return from the wrapped function.
    template<class I, class R, class... TArgs>
    R __attribute__((noinline)) invoker_func_instance(WrapperInstance<I, R, TArgs...>* instance, TArgs... args) {
        IL2CPP_CATCH_HANDLER(
            if constexpr (std::is_same_v<R, void>) {
                instance->wrappedFunc(&instance->rawInstance, args...);
            } else {
                return instance->wrappedFunc(&instance->rawInstance, args...);
            }
        )
    }

    /// @brief The invoker function for a delegate with a wrapped type.
    /// @tparam R The return type of the function.
    /// @tparam TArgs The argument types of the function.
    /// @param instance The wrapped instance of this context function.
    /// @param args The arguments to pass to this function.
    /// @return The return from the wrapped function.
    template<class R, class... TArgs>
    R __attribute__((noinline)) invoker_func_static(WrapperStatic<R, TArgs...>* instance, TArgs... args) {
        IL2CPP_CATCH_HANDLER(
            if constexpr (std::is_same_v<R, void>) {
                instance->wrappedFunc(args...);
            } else {
                return instance->wrappedFunc(args...);
            }
        )
    }

    /// @brief EXTREMEMLY UNSAFE ALLOCATION! THIS SHOULD BE AVOIDED UNLESS YOU KNOW WHAT YOU ARE DOING!
    /// This function allocates a GC-able object of the size provided by manipulating an existing Il2CppClass' instance_size.
    /// This is VERY DANGEROUS (and NOT THREAD SAFE!) and may cause all sorts of race conditions. Use at your own risk.
    /// @param size The size to allocate the unsafe object with.
    /// @return The returned GC-allocated instance.
    [[deprecated("DO NOT USE")]] void* __AllocateUnsafe(std::size_t size);

    /// @brief Makes a delegate wrapping a context function (such as a context lambda).
    /// @tparam T The type to return.
    /// @tparam I The instance object to provide to this delegate.
    /// @tparam R The return type of the delegate.
    /// @tparam TArgs The arguments of the delegate.
    /// @param delegateClass The Il2CppClass* of the delegate to create.
    /// @param instance The (move constructible) instance reference to provide to the delegate. This instance is moved and will no longer be valid.
    /// @param f The function to invoke with the delegate.
    /// @return The created delegate.
    template<typename T = MulticastDelegate*, class I, class R, class... TArgs>
    [[deprecated("DO NOT USE! USE custom_types INSTEAD!")]] T MakeDelegate(const Il2CppClass* delegateClass, I& instance, std::function<R(I*, TArgs...)> f) {
        auto* wrapperInstance = reinterpret_cast<WrapperInstance<I, R, TArgs...>*>(__AllocateUnsafe(sizeof(WrapperInstance<I, R, TArgs...>)));

        wrapperInstance->rawInstance = std::move(instance);
        wrapperInstance->wrappedFunc = f;
        return MakeDelegate<T>(delegateClass, wrapperInstance, &invoker_func_instance<I, R, TArgs...>);
    }

    /// @brief Makes a delegate wrapping a context function (such as a context lambda).
    /// @tparam T The type to return.
    /// @tparam I The instance object to provide to this delegate.
    /// @tparam R The return type of the delegate.
    /// @tparam TArgs The arguments of the delegate.
    /// @param instance The (move constructible) instance reference to provide to the delegate. This instance is moved and will no longer be valid.
    /// @param f The function to invoke with the delegate.
    /// @return The created delegate.
    template<typename T = MulticastDelegate*, class I, class R, class... TArgs>
    [[deprecated("DO NOT USE! USE custom_types INSTEAD!")]] T MakeDelegate(I& instance, std::function<R(I*, TArgs...)> f) {
        return MakeDelegate<T>(classof(T), instance, f);
    }

    /// @brief Makes a delegate wrapping a context function (such as a context lambda).
    /// @tparam T The type to return.
    /// @tparam I The instance object to provide to this delegate.
    /// @tparam R The return type of the delegate.
    /// @tparam TArgs The arguments of the delegate.
    /// @param delegateClass The Il2CppClass* of the delegate to create.
    /// @param instance The (move constructible) instance reference to provide to the delegate. This instance is moved and will no longer be valid.
    /// @param f The function to invoke with the delegate.
    /// @return The created delegate.
    template<typename T = MulticastDelegate*, class R, class... TArgs>
    [[deprecated("DO NOT USE! USE custom_types INSTEAD!")]] T MakeDelegate(const Il2CppClass* delegateClass, std::function<R(TArgs...)> f) {
        auto* wrapperInstance = reinterpret_cast<WrapperStatic<R, TArgs...>*>(__AllocateUnsafe(sizeof(WrapperStatic<R, TArgs...>)));
        wrapperInstance->wrappedFunc = f;
        return MakeDelegate<T>(delegateClass, wrapperInstance, &invoker_func_static<R, TArgs...>);
    }

    /// @brief Makes a delegate wrapping a context function (such as a context lambda).
    /// @tparam T The type to return.
    /// @tparam I The instance object to provide to this delegate.
    /// @tparam R The return type of the delegate.
    /// @tparam TArgs The arguments of the delegate.
    /// @param instance The (move constructible) instance reference to provide to the delegate. This instance is moved and will no longer be valid.
    /// @param f The function to invoke with the delegate.
    /// @return The created delegate.
    template<typename T = MulticastDelegate*, class R, class... TArgs>
    [[deprecated("DO NOT USE! USE custom_types INSTEAD!")]] T MakeDelegate(std::function<R(TArgs...)> f) {
        return MakeDelegate<T>(classof(T), f);
    }

    /// @brief Makes a delegate wrapping the provided instance method.
    /// @tparam T The type to return.
    /// @tparam I The instance object.
    /// @tparam R The return type of the delegate.
    /// @tparam TArgs The arguments of the delegate.
    /// @param delegateClass The Il2CppClass* of the delegate to create.
    /// @param instance The (move constructible) instance reference to provide to the delegate. This instance is moved and will no longer be valid.
    /// @param memberFunc A pointer to the member function on the provided instance to invoke for this delegate.
    /// @return The created delegate. 
    template<typename T = MulticastDelegate*, class I, class R, class... TArgs>
    [[deprecated("DO NOT USE! USE custom_types INSTEAD!")]] inline T MakeDelegate(const Il2CppClass* delegateClass, I& instance, R (I::*memberFunc)(TArgs...)) {
        return MakeDelegate<T>(delegateClass, instance, std::function<R(I*, TArgs...)>(memberFunc));
    }

    /// @brief Makes a delegate wrapping the provided instance method.
    /// @tparam T The type to return.
    /// @tparam I The instance object.
    /// @tparam R The return type of the delegate.
    /// @tparam TArgs The arguments of the delegate.
    /// @param instance The (move constructible) instance reference to provide to the delegate. This instance is moved and will no longer be valid.
    /// @param memberFunc A pointer to the member function on the provided instance to invoke for this delegate.
    /// @return The created delegate. 
    template<typename T = MulticastDelegate*, class I, class R, class... TArgs>
    [[deprecated("DO NOT USE! USE custom_types INSTEAD!")]] inline T MakeDelegate(I& instance, R (I::*memberFunc)(TArgs...)) {
        return MakeDelegate<T>(classof(T), instance, std::function<R(I*, TArgs...)>(memberFunc));
    }

    // Intializes an object (using the given args) fit to be passed to the given method at the given parameter index.
    template<typename... TArgs>
    Il2CppObject* CreateParam(const MethodInfo* method, int paramIdx, TArgs&& ...args) {
        static auto& logger = getLogger();
        auto* klass = RET_0_UNLESS(logger, GetParamClass(method, paramIdx));
        return il2cpp_utils::New(klass, args...);
    }

    template<typename... TArgs>
    Il2CppObject* CreateParamUnsafe(const MethodInfo* method, int paramIdx, TArgs&& ...args) {
        static auto& logger = getLogger();
        auto* klass = RET_0_UNLESS(logger, GetParamClass(method, paramIdx));
        return il2cpp_utils::NewUnsafe(klass, args...);
    }

    /// @brief Converts a vector to an Array*
    /// @tparam T Inner type of the vector and array
    /// @param vec Vector to create the Array from
    /// @return The created Array<T>*
    template<typename T>
    Array<T>* vectorToArray(::std::vector<T>& vec) {
        il2cpp_functions::Init();
        static auto& logger = getLogger();
        Array<T>* arr = reinterpret_cast<Array<T>*>(RET_0_UNLESS(logger, il2cpp_functions::array_new(il2cpp_type_check::il2cpp_no_arg_class<T>::get(), vec.size())));
        for (size_t i = 0; i < vec.size(); i++) {
            arr->values[i] = vec[i];
        }
        return arr;
    }

    // Calls the System.RuntimeType.MakeGenericType(System.Type gt, System.Type[] types) function
    Il2CppReflectionType* MakeGenericType(Il2CppReflectionType* gt, Il2CppArray* types);

    // Returns if a given source object is an object of the given class
    // Created by zoller27osu
    [[nodiscard]] bool Match(const Il2CppObject* source, const Il2CppClass* klass) noexcept;

    // Asserts that a given source object is an object of the given class
    // Created by zoller27osu
    bool AssertMatch(const Il2CppObject* source, const Il2CppClass* klass);

    template<class To, class From>
    // Downcasts a class from From* to To*
    [[nodiscard]] auto down_cast(From* in) noexcept {
        static_assert(::std::is_nothrow_convertible_v<To*, From*>);
        return static_cast<To*>(in);
    }

    template<typename... TArgs>
    // Runtime Invoke, but with a list initializer for args
    Il2CppObject* RuntimeInvoke(const MethodInfo* method, Il2CppObject* reference, Il2CppException** exc, TArgs* ...args) {
        il2cpp_functions::Init();

        void* invokeParams[] = {reinterpret_cast<void*>(args)...};
        return il2cpp_functions::runtime_invoke(method, reference, invokeParams, exc);
    }

    template<typename... TArgs>
    ::std::vector<const Il2CppClass*> ExtractFromFunctionNoArgs() {
        return { classof(TArgs)... };
    }

    /// @brief Creates and returns a C# System.Func<TArgs..., Ret> from the provided function_ptr_t.
    /// Note that this function assumes AOT code exists for a System.Func with the provided generic arguments.
    /// @tparam Ret The return type of the function
    /// @tparam TArgs The arguments of the function
    /// @returns The created System.Func<TArgs..., Ret>. Null if it could not be created.
    template<typename T = MulticastDelegate*, typename Ret, typename... TArgs>
    T MakeFunc(function_ptr_t<Ret, TArgs...> lambda) {
        static_assert(sizeof...(TArgs) + 1 <= 16, "Cannot create a Func`<T1, T2, ..., TN> where N is > 16!");
        static_assert(!std::is_same_v<Ret, void>, "Function used in ::il2cpp_utils::MakeFunc must have a non-void return!");
        static auto& logger = getLogger();
        // Get generic class with matching number of args
        static auto* genericClass = il2cpp_utils::GetClassFromName("System", "Func`" + ::std::to_string(sizeof...(TArgs) + 1));
        // Extract all parameter types and return types
        static auto genericClasses = ExtractFromFunctionNoArgs<Ret, TArgs...>();
        // Instantiate the Func` type
        auto* instantiatedFunc = RET_DEFAULT_UNLESS(logger, il2cpp_utils::MakeGeneric(genericClass, genericClasses));
        // Create the action from the instantiated Func` type
        return il2cpp_utils::MakeDelegate<T>(instantiatedFunc, static_cast<Il2CppObject*>(nullptr), lambda);
    }

    /// @brief Creates and returns a C# System.Action<TArgs...> from the provided function_ptr_t.
    /// Note that this function assumes AOT code exists for a System.Action with the provided generic arguments.
    /// @tparam TArgs The arguments of the function
    /// @returns The created System.Action<TArgs...>. Null if it could not be created.
    template<typename T = MulticastDelegate*, typename... TArgs>
    T MakeAction(function_ptr_t<void, TArgs...> lambda) {
        static_assert(sizeof...(TArgs) <= 16, "Cannot create an Action`<T1, T2, ..., TN> where N is > 16!");
        static auto& logger = getLogger();
        if constexpr (sizeof...(TArgs) != 0) {
            // Get generic class with matching number of args
            static auto* genericClass = il2cpp_utils::GetClassFromName("System", "Action`" + ::std::to_string(sizeof...(TArgs)));
            // Extract all parameter types and return types
            static auto genericClasses = ExtractFromFunctionNoArgs<TArgs...>();
            // Instantiate the Func` type
            auto* instantiatedFunc = RET_DEFAULT_UNLESS(logger, il2cpp_utils::MakeGeneric(genericClass, genericClasses));
            // Create the action from the instantiated Func` type
            return il2cpp_utils::MakeDelegate<T>(instantiatedFunc, static_cast<Il2CppObject*>(nullptr), lambda);
        } else {
            static auto* klass = il2cpp_utils::GetClassFromName("System", "Action");
            return il2cpp_utils::MakeDelegate<T>(klass, static_cast<Il2CppObject*>(nullptr), lambda);
        }
    }
    // MethodInfo* + hook variadic function --> type check
    template<class T>
    struct MethodTypeCheck;

    template<class T>
    struct InstanceMethodConverter;

    template<typename R>
    struct InstanceMethodConverter<R (*)()> {
        static_assert(!std::is_same_v<R, R>, "Cannot convert to an instance method, since the method has no parameters!");
    };

    template<typename R, typename T, typename... TArgs>
    struct InstanceMethodConverter<R (*)(T, TArgs...)> {
        using fType = R (*)(TArgs...);
    };

    template<typename R, typename... TArgs>
    /// @brief Provides a specialization for static method pointers that ensures a given method pointer matches the provided MethodInfo*.
    /// @tparam R The return type
    /// @tparam TArgs The parameter types
    struct MethodTypeCheck<R (*)(TArgs...)> {
        /// @brief Returns true if the provided MethodInfo* is a match for the function type: R (TArgs...).
        /// @param info The MethodInfo* to use when checking
        /// @return True if the MethodInfo* is a valid match, false otherwise.
        static bool valid(const MethodInfo* info) noexcept {
            if (!info) {
                getLogger().warning("Null MethodInfo* provided to: MethodTypeCheck::valid!");
                return false;
            }
            if ((info->flags & METHOD_ATTRIBUTE_STATIC) == 0) {
                return false;
            }
            il2cpp_functions::Init();
            if (!AssignableFrom<R>(il2cpp_functions::class_from_type(info->return_type))) {
                return false;
            }
            if (sizeof...(TArgs) != info->parameters_count) {
                return false;
            }
            auto* params = info->parameters;
            // Because we check arguments left to right, we can take advantage of params++ to iterate through the elements.
            // We know they must be valid since we check the parameter count above.
            if (!(AssignableFrom<TArgs>(il2cpp_functions::class_from_type((params++)->parameter_type)) && ...)) {
                return false;
            }
            return true;
        }
        /// @brief Finds a MethodInfo* that matches the template types.
        static const MethodInfo* find(::std::string_view nameSpace, ::std::string_view className, ::std::string_view methodName) {
            il2cpp_functions::Init();
            return ::il2cpp_utils::FindMethod(nameSpace, className, methodName, ::std::vector<Il2CppClass*>{}, ::std::array<const Il2CppType*, sizeof...(TArgs)>{ExtractIndependentType<TArgs>()...});
        }
        /// @brief Finds a MethodInfo* that matches the template types.
        static const MethodInfo* find(Il2CppClass* klass, ::std::string_view methodName) {
            il2cpp_functions::Init();
            return ::il2cpp_utils::FindMethod(klass, methodName, ::std::vector<Il2CppClass*>{}, ::std::array<const Il2CppType*, sizeof...(TArgs)>{ExtractIndependentType<TArgs>()...});
        }
        /// @brief Finds a MethodInfo* that matches the template types.
        static const MethodInfo* find_unsafe(::std::string_view nameSpace, ::std::string_view className, ::std::string_view methodName, bool instance = false) {
            il2cpp_functions::Init();
            return ::il2cpp_utils::FindMethodUnsafe(nameSpace, className, methodName, instance ? sizeof...(TArgs) - 1 : sizeof...(TArgs));
        }
        /// @brief Finds a MethodInfo* that matches the template types.
        static const MethodInfo* find_unsafe(Il2CppClass* klass, ::std::string_view methodName, bool instance = false) {
            il2cpp_functions::Init();
            return ::il2cpp_utils::FindMethodUnsafe(klass, methodName, instance ? sizeof...(TArgs) - 1 : sizeof...(TArgs));
        }
    };
    template<typename R, typename T, typename... TArgs>
    /// @brief Provides a specialization for instance method pointers that ensures a given method pointer matches the provided MethodInfo*.
    /// @tparam R The return type
    /// @tparam TArgs The parameter types
    struct MethodTypeCheck<R (T::*)(TArgs...)> {
        /// @brief Returns true if the provided MethodInfo* is a match for the function type: R (TArgs...).
        /// @param info The MethodInfo* to use when checking
        /// @return True if the MethodInfo* is a valid match, false otherwise.
        static bool valid(const MethodInfo* info) noexcept {
            if (!info) {
                getLogger().warning("Null MethodInfo* provided to: MethodTypeCheck::valid!");
                return false;
            }
            if ((info->flags & METHOD_ATTRIBUTE_STATIC) != 0) {
                return false;
            }
            il2cpp_functions::Init();
            if (!AssignableFrom<R>(il2cpp_functions::class_from_type(info->return_type))) {
                return false;
            }
            if (sizeof...(TArgs) != info->parameters_count) {
                return false;
            }
            auto* params = info->parameters;
            // Because we check arguments left to right, we can take advantage of params++ to iterate through the elements.
            // We know they must be valid since we check the parameter count above.
            if (!(AssignableFrom<TArgs>(il2cpp_functions::class_from_type((params++)->parameter_type)) && ...)) {
                return false;
            }
            return true;
        }
        /// @brief Finds a MethodInfo* that matches the template types.
        static const MethodInfo* find(::std::string_view nameSpace, ::std::string_view className, ::std::string_view methodName) {
            il2cpp_functions::Init();
            return ::il2cpp_utils::FindMethod(nameSpace, className, methodName, ::std::vector<Il2CppClass*>{}, ::std::array<const Il2CppType*, sizeof...(TArgs)>{ExtractIndependentType<TArgs>()...});
        }
        /// @brief Finds a MethodInfo* that matches the template types.
        static const MethodInfo* find(Il2CppClass* klass, ::std::string_view methodName) {
            il2cpp_functions::Init();
            return ::il2cpp_utils::FindMethod(klass, methodName, ::std::vector<Il2CppClass*>{}, ::std::array<const Il2CppType*, sizeof...(TArgs)>{ExtractIndependentType<TArgs>()...});
        }
    };

    /// @brief Resolves the provided icall, throwing an il2cpp_utils::RunMethodException with backtrace information if failed.
    /// Does NOT cache the resolved method pointer.
    /// Also does NOT perform any type checking of parameters, so make sure you check your parameters and return types!
    /// @tparam R The return type of the function to resolve
    /// @tparam TArgs The arguments of the function to resolve
    /// @param icallName The name of the icall to resolve
    /// @return The resolved function pointer, will always be valid or throws an il2cpp_utils::RunMethodException.
    template<class R, class... TArgs>
    function_ptr_t<R, TArgs...> resolve_icall(std::string_view icallName) {
        il2cpp_functions::Init();
        auto out = reinterpret_cast<function_ptr_t<R, TArgs...>>(il2cpp_functions::resolve_icall(icallName.data()));
        if (!out) {
            throw il2cpp_utils::RunMethodException(string_format("Failed to resolve_icall for icall: %s!", icallName.data()), nullptr);
        }
        return out;
    }
}

#pragma pack(pop)

#endif /* IL2CPP_UTILS_H */
