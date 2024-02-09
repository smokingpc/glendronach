#include "pch.h"

void ParseMsiCaps(
    PCI_COMMON_CONFIG* ptr,
    PPCI_MSI_CAP &msi,
    PPCI_MSIX_CAP &msix)

{
    PUCHAR begin = (PUCHAR)ptr;
    PUCHAR cursor = NULL;
    UINT8 offset = (UINT8)(ptr->u.type0.CapabilitiesPtr);
    //begin of each node in CapList has 2 bytes. 
    //cursor[0] is id and cursor[1] is "next cap offset from begin ptr"
    do {
        cursor = begin + offset;

        switch (cursor[0])
        {
        case PCI_CAPABILITY_ID_MSI:
            msi = (PPCI_MSI_CAP)cursor;
            break;
        case PCI_CAPABILITY_ID_MSIX:
            msix = (PPCI_MSIX_CAP)cursor;
            break;
        }
        offset = cursor[1];
    } while (((cursor - begin) < sizeof(PCI_COMMON_CONFIG) - 1) && offset > 0);
}
