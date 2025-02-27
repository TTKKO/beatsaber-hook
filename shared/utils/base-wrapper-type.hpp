#pragma once
#include <type_traits>
#include <concepts>
#include <cstdint>
#include "il2cpp-type-check.hpp"

namespace il2cpp_utils {
    template<class T>
    /// @brief A concept depicting if a type is a wrapper type.
    // TODO: Make this use a static creation method instead of a constructor
    concept has_il2cpp_conversion = requires (T t) {
        {t.convert()} -> std::same_as<void*>;
        std::is_constructible_v<T, void*>;
    };
}

namespace bs_hook {
    /// @brief Represents the most basic wrapper type.
    /// All other wrapper types should inherit from this or otherwise satisfy the constraint above.
    struct Il2CppWrapperType {
        constexpr explicit Il2CppWrapperType(void* i) noexcept : instance(i) {}
        constexpr void* convert() const noexcept {
            return const_cast<void*>(instance);
        }
        protected:
        void* instance;
    };
    static_assert(il2cpp_utils::has_il2cpp_conversion<Il2CppWrapperType>);
}

NEED_NO_BOX(bs_hook::Il2CppWrapperType);
DEFINE_IL2CPP_DEFAULT_TYPE(bs_hook::Il2CppWrapperType, object);
