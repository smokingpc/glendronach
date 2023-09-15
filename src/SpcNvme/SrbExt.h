#pragma once
// ================================================================
// SpcNvme : OpenSource NVMe Driver for Windows 8+
// Author : Roy Wang(SmokingPC).
// Licensed by MIT License.
// 
// Copyright (C) 2022, Roy Wang (SmokingPC)
// https://github.com/smokingpc/
// 
// NVMe Spec: https://nvmexpress.org/specifications/
// Contact Me : smokingpc@gmail.com
// ================================================================
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this softwareand associated documentation files(the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and /or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
// IN THE SOFTWARE.
// ================================================================
// [Additional Statement]
// This Driver is implemented by NVMe Spec 1.3 and Windows Storport Miniport Driver.
// You can copy, modify, redistribute the source code. 
// 
// There is only one requirement to use this source code:
// PLEASE DO NOT remove or modify the "original author" of this codes.
// Keep "original author" declaration unmodified.
// 
// Enjoy it.
// ================================================================


class CNvmeDevice;
struct _SPCNVME_SRBEXT;

typedef VOID SPC_SRBEXT_COMPLETION(struct _SPCNVME_SRBEXT *srbext);
typedef SPC_SRBEXT_COMPLETION* PSPC_SRBEXT_COMPLETION;

typedef struct _SPCNVME_SRBEXT
{
    static _SPCNVME_SRBEXT *GetSrbExt(PSTORAGE_REQUEST_BLOCK srb);
	static _SPCNVME_SRBEXT* InitSrbExt(PVOID devext, PSTORAGE_REQUEST_BLOCK srb);

    CNvmeDevice *DevExt;
    PSTORAGE_REQUEST_BLOCK Srb;
    UCHAR SrbStatus;        //returned SrbStatus for SyncCall of Admin cmd (e.g. IndeitfyController) 
    BOOLEAN InitOK;
    BOOLEAN FreePrp2List;
    BOOLEAN DeleteInComplete;
    BOOLEAN IsCompleted;
    NVME_COMMAND NvmeCmd;
    NVME_COMPLETION_ENTRY NvmeCpl;
    PVOID Prp2VA;
    PHYSICAL_ADDRESS Prp2PA;
    PSPC_SRBEXT_COMPLETION CompletionCB;
    //ExtBuf is used to retrieve data by cmd. e.g. LogPage Buffer in GetLogPage().
    //It should be freed in CompletionCB.
    PVOID ExtBuf;      
    #pragma region ======== for Debugging ========
    class CNvmeQueue *SubmittedQ;
    ULONG IoQueueIndex;
    PNVME_COMMAND SubmittedCmd;
    ULONG Tag;
    ULONG SubIndex;
    #pragma endregion

    void Init(PVOID devext, STORAGE_REQUEST_BLOCK *srb);
    void CleanUp();
    void CompleteSrb(UCHAR status);
    void CompleteSrb(NVME_COMMAND_STATUS& nvme_status);
    ULONG FuncCode();         //SRB Function Code
    ULONG ScsiQTag();
    PCDB Cdb();
    UCHAR CdbLen();
    UCHAR PathID();           //SCSI Path (bus) ID
    UCHAR TargetID();         //SCSI Device ID
    UCHAR Lun();              //SCSI Logical UNit ID
    PVOID DataBuf();
    ULONG DataBufLen();
    void SetTransferLength(ULONG length);
    void ResetExtBuf(PVOID new_buffer = NULL);
    PSRBEX_DATA_PNP SrbDataPnp();
}SPCNVME_SRBEXT, * PSPCNVME_SRBEXT;

UCHAR NvmeToSrbStatus(NVME_COMMAND_STATUS& status);
UCHAR NvmeGenericToSrbStatus(NVME_COMMAND_STATUS &status);
UCHAR NvmeCmdSpecificToSrbStatus(NVME_COMMAND_STATUS &status);
UCHAR NvmeMediaErrorToSrbStatus(NVME_COMMAND_STATUS &status);

void SetScsiSenseBySrbStatus(PSTORAGE_REQUEST_BLOCK srb, UCHAR &status);
