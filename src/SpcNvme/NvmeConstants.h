#pragma once

namespace NVME_CONST{
    static const char* VENDOR_ID = "SPC     ";           //vendor name
    static const char* PRODUCT_ID = "SomkingPC NVMe  ";  //model name
    static const char* PRODUCT_REV = "0100";

    static const ULONG TX_PAGES = 512; //PRP1 + PRP2 MAX PAGES
    static const ULONG TX_SIZE = PAGE_SIZE * TX_PAGES;
    static const UCHAR SUPPORT_NAMESPACES = 4;
    static const ULONG UNSPECIFIC_NSID = 0;
    static const UCHAR MAX_TARGETS = 1;
    static const UCHAR MAX_LU = 1;
    static const ULONG MAX_IO_PER_LU = 4096;
    static const UCHAR IOSQES = 6; //sizeof(NVME_COMMAND)==64 == 2^6, so IOSQES== 6.
    static const UCHAR IOCQES = 4; //sizeof(NVME_COMPLETION_ENTRY)==16 == 2^4, so IOCQES== 4.
    static const UCHAR ADMIN_QUEUE_DEPTH = 64;  //how many entries does the admin queue have?
    
    static const USHORT CPL_INIT_PHASETAG = 1;  //CompletionQueue phase tag init value;
    static const UCHAR IO_QUEUE_COUNT = 4;
    static const UCHAR IO_QUEUE_DEPTH = 128;

    static const USHORT MAX_CID = MAXUSHORT;

    //static const USHORT SQ_CMD_SIZE = sizeof(NVME_COMMAND);
    //static const USHORT SQ_CMD_SIZE_SHIFT = 6; //sizeof(NVME_COMMAND) is 64 bytes == 2^6

    static const ULONG STALL_TIME_US = 50;  //in micro-seconds
    static const ULONG SLEEP_TIME_US = STALL_TIME_US*10;    //in micro-seconds

    #pragma region ======== SetFeature default values ========
    static const UCHAR INTCOAL_TIME = 8;   //Interrupt Coalescing time threshold in 100us unit.
    static const UCHAR INTCOAL_THRESHOLD = 16;   //Interrupt Coalescing trigger threshold.

    static const UCHAR AB_BURST = 7;        //Arbitration Burst. 111b(0n7) is Unlimit.
    static const UCHAR AB_HPW = 32 - 1;     //High Priority Weight. it is 0-based so need substract 1.
    static const UCHAR AB_MPW = 16 - 1;     //Medium Priority Weight. it is 0-based so need substract 1.
    static const UCHAR AB_LPW = 8 - 1;      //Low Priority Weight. it is 0-based so need substract 1.

    #pragma endregion
};
