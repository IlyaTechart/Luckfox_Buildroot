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
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h> // Для pipe()
#include "usb_com.h"
#include "display_data.h"

// Глобальные макросы пользовательской программы 
#define SUPPORT_NUMBER_DEVICE_USB 24
#define NUMBER_ELLEMENTS_RECESIVE 10000
#define TAKE_HEAP_MEMORY_FOR_ELEMENTS 10000


#define SIZE_QUEUE_DISPLAY_ELEMENTS 20


/// @brief "Труба" для передачи 
int hotplug_pipe[2]; // Работает как файловый дискриптор 

// Какие бывают команды от охранника
typedef enum {
    USB_ACTION_ADD,
    USB_ACTION_REMOVE
} UsbAction_t;

// Сама "записка", которую закидывает отправитель 
typedef struct {
    UsbAction_t action;      // Что случилось: добавили или удалили?
    char device_path[256];   // Какой именно? Например: "/dev/ttyACM5"
} HotplugMsg_t;




typedef enum{
    QUEUE_PASS_STATE,
    QUEUE_WAIT_STATE
}Queue_state_t;


typedef enum{
    SHOW_NONE = -1,
    SHOW_AVE_MODE,
    SHOW_DUMP_MODE
}Print_Mode_t;




/// @brief Структура очереди  
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





/// @brief 
typedef struct 
{
    void* (*threads_cdc)(void*);
    pthread_t pthread;
    COM_Ports_Handle_t* COM_Ports_Handle;
    uint16_t TotalNumberOfDevice;
}Thread_CDC_Device_t;




Thread_CDC_Device_t Thread_CDC_Device;
pthread_t pthread_hotpug_connect;
pthread_t pthread_display;
pthread_t pthread_filesystem;
extern Queue_Handle_t Queue;





uint8_t Queue_Init(Queue_Handle_t* Queue, uint32_t len);
void Queue_Push(Queue_Handle_t* Queue, ModulData_t* data_ptr, Queue_state_t Mode);
int Queue_Pop(Queue_Handle_t *Queue, ModulData_t* data_ptr, uint32_t cnt_read_frame, Queue_state_t Mode);
void* thread_hotpug_connect(void* arg);
void* thread_cdc_generic(void* arg);
void* thread_display(void* arg);
void* thread_filesystem(void* arg);
