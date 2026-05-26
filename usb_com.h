#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

// ID head-ров сообщений от Device по их типу 
#define ID_AVE_FRAME_START   (const uint32_t)0x22446688
#define ID_DUMP_FRAME_START  (const uint32_t)0x336699FF

#define ID_TAIL_FRMES        (const uint32_t)0x55AA55AA

typedef enum{
    USB_OK,
    USB_TIMEOUT,
    USB_ERR
}USB_State_t;

typedef struct {
    struct termios tty;
    char path_ttyACM[100];
    int File_Descriptor;
    int DeviceNumber;
}COM_Ports_Handle_t;


COM_Ports_Handle_t COM_Ports_Handle[24];



uint32_t USB_Com_Init(COM_Ports_Handle_t* COM_Port);
USB_State_t USB_Read_COM(COM_Ports_Handle_t* COMPort, void* buffer, uint32_t size, uint32_t Timeout);