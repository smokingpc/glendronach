#pragma once

typedef struct _SPCNVME_DEVEXT
{
    NVME_STATE State;
    CSpcNvmeDevice NvmeDev;

}SPCNVME_DEVEXT, * PSPCNVME_DEVEXT;

typedef struct _SPCNVME_SRBEXT
{
    PSTORAGE_REQUEST_BLOCK Srb;
}SPCNVME_SRBEXT, * PSPCNVME_SRBEXT;
