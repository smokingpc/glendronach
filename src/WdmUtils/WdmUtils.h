#pragma once
#define CPP_TAG             (ULONG)'MPPC'       //tag == 'CPPM' in memory
void* operator new (size_t size, POOL_TYPE type, ULONG tag = CPP_TAG);
void* operator new[](size_t size, POOL_TYPE type, ULONG tag = CPP_TAG);

void DebugSrbFunctionCode(ULONG code);
void DebugScsiOpCode(UCHAR opcode);
bool IsSupportedOS(ULONG major, ULONG minor = 0);

class CDebugCallInOut
{
private:
    static const int BufSize = 64;
    char* Name = NULL;
public:
    CDebugCallInOut(char* name);
    CDebugCallInOut(const char* name);
    ~CDebugCallInOut();
};
