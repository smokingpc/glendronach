
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include "pch.h"

__inline void DebugCallIn(const char* func_name, const char* prefix)
{
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_FILTER, "%s [%s] IN =>\n", prefix, func_name); 
}
__inline void DebugCallIn(const char* func_name)
{
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_FILTER, "[%s] IN =>\n", func_name);
}
__inline void DebugCallOut(const char* func_name, const char* prefix)
{
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_FILTER, "%s [%s] OUT <=\n", prefix, func_name);
}
__inline void DebugCallOut(const char* func_name)
{
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_FILTER, "[%s] OUT <=\n", func_name);
}

CDebugCallInOut::CDebugCallInOut(const char* name)
    : CDebugCallInOut((char*)name){}

CDebugCallInOut::CDebugCallInOut(char* name)
{
    this->Name = (char*) new(NonPagedPoolNx, CALLINOUT_TAG) char[this->BufSize+1];
    if(NULL != this->Name)
    {
        memcpy(this->Name, name, BufSize);
        DebugCallIn(this->Name, DEBUG_PREFIX);
    }
}

CDebugCallInOut::~CDebugCallInOut()
{
    if (NULL != this->Name)
    {
        DebugCallOut(this->Name, DEBUG_PREFIX);
        delete[] Name;
    }
}
