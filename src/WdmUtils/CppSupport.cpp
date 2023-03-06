// WdmCppSupport.cpp : Defines the functions for the static library.
//
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include "pch.h"

//To implement OOP in WDM, you only have to implement global new/delete operators.
//The operator declaration is already built in VC++, but WDM doesn't provide implementation.
//In usermode application, this implementation is provided in usermode CRT lib.

typedef struct _OBJ_POOL_TRACK
{
    int             Marker;
    size_t          TrackSize;          //size of entire OBJ_POOL_TRACK allocated.
    size_t          PtrSize;            //size of Ptr. It should be (TrackSize - sizeof(OBJ_POOL_TRACK))
    BOOLEAN         IsArray;            //Is this allocation from new[]  ?
    ULONG           PoolTag;            //the pool tag assigned during buffer creating.
    char            Ptr[1];             //pointer returned to caller
}OBJ_POOL_TRACK, * POBJ_POOL_TRACK;

static POBJ_POOL_TRACK CreatePoolTrackEntry(size_t size, POOL_TYPE type, ULONG tag = CPP_TAG)
{
    size_t head_size = sizeof(OBJ_POOL_TRACK);
    POBJ_POOL_TRACK ptr = (POBJ_POOL_TRACK)ExAllocatePoolWithTag(type, head_size + size, tag);

    if (ptr)
    {
        RtlZeroMemory(ptr->Ptr, size);
        ptr->PtrSize = size;
        ptr->TrackSize = head_size + size;
        ptr->Marker = MARKER_TAG;
        ptr->PoolTag = tag;
    }

    return ptr;
}

static void DeletePoolTrackEntry(PVOID ptr)
{
    POBJ_POOL_TRACK track = CONTAINING_RECORD(ptr, OBJ_POOL_TRACK, Ptr);
    if (ptr)
        ExFreePoolWithTag(track, track->PoolTag);
}
static void DeletePoolTrackEntry(PVOID ptr, size_t size)
{
    POBJ_POOL_TRACK track = CONTAINING_RECORD(ptr, OBJ_POOL_TRACK, Ptr);
    if (ptr)
    {
        if (size > 0 && size != track->PtrSize)
            KeBugCheckEx(BSOD_DELETE_ERROR, (ULONG_PTR)ptr, size, 0, 0);
        ExFreePoolWithTag(track, track->PoolTag);
    }
}

//usage: MyClass *ptr = new MyClass();
void* __cdecl operator new (size_t size)
{
    POBJ_POOL_TRACK buffer = CreatePoolTrackEntry(size, NonPagedPool);
    if (buffer != NULL)
        return (PVOID)(buffer->Ptr);
    return NULL;
}


//usage: MyClass *ptr = new(NonPagedPool, MY_POOL_TAG) MyClass();
void* operator new (size_t size, POOL_TYPE type, ULONG tag)
{
    POBJ_POOL_TRACK buffer = CreatePoolTrackEntry(size, type, tag);
    if (buffer != NULL)
        return (PVOID)(buffer->Ptr);
    return NULL;
}

//usage: char *ptr = new char[10];
void* __cdecl operator new[](size_t size)
{
    POBJ_POOL_TRACK buffer = CreatePoolTrackEntry(size, NonPagedPool);
    if (buffer != NULL)
    {
        buffer->IsArray = TRUE;
        return (PVOID)(buffer->Ptr);
    }
    return NULL;
}

//usage: char *ptr = new(NonPagedPool, MY_POOL_TAG) char[10];
void* operator new[](size_t size, POOL_TYPE type, ULONG tag)
{
    POBJ_POOL_TRACK buffer = CreatePoolTrackEntry(size, type, tag);
    if (buffer != NULL)
    {
        buffer->IsArray = TRUE;
        return (PVOID)(buffer->Ptr);
    }
    return NULL;
}

//usage: delete ptr;
void __cdecl operator delete (void* ptr, size_t size)
{
    DeletePoolTrackEntry(ptr, size);
}

//usage: delete ptr[];
void __cdecl operator delete[](void* ptr)
{
    DeletePoolTrackEntry(ptr);
}

void __cdecl operator delete[](void* ptr, size_t size)
{
    DeletePoolTrackEntry(ptr, size);
}
