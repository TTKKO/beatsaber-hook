#ifdef TEST_ARRAY

#include "../../shared/utils/typedefs-array.hpp"
#include <iostream>

static void constDoThing(const ArrayW<int>& wrap) {
    auto i = wrap[0];
    assert(wrap.Length() == 5);
    for (auto itr : wrap) {
        // do thing with each int, const
        assert(itr == i);
        std::cout << itr << std::endl;
    }
    std::cout << i << std::endl;
}

ArrayW<float> initThing;

static void doThing() {
    ArrayW<int> arr(5);
    ArrayW<int> arr2(arr);
    // Init the pointer to nullptr
    ArrayW<int*> arr3(nullptr);
    auto i = arr[0];
    assert(arr.Length() == 5);
    assert(arr2.Length() == 5);
    for (auto itr : arr) {
        // do thing with each int
        assert(itr == i);
        std::cout << itr << std::endl;
    }
    for (auto itr : arr2) {
        // do thing with each int
        assert(itr == i);
        std::cout << itr << std::endl;
    }
    std::cout << arr.Length() << std::endl;
    std::cout << static_cast<Array<int*>*>(arr3) << std::endl;
    // Should be allowed to cast back
    std::cout << static_cast<Array<int>*>(arr) << std::endl;
    std::cout << i << std::endl;
    // Should be simply nullptr
    std::cout << static_cast<Array<float>*>(initThing) << std::endl;
    
    /// get first element that fulfills the predicate
    arr.FirstOrDefault();
    arr3.First();
    arr.FirstOrDefault([](auto x){ return x == 0; });
    arr3.First([](auto x){ return x == 0; });
 
    /// get first reverse iter element that fulfills the predicate
    arr.FirstOrDefault();
    arr3.First();
    arr.LastOrDefault([](auto x){ return x == 0; });
    arr3.Last([](auto x){ return x == 0; });
}

#include "../../shared/utils/il2cpp-utils.hpp"

static void doThing2() {
    ArrayW<int> arr(2);
    MethodInfo info;
    il2cpp_utils::RunMethodThrow(static_cast<Il2CppObject*>(nullptr), &info, arr);
    il2cpp_utils::RunMethodThrow<ArrayW<Il2CppObject*>>(static_cast<Il2CppObject*>(nullptr), &info);
    il2cpp_utils::RunMethod<ArrayW<Il2CppObject*>>(static_cast<Il2CppObject*>(nullptr), &info);
    if (arr) {
        ArrayW<float> x(static_cast<ArrayW<float>>(arr));
        for (auto itr : x) {
            std::cout << itr;
        }
    }
    il2cpp_utils::RunMethod(arr, "test", arr);
    il2cpp_utils::RunMethodRethrow<ArrayW<Il2CppObject*>>((Il2CppClass*)nullptr, &info);
}

#endif