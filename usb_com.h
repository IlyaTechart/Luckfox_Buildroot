#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/epoll.h>
#include "frames_structure.h"

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
    char path_ttyACM[100];
    int File_Descriptor;
    int DeviceNumber;
    char* name_device[20];
    char* path_dump_file[50];
    FILE* file_dump;
}COM_Ports_Handle_t;


COM_Ports_Handle_t COM_Ports_Handle[24];



uint32_t USB_Add_New_Device(COM_Ports_Handle_t* COM_Port);
int USB_Read_COM(COM_Ports_Handle_t* COMPort, void* buffer, uint32_t size, uint32_t Timeout);
ReadDataState_t Read_Head_Frame(COM_Ports_Handle_t* COMPort, uint32_t *read_head);
int Read_Count_Frame(COM_Ports_Handle_t* COMPort, Package_t *DumpData_Rx);
int Read_Data_Dump(COM_Ports_Handle_t* COMPort, Package_t *DumpData_Rx);
int Read_AVE_Frame(COM_Ports_Handle_t* COMPort, Package_t *DumpData_Rx);
int Read_Tail_Frame(COM_Ports_Handle_t* COMPort, Package_t *DumpData_Rx);
