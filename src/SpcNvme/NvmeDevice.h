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


typedef struct _DOORBELL_PAIR{
    NVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubTail;
    NVME_COMPLETION_QUEUE_HEAD_DOORBELL CplHead;
}DOORBELL_PAIR, *PDOORBELL_PAIR;

//HW_MESSAGE_SIGNALED_INTERRUPT_ROUTINE NvmeMsixISR;

//Using this class represents the DeviceExtension.
//Because memory allocation in kernel is still C style,
//the constructor and destructor are useless. They won't be called.
//So using Setup() and Teardown() to replace them.
class CNvmeDevice {
public:
    static const ULONG BUGCHECK_BASE = 0x23939889;          //pizzahut....  XD
    static const ULONG BUGCHECK_ADAPTER = BUGCHECK_BASE + 1;            //adapter has some problem. e.g. CSTS.CFS==1
    static const ULONG BUGCHECK_INVALID_STATE = BUGCHECK_BASE + 2;      //if action in invalid controller state, fire this bugcheck
    static const ULONG BUGCHECK_NOT_IMPLEMENTED = BUGCHECK_BASE + 10;
    static const ULONG DEV_POOL_TAG = (ULONG) 'veDN';
    static BOOLEAN NvmeMsixISR(IN PVOID devext, IN ULONG msgid);
    static void RestartAdapterDpc(
            IN PSTOR_DPC  Dpc,
            IN PVOID  DevExt,
            IN PVOID  Arg1,
            IN PVOID  Arg2);
    static void RestartAdapterWorker(
        _In_ PVOID DevExt,
        _In_ PVOID Context,
        _In_ PVOID Worker);
public:
    NTSTATUS Setup(PPORT_CONFIGURATION_INFORMATION pci);
    void Teardown();

    NTSTATUS EnableController();
    NTSTATUS DisableController();
    NTSTATUS ShutdownController();  //set CC.SHN and wait CSTS.SHST==2

    NTSTATUS InitController();
    NTSTATUS InitNvmeStage1();      //InitNvmeStage1() should be called AFTER HwFindAdapte because it need interrupt.
    NTSTATUS InitNvmeStage2();      //InitNvmeStage1() should be called AFTER HwFindAdapte because it need interrupt.
    NTSTATUS RestartController();   //for AdapterControl's ScsiRestartAdaptor

    NTSTATUS RegisterIoQueues(PSPCNVME_SRBEXT srbext);
    NTSTATUS UnregisterIoQueues(PSPCNVME_SRBEXT srbext);

    //NTSTATUS InitIdentifyCtrl();
    NTSTATUS IdentifyAllNamespaces();
    NTSTATUS IdentifyFirstNamespace();
    NTSTATUS CreateIoQueues(bool force = false);    //if(force) => delete exist queue objects and recreate again.

    NTSTATUS IdentifyController(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_CONTROLLER_DATA ident, bool poll = false);
    NTSTATUS IdentifyNamespace(PSPCNVME_SRBEXT srbext, ULONG nsid, PNVME_IDENTIFY_NAMESPACE_DATA data);
    //nsid_list : variable to store query result. It's size should be PAGE_SIZE.(NVMe max support 1024 NameSpace)
    NTSTATUS IdentifyActiveNamespaceIdList(PSPCNVME_SRBEXT srbext, PVOID nsid_list, ULONG &ret_count);

