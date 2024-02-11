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


//Using this structure represents the DeviceExtension.
//And CNvmeDevice inherit this structure to apply behaviors.
typedef struct _NVME_DEVEXT{

}NVME_DEVEXT, *PNVME_DEVEXT;


//Using this class represents the DeviceExtension.
//Because memory allocation in kernel is still C style,
//the constructor and destructor are useless. They won't be called.
//So using Init() and Teardown() to replace them.
class CNvmeDevice{// : public NVME_DEVEXT {
public:
    static const ULONG BUGCHECK_BASE = 0x23939889;          //pizzahut....  XD
    static const ULONG BUGCHECK_ADAPTER = BUGCHECK_BASE + 1;            //adapter has some problem. e.g. CSTS.CFS==1
    static const ULONG BUGCHECK_INVALID_STATE = BUGCHECK_BASE + 2;      //if action in invalid controller state, fire this bugcheck
    static const ULONG BUGCHECK_NOT_IMPLEMENTED = BUGCHECK_BASE + 10;
    static BOOLEAN NvmeMsixISR(IN PVOID devext, IN ULONG msgid);
    static void RestartAdapterDpc(
            IN PSTOR_DPC  Dpc,
            IN PVOID  NvmeDev,
            IN PVOID  Arg1,
            IN PVOID  Arg2);
    static void RestartAdapterWorker(
        _In_ PVOID NvmeDev,
        _In_ PVOID Context,
        _In_ PVOID Worker);
    static VOID HandleAsyncEvent(
        _In_ PSPCNVME_SRBEXT srbext);
    static VOID HandleErrorInfoLogPage(
        _In_ PSPCNVME_SRBEXT srbext);
    static VOID HandleSmartInfoLogPage(
        _In_ PSPCNVME_SRBEXT srbext);
    static VOID HandleFwSlotInfoLogPage(
        _In_ PSPCNVME_SRBEXT srbext);
public:
    bool ReadCacheEnabled;
    bool WriteCacheEnabled;
    BOOLEAN RebalancingPnp;
    ULONG MinPageSize;
    ULONG MaxPageSize;
    ULONG MaxTxSize;
    ULONG MaxTxPages;
    NVME_STATE State;
    ULONG RegisteredIoQ;
    ULONG AllocatedIoQ;
    ULONG DesiredIoQ;
    ULONG DeviceTimeout;//should be updated by CAP, unit in micro-seconds
    ULONG StallDelay;
    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT];         //AccessRange from miniport HwFindAdapter.
    ULONG AccessRangeCount;
    ULONG Bar0Size;
    UCHAR MaxNamespaces;
    USHORT IoDepth;
    USHORT AdmDepth;
    ULONG NamespaceCount;       //how many namespace active in current device?

    UCHAR CoalescingThreshold;  //how many interrupt should be coalesced into one interrupt?
    UCHAR CoalescingTime;       //how long(100us unit) should interrupts be coalesced?

    USHORT  VendorID;
    USHORT  DeviceID;

    NVME_VERSION                        NvmeVer;
    NVME_CONTROLLER_CAPABILITIES        CtrlCap;
    NVME_IDENTIFY_CONTROLLER_DATA       CtrlIdent;
    NVME_IDENTIFY_NAMESPACE_DATA        NsData[SUPPORT_NAMESPACES];

    //these 2 DPC and WorkItem are used for HwAdapterControl::ScsiRestartAdapter event.
    PVOID                               RestartWorker;
    STOR_DPC                            RestartDpc;
    ULONG                               CpuCount;
    PGROUP_AFFINITY                     MsgGroupAffinity;

    PNVME_CONTROLLER_REGISTERS          CtrlReg;
    PPORT_CONFIGURATION_INFORMATION     PortCfg;
    volatile ULONG* Doorbells;
    CNvmeQueue* AdmQueue;
    CNvmeQueue* IoQueue[MAX_IO_QUEUE_COUNT];
    PVOID UncachedExt;
    ULONG MsixCount;
    //note: if extend AsyncEvent to multiple event, here should be refactor to 
    //      make saving AsyncEventLog atomic.
    NVME_COMPLETION_DW0_ASYNC_EVENT_REQUEST AsyncEventLog[MAX_ASYNC_EVENT_LOG];
    ULONG CurrentAsyncEvent;
    PVOID AsyncEventLogPage[MAX_ASYNC_EVENT_LOGPAGES];
    ULONG CurrentLogPage = MAXULONG;

    //Following are huge data.
    //for more convenient windbg debugging, I put them on tail of class data.
    PCI_COMMON_CONFIG                   CopiedPciCfg;
    PPCI_COMMON_CONFIG                  PciCfgPtr;
    PVOID DevExt;

    ULONG Guard;
