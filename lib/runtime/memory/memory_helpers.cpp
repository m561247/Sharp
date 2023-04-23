//
// Created by bknun on 9/18/2022.
//

#include "memory_helpers.h"
#include "../../../stdimports.h"
#include "garbage_collector.h"
#include "../virtual_machine.h"
#include "../error/vm_exception.h"

template<class T>
T* malloc_mem(uInt bytes, bool unsafe)
{
    T* ptr =nullptr;
    reserve_bytes(bytes, unsafe);

    ptr=(T*)malloc(bytes);
    if(ptr == nullptr) {
        throw vm_exception(vm.out_of_memory_except, "out of memory");
    }

    return ptr;
}

template<class T>
T* malloc_struct(uInt bytes, uInt size, bool unsafe) {
    T* data = malloc_mem<T>(bytes, unsafe);

    for(uInt i = 0; i < size; i++)
        init_struct(&data[i]);
    return data;
}
