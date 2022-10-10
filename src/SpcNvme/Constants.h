#pragma once

#define NVME_INVALID_ID     ((ULONG)-1)
#define NVME_INVALID_CID    ((USHORT)-1)
#define NVME_INVALID_QID    ((USHORT)-1)
#define INVALID_DBL_TAIL    ((USHORT)-1)
#define INVALID_DBL_HEAD    INVALID_DBL_TAIL
#define INIT_CPL_DBL_HEAD   0


#define MAX_LOGIC_UNIT      1
#define MAX_IO_QUEUE_COUNT  32

#define MAX_TX_PAGES        32
#define MAX_TX_SIZE         (PAGE_SIZE * MAX_TX_PAGES)
#define MAX_CONCURRENT_IO   4096

#define ACCESS_RANGE_COUNT  2