#include "epoll.h"


static int epoll_fd = 0;
struct epoll_event events[SUPPORT_NUMBER_DEVICE_USB]; // TODO (переделать на задаваемый пользователем параметр)

void Epoll_Add_InitUSB(Thread_CDC_Device_t* Thread_CDC_Device)
{

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Ошибка: epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;

    for(uint16_t i = 0; i < Thread_CDC_Device->TotalNumberOfDevice; i++ )
    {
        if(Thread_CDC_Device->COM_Ports_Handle[i].File_Descriptor < 0){

            continue;
        }
        event.events = EPOLLIN;
        event.data.ptr = &Thread_CDC_Device->COM_Ports_Handle[i];
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, Thread_CDC_Device->COM_Ports_Handle[i].File_Descriptor, &event) == -1) {
            fprintf(stderr,"Ошибка: epoll_ctl не добалвен для устройсва с номером %d", Thread_CDC_Device->COM_Ports_Handle[i].DeviceNumber);
        }
    }
}

int Epoll_Wait(void)
{
        int nfds = epoll_wait(epoll_fd, events, SUPPORT_NUMBER_DEVICE_USB, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                // Вызов прерван сигналом (например, отладчиком GDB).
                // Это нормально, просто игнорируем и ждем данные дальше.
                return -2;
            }
            perror("Ошибка: epoll_wait\n");
            return -1;
        }

    return nfds;
}

void Epoll_Add_Device(COM_Ports_Handle_t* COM_Ports_Active)
{
    struct epoll_event event;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, COM_Ports_Handle->File_Descriptor, &event) == -1) {
        fprintf(stderr,"Ошибка: epoll_ctl не добалвен для устройсва с номером %d", COM_Ports_Handle->File_Descriptor);
    }
    printf("Устройство %s было добавлено в epoll\n", COM_Ports_Active->path_ttyACM);
}

void Epoll_Delete(COM_Ports_Handle_t* COM_Ports_Active)
{
    printf("Устройство %s не прочитало head и было удаленоиз epoll\n", COM_Ports_Active->path_ttyACM);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, COM_Ports_Active->File_Descriptor, NULL);
    close(COM_Ports_Active->File_Descriptor);
}

void Epoll_Add_Pipe(int *fd)
{
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = *fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, *fd, &event);
}