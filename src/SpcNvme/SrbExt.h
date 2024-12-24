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
// Please keep my name in "author" field.
// 
// Enjoy it.
// ================================================================

typedef VOID SPC_SRBEXT_COMPLETION(struct _SPC_SRBEXT *srbext);
typedef SPC_SRBEXT_COMPLETION* PSPC_SRBEXT_COMPLETION;

//SPC stands for SmokingPC  ... :p
typedef struct _SPC_SRBEXT
{
    PVOID DevExt;
    PSCSI_REQUEST_BLOCK Srb;

    bool InitOK;
    bool IsCompleted;
    bool IsWrite;
    bool DeleteInComplete;
    UCHAR SrbStatus;        //returned SrbStatus for SyncCall of Admin cmd (e.g. IndeitfyController) 
    NVME_COMMAND NvmeCmd;
    NVME_COMPLETION_ENTRY NvmeCpl;

    PVOID Prp1VA;               //for debug tracking
    PHYSICAL_ADDRESS Prp1PA;    //for debug tracking
    PVOID Prp2VA;               //for debug tracking
    PHYSICAL_ADDRESS Prp2PA;    //for debug tracking
    PVOID Prp2List;             //if (PrpCount > 2), this field will assign to new page to store PRP2 Entries
    ULONG PrpCount;            //total PRP entries
    PSPC_SRBEXT_COMPLETION CompletionCB;
    
    //ExtraBuf is used to retrieve data by cmd. e.g. LogPage Buffer in GetLogPageForAsyncEvent().
    //It should be freed in CompletionCB.
    PVOID ExtraBuf;
    ULONG ExtraBufSize;        //size in bytes

#pragma region ======== Information parsed from SRB ========
    ULONG FunctionCode;        //SRB_FUNCTION_XXX code from srb
    PVOID DataBuffer;
    ULONG DataBufLen;
    USHORT StoragePort;
    UCHAR ScsiPath;
    UCHAR ScsiTarget;
    UCHAR ScsiLun;
    ULONG ScsiTag;          //scsi tag from storport, unique id for each LU
    PCDB Cdb;
    UCHAR CdbLen;
#pragma endregion ======== Information parsed from SRB ========

    #pragma region ======== for Debugging ========
    class CNvmeQueue *SubmittedQ;
    UINT32 SubTail;
    PNVME_COMMAND SubmitCmdPtr;
    #pragma endregion

    ULONG OverrunGuard;

    void Init(
        _In_ PVOID devext, 
        _In_ PSCSI_REQUEST_BLOCK srb, 
        _In_ PSPC_SRBEXT_COMPLETION callback = nullptr);
    void CleanUp();
    void CompleteSrb(_In_ UCHAR status);
    void CompleteSrb(_In_ NVME_COMMAND_STATUS& nvme_status);
    void SetDataBufTxLength(_In_ ULONG length);
    void ResetExtBuf(_In_ PVOID new_buffer = nullptr);
    bool GetPnpRequest(_Out_ STOR_PNP_ACTION& action, _Out_ ULONG& flags);
    PVOID AllocExtraBuf(ULONG size);

#if 0    
    bool BuildPrpByBuffer(PVOID buffer, UINT32 buf_size);
    inline bool BuildPrpByDataBuf()
    {
        this->BuildPrpByBuffer(this->DataBuffer, this->DataBufLen);
    }
    inline bool BuildPrpByExtraBuf()
    {
        this->BuildPrpByBuffer(this->ExtraBuf, this->ExtraBufSize);
    }
    inline bool IsSrbEx()
    {
        //in storport system, STORAGE_REQUEST_BLOCK also named as SRBEX.
        if(nullptr == Srb)
            return false;
        return (SRB_FUNCTION_STORAGE_REQUEST_BLOCK == SrbFuncCode);
    }
    inline bool IsScsiSrb()
    {
        if (nullptr == Srb)
            return false;
        return (SRB_FUNCTION_STORAGE_REQUEST_BLOCK != SrbFuncCode);
    }
#endif
}SPC_SRBEXT, * PSPC_SRBEXT;

__inline PSPC_SRBEXT GetSrbExt(_In_ PSCSI_REQUEST_BLOCK srb)
{
    return (PSPC_SRBEXT)SrbGetMiniportContext(srb);
}
__inline PSPC_SRBEXT InitAndGetSrbExt(
    _In_ PVOID devext,
    _In_ PSCSI_REQUEST_BLOCK srb,
    _In_ PSPC_SRBEXT_COMPLETION callback = nullptr)
{
    PSPC_SRBEXT srbext = GetSrbExt(srb);
    srbext->Init(devext, srb, callback);
    return srbext;
}

UCHAR NvmeToSrbStatus(NVME_COMMAND_STATUS& status);
UCHAR NvmeGenericToSrbStatus(NVME_COMMAND_STATUS &status);
UCHAR NvmeCmdSpecificToSrbStatus(NVME_COMMAND_STATUS &status);
UCHAR NvmeMediaErrorToSrbStatus(NVME_COMMAND_STATUS &status);
void SetScsiSenseBySrbStatus(PSCSI_REQUEST_BLOCK srb, UCHAR &status);
