// WdmCppSupport.cpp : Defines the functions for the static library.
//
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include "pch.h"

//To implement OOP in WDM, you only have to implement global new/delete operators.
//The operator declaration is already built in VC++, but WDM doesn't provide implementation.
//In usermode application, this implementation is provided in usermode CRT lib.

static FORCEINLINE void* AllocateKernelMemory(
    size_t size,
    POOL_TYPE type,
    ULONG tag)
{
//Static DriverVerifier will fail if use ExAllocatePoolWithTag.
//replace it by ExAllocatePoolUninitialized can pass it. 
    void* buffer = ExAllocatePoolUninitialized(type, size, tag);
    if (NULL != buffer)
        RtlZeroMemory(buffer, size);
    return buffer;
}

//usage: MyClass *ptr = new MyClass();
void* __cdecl operator new (size_t size)
{
    return AllocateKernelMemory(size, PagedPool, CPP_TAG);
}


//usage: MyClass *ptr = new(NonPagedPool, MY_POOL_TAG) MyClass();
void* operator new (size_t size, POOL_TYPE type, ULONG tag)
{
    return AllocateKernelMemory(size, type, tag);
}

//usage: char *ptr = new char[10];
void* __cdecl operator new[](size_t size)
{
    return AllocateKernelMemory(size, PagedPool, CPP_TAG);
}

//usage: char *ptr = new(NonPagedPool, MY_POOL_TAG) char[10];
void* operator new[](size_t size, POOL_TYPE type, ULONG tag)
{
    return AllocateKernelMemory(size, type, tag);
}

//usage: delete ptr;
void __cdecl operator delete (void* ptr, size_t size)
{
    UNREFERENCED_PARAMETER(size);
    ExFreePool(ptr);
}

//usage: delete ptr[];
void __cdecl operator delete[](void* ptr)
{
    ExFreePool(ptr);
}

void __cdecl operator delete[](void* ptr, size_t size)
{
    UNREFERENCED_PARAMETER(size);
    ExFreePool(ptr);
}
