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


#define SUPPORT_NUMBER_DEVICE_USB 24
#define NUMBER_ELLEMENTS_RECESIVE 10000

#define ID_AVE_FRAME_START   (const uint32_t)0x22446688
#define ID_DUMP_FRAME_START  (const uint32_t)0x336699FF

typedef struct 
{
    void* (*threads_cdc)(void*);
    pthread_t pthread;
    COM_Ports_Handle_t* COM_Ports_Handle;
    uint16_t TotalNumberOfDevice;
}Thread_CDC_Device_t;

Thread_CDC_Device_t Thread_CDC_Device;
pthread_t pthread_display;



void* thread_cdc_generic(void* arg);
void* thread_display(void* arg);

