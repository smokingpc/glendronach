#include "pch.h"

UCHAR Scsi_ReportLuns12(PSPCNVME_SRBEXT srbext)
{
//according SEAGATE SCSI reference, SCSIOP_REPORT_LUNS
//is used to query SCSI Logical Unit class address.
//each address are 8 bytes. In current windows system 
//I can't find reference data to determine LU address.
//MSDN also said it's NOT recommend to translate this SCSI 
//command for NVMe device.
//So I skip this command....
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR Scsi_Read12(PSPCNVME_SRBEXT srbext)
{
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB12, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, false);
}

UCHAR Scsi_Write12(PSPCNVME_SRBEXT srbext)
{
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB12, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, true);
}

UCHAR Scsi_Verify12(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;

    ////todo: complete this handler for FULL support of verify
    //UCHAR srb_status = SRB_STATUS_ERROR;
    //CRamdisk* disk = srbext->NvmeDev->RamDisk;
    //PCDB cdb = srbext->Cdb;
    //UINT32 lba_start = 0;    //in Blocks, not bytes
    //REVERSE_BYTES_4(&lba_start, cdb->CDB12.LogicalBlock);

    //UINT32 verify_len = 0;    //in Blocks, not bytes
    //REVERSE_BYTES_4(&verify_len, &cdb->CDB12.TransferLength);

    //if (FALSE == disk->IsExceedLbaRange(lba_start, verify_len))
    //    srb_status = SRB_STATUS_SUCCESS;

    //SrbSetDataTransferLength(srbext->Srb, 0);
    //return srb_status;
}

//In windows, SED(Self Encrypted Disk) features are implemented by 
//SCSIOP_SECURITY_PROTOCOL_IN and SCSIOP_SECURITY_PROTOCOL_OUT.
//SED tool (e.g. sed-utils) send SED cmd to disk via IOCTL_SCSI_PASS_THROUGH_DIRECT,
//which carried CDB data with SCSIOP_SECURITY_PROTOCOL_IN and SCSIOP_SECURITY_PROTOCOL_OUT.
//Then in disk.sys it picks CDB data and send to NVMe driver via SCSI command.

//SCSIOP_SECURITY_PROTOCOL_IN => Host retrieve security protocol data from device
UCHAR Scsi_SecurityProtocolIn(PSPCNVME_SRBEXT srbext)
{
    UCHAR srb_status = SRB_STATUS_SUCCESS;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    BOOLEAN is_support = srbext->NvmeDev->CtrlIdent.OACS.SecurityCommands;
    if(!is_support)
        return SRB_STATUS_ERROR;

    //Note: In this command , payload data should be aligned to block size 
    //of namespace format. Usually it is PAGE_SIZE from app.
    BuildCmd_AdminSecurityRecv(srbext, DEFAULT_CTRLID, srbext->Cdb());
    status = srbext->NvmeDev->SubmitAdmCmd(srbext, &srbext->NvmeCmd);
    if (!NT_SUCCESS(status))
        srb_status = SRB_STATUS_ERROR;
    else
        srb_status = SRB_STATUS_PENDING;

    return srb_status;
}
//SCSIOP_SECURITY_PROTOCOL_OUT => Host send security protocol data to device
UCHAR Scsi_SecurityProtocolOut(PSPCNVME_SRBEXT srbext)
{
    UCHAR srb_status = SRB_STATUS_SUCCESS;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    BOOLEAN is_support = srbext->NvmeDev->CtrlIdent.OACS.SecurityCommands;
    if (!is_support)
        return SRB_STATUS_ERROR;

    //Note: In this command , payload data should be aligned to block size 
    //of namespace format. Usually it is PAGE_SIZE from app.
    BuildCmd_AdminSecuritySend(srbext, DEFAULT_CTRLID, srbext->Cdb());
    status = srbext->NvmeDev->SubmitAdmCmd(srbext, &srbext->NvmeCmd);
    if (!NT_SUCCESS(status))
        srb_status = SRB_STATUS_ERROR;
    else
        srb_status = SRB_STATUS_PENDING;

    return srb_status;
}
