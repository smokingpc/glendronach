#pragma once

void BuildCmd_IdentCtrler(PNVME_COMMAND cmd, PNVME_IDENTIFY_CONTROLLER_DATA data);
void BuildCmd_IdentNamespace(PNVME_COMMAND cmd, PNVME_IDENTIFY_NAMESPACE_DATA data, ULONG nsid);

void BuildCmd_RegIoSubQ(PNVME_COMMAND cmd, CNvmeQueue* queue);
void BuildCmd_RegIoCplQ(PNVME_COMMAND cmd, CNvmeQueue* queue);
void BuildCmd_UnRegIoSubQ(PNVME_COMMAND cmd, CNvmeQueue* queue);
void BuildCmd_UnRegIoCplQ(PNVME_COMMAND cmd, CNvmeQueue* queue);

void BuildCmd_InterruptCoalescing(PNVME_COMMAND cmd, UCHAR threshold, UCHAR interval);
void BuildCmd_SetArbitration(PNVME_COMMAND cmd);