    NTSTATUS SetNumberOfIoQueue(USHORT count);  //tell NVMe device: I want xx i/o queues. then device reply: I can allow you use xxxx queues.
    NTSTATUS SetInterruptCoalescing();
    NTSTATUS SetAsyncEvent();
    NTSTATUS RequestAsyncEvent();
    NTSTATUS SetArbitration();
    NTSTATUS SetSyncHostTime(PSPCNVME_SRBEXT srbext = NULL);
    NTSTATUS SetPowerManagement();
    NTSTATUS SetHostBuffer();
    NTSTATUS GetLbaFormat(ULONG nsid, NVME_LBA_FORMAT &format);
    NTSTATUS GetNamespaceBlockSize(ULONG nsid, ULONG& size);    //get LBA block size in Bytes
    NTSTATUS GetNamespaceTotalBlocks(ULONG nsid, ULONG64& blocks);    //get LBA total block count of specified namespace.
    NTSTATUS SubmitAdmCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd);
    NTSTATUS SubmitIoCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd);
    void ResetOutstandingCmds();
    NTSTATUS SetPerfOpts();
    bool IsInValidIoRange(ULONG nsid, ULONG64 offset, ULONG len);

    bool IsWorking();
    bool IsSetup();
    bool IsTeardown();
    bool IsStop();

    bool ReadCacheEnabled = false;
    bool WriteCacheEnabled = false;
    BOOLEAN RebalancingPnp = FALSE;
    ULONG MinPageSize = PAGE_SIZE;
    ULONG MaxPageSize = PAGE_SIZE;
    ULONG MaxTxSize = 0;
    ULONG MaxTxPages = 0;
    NVME_STATE State = NVME_STATE::STOP;
    ULONG RegisteredIoQ = 0;
    ULONG AllocatedIoQ = 0;
    ULONG DesiredIoQ = NVME_CONST::IO_QUEUE_COUNT;

    ULONG DeviceTimeout = 2000 * NVME_CONST::STALL_TIME_US;//should be updated by CAP, unit in micro-seconds
    ULONG StallDelay = NVME_CONST::STALL_TIME_US;

    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT] = {0};         //AccessRange from miniport HwFindAdapter.
    ULONG AccessRangeCount = 0;
    ULONG Bar0Size = 0;
    UCHAR MaxNamespaces = NVME_CONST::SUPPORT_NAMESPACES;
    USHORT IoDepth = NVME_CONST::IO_QUEUE_DEPTH;
    USHORT AdmDepth = NVME_CONST::ADMIN_QUEUE_DEPTH;
    ULONG TotalNumaNodes = 0;
    ULONG NamespaceCount = 0;       //how many namespace active in current device?
    
    UCHAR CoalescingThreshold = DEFAULT_INT_COALESCE_COUNT;  //how many interrupt should be coalesced into one interrupt?
    UCHAR CoalescingTime = DEFAULT_INT_COALESCE_TIME;       //how long(100us unit) should interrupts be coalesced?

    USHORT  VendorID = 0;
    USHORT  DeviceID = 0;
    ULONG   CpuCount = 0;
    long    OutstandAsyncEvent = 0;
    volatile long    MaxAsyncEvent = 0;
    volatile long    AsyncEventCid = 0;
    NVME_VERSION                        NvmeVer = {0};
    NVME_CONTROLLER_CAPABILITIES        CtrlCap = {0};
    NVME_IDENTIFY_CONTROLLER_DATA       CtrlIdent = {0};
    NVME_IDENTIFY_NAMESPACE_DATA        NsData[NVME_CONST::SUPPORT_NAMESPACES] = {0};
    
    //these 2 DPC and WorkItem are used for HwAdapterControl::ScsiRestartAdapter event.
    PVOID                               RestartWorker = NULL;
    STOR_DPC                            RestartDpc;
    GROUP_AFFINITY                      MsgGroupAffinity[NVME_CONST::MAX_INT_COUNT] = {0};
private:
    PNVME_CONTROLLER_REGISTERS          CtrlReg = NULL;
    PPORT_CONFIGURATION_INFORMATION     PortCfg = NULL;
    ULONG *Doorbells = NULL;
    PMSIX_TABLE_ENTRY MsixTable = NULL;
    CNvmeQueue* AdmQueue = NULL;
    CNvmeQueue* IoQueue[MAX_IO_QUEUE_COUNT] = {0};

    //Following are huge data.
    //for more convenient windbg debugging, I put them on tail of class data.
    PCI_COMMON_CONFIG                   PciCfg;

    void InitVars();
    void LoadRegistry();

    NTSTATUS CreateAdmQ();
    NTSTATUS RegisterAdmQ();
    NTSTATUS UnregisterAdmQ();
    NTSTATUS DeleteAdmQ();

    NTSTATUS CreateIoQ();   //create all IO queue
    NTSTATUS DeleteIoQ();   //delete all IO queue

    void ReadCtrlCap();      //load capability and informations AFTER register address mapped.
    bool MapCtrlRegisters();
    //bool GetMsixTable();
    bool GetPciBusData(INTERFACE_TYPE type, ULONG bus, ULONG slot);

    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy);
    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy, BOOLEAN cc_en);
    bool WaitForCtrlerShst(ULONG time_us);

    void ReadNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier = true);
    void ReadNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier = true);
    void ReadNvmeRegister(NVME_VERSION &ver, bool barrier = true);
    void ReadNvmeRegister(NVME_CONTROLLER_CAPABILITIES &cap, bool barrier = true);
    void ReadNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES& aqa,
                        NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS& asq,
                        NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS& acq,
                        bool barrier = true);

    void WriteNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier = true);
    void WriteNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier = true);
    void WriteNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES &aqa, 
                        NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS &asq, 
                        NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS &acq, 
                        bool barrier = true);
    
    void GetAdmQueueDbl(PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL &sub, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL &cpl);
    void GetQueueDbl(ULONG qid, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL& sub, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL& cpl);

    BOOLEAN IsControllerEnabled(bool barrier = true);
    BOOLEAN IsControllerReady(bool barrier = true);
    void UpdateParamsByCtrlIdent();
    USHORT NextAsyncEventCid();
    //void DoQueueCompletion(CNvmeQueue* queue);
};

