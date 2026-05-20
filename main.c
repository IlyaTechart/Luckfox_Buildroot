#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <mqueue.h>
#include <time.h>
#include "threads.h"
#include "usb_com.h"



void Get_Discription_Connected_Devices(void)
{
    void;
}


int main(int argc, char * argv[])
{
    Get_Discription_Connected_Devices();

    Thread_CDC_Device.COM_Ports_Handle = COM_Ports_Handle;
    snprintf((char*)Thread_CDC_Device.COM_Ports_Handle[0].path_ttyACM, sizeof(Thread_CDC_Device.COM_Ports_Handle[0].path_ttyACM),"/dev/ttyACM0");
    Thread_CDC_Device.threads_cdc = thread_cdc_generic;
    Thread_CDC_Device.TotalNumberOfDevice = 1;

    int result;
    result = pthread_create(&Thread_CDC_Device.pthread, NULL, Thread_CDC_Device.threads_cdc , &Thread_CDC_Device);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: generic\n");
    return EXIT_FAILURE;
    }

    result = pthread_create(&pthread_display, NULL, thread_display , NULL);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: display\n");
    return EXIT_FAILURE;
    }


    while(1)
    {

        usleep(1000000);

    }
    printf("Программа завершилась\n");
    return 0;
}