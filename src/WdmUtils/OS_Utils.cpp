#include "pch.h"

bool IsSupportedOS(ULONG major, ULONG minor)
{
    OSVERSIONINFOW info = { 0 };
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    NTSTATUS status = RtlGetVersion(&info);
    if (NT_SUCCESS(status))
    {
        if(info.dwMajorVersion >= major)
            return true;
        if(info.dwMajorVersion == major && info.dwMinorVersion >= minor)
            return true;
    }
    return false;
}

