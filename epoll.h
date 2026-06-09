#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/epoll.h>
#include "threads.h"




struct epoll_event events[SUPPORT_NUMBER_DEVICE_USB];




void Epoll_Add_InitUSB(Thread_CDC_Device_t* Thread_CDC_Device);
int Epoll_Wait(void);
void Epoll_Add_Device(COM_Ports_Handle_t* COM_Ports_Active);
void Epoll_Delete(COM_Ports_Handle_t* COM_Ports_Active);
void Epoll_Add_Pipe(int *fd);