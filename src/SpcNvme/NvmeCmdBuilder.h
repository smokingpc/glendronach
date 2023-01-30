#pragma once

void BuildCmd_IdentCtrler(PNVME_COMMAND cmd, PNVME_IDENTIFY_CONTROLLER_DATA data);
void BuildCmd_RegisterIoSubQ(PNVME_COMMAND cmd, CNvmeQueue* queue);
void BuildCmd_RegisterIoCplQ(PNVME_COMMAND cmd, CNvmeQueue* queue);
void BuildCmd_UnRegisterIoSubQ(PNVME_COMMAND cmd, CNvmeQueue* queue);
void BuildCmd_UnRegisterIoCplQ(PNVME_COMMAND cmd, CNvmeQueue* queue);
