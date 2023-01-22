#pragma once

#define NVME_INVALID_ID     ((ULONG)-1)
#define NVME_INVALID_CID    ((USHORT)-1)        //should align to NVME CID size
#define NVME_INVALID_QID    ((USHORT)-1)
//#define INVALID_DBL_TAIL    ((USHORT)-1)
//#define INVALID_DBL_HEAD    INVALID_DBL_TAIL
#define INVALID_DBL_VALUE   ((USHORT)-1)
#define INIT_CPL_DBL_HEAD   0


#define MAX_LOGIC_UNIT      1
#define MAX_IO_QUEUE_COUNT  64

//#define MAX_TX_PAGES        32
//#define MAX_TX_SIZE         (PAGE_SIZE * MAX_TX_PAGES)
#define MAX_CONCURRENT_IO   4096

#define ACCESS_RANGE_COUNT  2

#pragma region  ======== SCSI and SRB RELATED ========
#define SRB_FUNCTION_SPC_INTERNAL   0xFF
#define INVALID_PATH_ID      ((UCHAR)~0)
#define INVALID_TARGET_ID    ((UCHAR)~0)
#define INVALID_LUN_ID       ((UCHAR)~0)
#define INVALID_SRB_QUEUETAG ((ULONG)~0)
#pragma endregion

#pragma region  ======== NVME_RELATED ========
#pragma endregion