public:
    #if 0
    CNvmeDevice();
    ~CNvmeDevice();
    NTSTATUS Setup(PPORT_CONFIGURATION_INFORMATION pci);
    #endif
    NTSTATUS Setup(PVOID devext, PVOID pcidata, PVOID ctrlreg);
    void EnableMsix();
    void DisableMsix();
    void UpdateMsixTable();
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

    NTSTATUS IdentifyAllNamespaces();
    NTSTATUS IdentifyFirstNamespace();
    NTSTATUS CreateIoQueues(bool force = false);    //if(force) => delete exist queue objects and recreate again.

    NTSTATUS IdentifyController(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_CONTROLLER_DATA ident, bool poll = false);
    NTSTATUS IdentifyNamespace(PSPCNVME_SRBEXT srbext, ULONG nsid, PNVME_IDENTIFY_NAMESPACE_DATA data);
    //nsid_list : variable to store query result. It's size should be PAGE_SIZE.(NVMe max support 1024 NameSpace)
    NTSTATUS IdentifyActiveNamespaceIdList(PSPCNVME_SRBEXT srbext, PVOID nsid_list, ULONG &ret_count);

    NTSTATUS SetNumberOfIoQueue(USHORT count, bool poll = false);  //tell NVMe device: I want xx i/o queues. then device reply: I can allow you use xxxx queues.
    NTSTATUS SetInterruptCoalescing();
    NTSTATUS SetAsyncEvent();
    NTSTATUS RequestAsyncEvent();
    NTSTATUS GetLogPageForAsyncEvent(UCHAR logid);
    NTSTATUS SetArbitration();
    NTSTATUS SetSyncHostTime();
    NTSTATUS SetPowerManagement();
    NTSTATUS SetHostBuffer();
    NTSTATUS GetLbaFormat(ULONG nsid, NVME_LBA_FORMAT &format);
    NTSTATUS GetNamespaceBlockSize(ULONG nsid, ULONG& size);    //get LBA block size in Bytes
    NTSTATUS GetNamespaceTotalBlocks(ULONG nsid, ULONG64& blocks);    //get LBA total block count of specified namespace.
    NTSTATUS SubmitAdmCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd);
    NTSTATUS SubmitIoCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd);
    void ReleaseOutstandingSrbs();
    NTSTATUS SetPerfOpts();
    void SaveAsyncEvent(PNVME_COMPLETION_DW0_ASYNC_EVENT_REQUEST event);
    void SaveAsyncEventLogPage(PVOID page);
    bool IsFitValidIoRange(ULONG nsid, ULONG64 offset, ULONG len);
    bool IsNsExist(ULONG nsid);
    bool IsLunExist(UCHAR lun);
    bool IsWorking();
    bool IsSetup();
    bool IsTeardown();
    bool IsStop();

protected:
    void InitVars();
    void InitDpcs();
    void LoadRegistry();

    NTSTATUS CreateAdmQ();
    NTSTATUS RegisterAdmQ();
    NTSTATUS UnregisterAdmQ();
    NTSTATUS DeleteAdmQ();

    NTSTATUS CreateIoQ();   //create all IO queue
    NTSTATUS DeleteIoQ();   //delete all IO queue

    void ReadCtrlCap();      //load capability and informations AFTER register address mapped.
    bool MapCtrlRegisters();
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
};
