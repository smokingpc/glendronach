#pragma once

__inline PSPCNVME_SRBEXT GetSrbExt(PSTORAGE_REQUEST_BLOCK srb)
{
    return (PSPCNVME_SRBEXT)SrbGetMiniportContext(srb);
}

PSPCNVME_SRBEXT InitAndGetSrbExt(PVOID devext, PSTORAGE_REQUEST_BLOCK srb);

