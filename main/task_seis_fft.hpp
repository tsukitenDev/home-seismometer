#pragma once


#define BIT_TASK_PROCESS_SHINDO_READY BIT0
#define BIT_TASK_PROCESS_SHINDO_RUN BIT1
#define BIT_TASK_PROCESS_SHINDO_RUNNING BIT2


enum class SENSOR_STATE {
    CONNECTED,
    NOT_CONNECTED,
    CONNECT_FAILED
};

enum class SEISMOMETER_STATE {
    SHINDO_STABILIZED,
    NOT_STARTED,
    HPF_STABILIZING,
    SHINDO_STABILIZING,
};

void task_process_shindo_fft(void * pvParameters);

void task_acc_read_fft(void * pvParameters);
