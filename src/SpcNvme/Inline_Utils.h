#pragma once

FORCEINLINE size_t DivRoundUp(size_t value, size_t align_size)
{
    return (size_t)((value + (align_size-1))/align_size);
}

FORCEINLINE size_t RoundUp(size_t value, size_t align_size)
{
    return (DivRoundUp(value, align_size) * align_size);
}
