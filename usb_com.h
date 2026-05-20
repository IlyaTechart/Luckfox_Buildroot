#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>


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