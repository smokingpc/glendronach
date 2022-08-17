#pragma once

typedef enum _NVME_STATE{
    STOP = 0,
    SETUP = 1,
    RUNNING = 2,
    TEARDOWN = 3,
    RESET = 4,
}NVME_STATE;
