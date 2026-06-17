#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include "epoll.h"
#include "frames_structure.h"


#define SUPPORT_NUMBER_DEVICE_USB 24
#define NUMBER_ELLEMENTS_RECESIVE 1000
#define TAKE_MEMORY_FOR_ELEMENTS  1000

#if NUMBER_ELLEMENTS_RECESIVE > TAKE_MEMORY_FOR_ELEMENTS
#error "The number TAKE_MEMORY_FOR_ELEMENTS must be greater than the NUMBER_ELLEMENTS_RECESIVE."
#endif

// ID head-ров сообщений от Device по их типу 
#define ID_AVE_FRAME_START   (const uint32_t)0x22446688
#define ID_DUMP_FRAME_START  (const uint32_t)0x336699FF

#define ID_TAIL_FRMES        (const uint32_t)0x55AA55AA

typedef enum{
    USB_TIMEOUT = -1,
    USB_ERR = -2
}USB_State_t;

typedef enum{
    READ_NONE,
    READ_HEAD_DUMP,
    READ_HEAD_AVE,
    READ_ERROR
}ReadDataState_t;

typedef struct {
    struct termios tty;
    struct termios old_tty;

    char path_ttyACM[100]; // Для работы с файлами 
    int File_Descriptor;

    bool active;            // Состояния устройства 
    uint16_t Device_ID;
    Epoll_User_Data_t Epoll_User_Data;

    char* path_dump_file[50]; // Для работы с дампом и записью в файловую систему
    FILE* file_dump;
}COM_Ports_Handle_t;

/// @brief 
typedef struct {
    COM_Ports_Handle_t* COM_Ports_Handle;
    uint16_t CurrentNum_Device; // Текущее количесвто устройств 
    uint16_t NumberDev_of_Init; // Число устрйоств при инициализации 
}Thread_CDC_Device_t;

typedef enum{
    FRAME_PACKAGE_OK =              0,
    ERR_COUNT_FRAME =        (1 << 0),
    ERR_READ_DATA_PAYLOAD =  (1 << 1), 
    ERR_READ_TAIL =          (1 << 2),
    NOT_EPOLLIN_FROM_EPOLL = (1 << 3),
}Staite_Msg_Frame;

typedef struct {
    Staite_Msg_Frame States;
    ReadDataState_t KindeOfFrame;
    uint8_t ID_Dev_Who_From;
    char NameDev[20];
    bool activeate;
}Monitor_Msg_t;


/// @brief Сущности пакетных форматов 
Package_t DumpData_Rx;
Package_t AVE_Data_Rx;



Thread_CDC_Device_t Thread_CDC_Device;
COM_Ports_Handle_t COM_Ports_Handle[SUPPORT_NUMBER_DEVICE_USB];


void USB_Buffers_Init(void);
uint32_t USB_Add_New_Device(COM_Ports_Handle_t* COM_Port);
int USB_Read_COM(COM_Ports_Handle_t* COMPort, void* buffer, uint32_t size, uint32_t Timeout);
int USB_Finde_Free_Device(COM_Ports_Handle_t* COMPort);
int USB_Finde_Device_Of_Path(char *path, COM_Ports_Handle_t* COMPort);
int USB_Remove_Device(COM_Ports_Handle_t* COM_Port, Thread_CDC_Device_t* Thread_Devices);
ReadDataState_t Read_Head_Frame(COM_Ports_Handle_t* COMPort, uint32_t *read_head);
int Read_Count_Frame(COM_Ports_Handle_t* COMPort, Package_t *DumpData_Rx);
int Read_Data_Payload(COM_Ports_Handle_t* COMPort, Package_t *Data_Rx);
int Read_Tail_Frame(COM_Ports_Handle_t* COMPort, Package_t *DumpData_Rx);
void Receive_msg(int nfds, Monitor_Msg_t *Monitor_Debug);