#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/epoll.h>
#include <mqueue.h>
#include "usb_com.h"
#include "display_data.h"

// Глобальные макросы пользовательской программы 
#define SUPPORT_NUMBER_DEVICE_USB 24
#define NUMBER_ELLEMENTS_RECESIVE 10000

// ID head-ров сообщений от Device по их типу 
#define ID_AVE_FRAME_START   (const uint32_t)0x22446688
#define ID_DUMP_FRAME_START  (const uint32_t)0x336699FF

typedef enum{
    QUEUE_PASS_STATE,
    QUEUE_WAIT_STATE
}Queue_state_t;

typedef struct{
    ModulData_t *data;
    int head; // Индекс записи (указывает на +1 к текущем записанным данным)
    int tail; // Индекс чтения (указыыает на самые старые данные)
    int count; // Колличество текущих элементов 

    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full; // Опционально, если очередь ограничена

    uint32_t len; // Общий рамер очереди в элементах очереди  
    char name[30]; // Название очереди 
}Queue_Handle_t;



typedef struct 
{
    void* (*threads_cdc)(void*);
    pthread_t pthread;
    COM_Ports_Handle_t* COM_Ports_Handle;
    uint16_t TotalNumberOfDevice;
}Thread_CDC_Device_t;

Thread_CDC_Device_t Thread_CDC_Device;
pthread_t pthread_display;
extern Queue_Handle_t Queue_Dump;


uint8_t Queue_Init(Queue_Handle_t* Queue, uint32_t len);
void Queue_Push(Queue_Handle_t* Queue, ModulData_t* data_ptr, Queue_state_t Mode);
ModulData_t* Queue_Pop(Queue_Handle_t *Queue, Queue_state_t Mode);
void* thread_cdc_generic(void* arg);
void* thread_display(void* arg);

