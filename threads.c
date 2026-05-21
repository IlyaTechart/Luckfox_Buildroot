#include <unistd.h>
#include "threads.h"
#include "usb_com.h"
#include "frames_structure.h"


Thread_CDC_Device_t Thread_CDC_Device = {0};
pthread_t pthread_display;

uint8_t Queue_Init(Queue_Handle_t* Queue, uint32_t len)
{
    Queue->head = 0;
    Queue->tail = 0;
    Queue->data = (ModulData_t**)calloc(len, sizeof(ModulData_t*));
    Queue->len = len;
    Queue->count = 0;

    
    if(pthread_mutex_init(&Queue->mutex, NULL) != 0){
        perror("Ошибка инициализации мьютекса\n");
        return 1;
    }
    if(pthread_cond_init(&Queue->cond_not_empty, NULL) != 0){
        perror("Ошибка инициализации динамической переменной\n");
        return 1;
    }
    if(pthread_cond_init(&Queue->cond_not_full, NULL) != 0){
        perror("Ошибка инициализации динамической переменной\n");
        return 1;
    }

    return 0;
}
void Queue_Push(Queue_Handle_t* Queue, ModulData_t* data_ptr, Queue_state_t Mode)
{
    pthread_mutex_lock(&Queue->mutex);
    
    if(Mode == QUEUE_WAIT_STATE){
        while(Queue->count == Queue->len){
            if(pthread_cond_wait(&Queue->cond_not_full, &Queue->mutex) != 0)
            {
                perror("Push: Неуспешное выполнение функции pthread_cond_wait\n");
            }
        }
    }

    if (Queue->count < Queue->len) {
        Queue->data[Queue->head] = data_ptr;
        Queue->head = (Queue->head + 1) % Queue->len;
        Queue->count++;
    } else {
        printf("Очередь переполнена, кадр отброшен!\n");
    }

    if(pthread_cond_signal(&Queue->cond_not_empty) != 0)
    {
        perror("Push: Неуспешное выполнение функции pthread_cond_signal\n");
    } 

    pthread_mutex_unlock(&Queue->mutex);
}

ModulData_t* Queue_Pop(Queue_Handle_t *Queue, Queue_state_t Mode)
{
    ModulData_t* result = NULL;
    pthread_mutex_lock(&Queue->mutex);
    
    if(Mode == QUEUE_WAIT_STATE){
        while(Queue->count == 0){
            if(pthread_cond_wait(&Queue->cond_not_empty, &Queue->mutex) != 0)
            {
                perror("Pop: Неуспешное выполнение функции pthread_cond_wait\n");
            }
        }
    }

    if (Queue->count > 0) {
        result = Queue->data[Queue->tail];
        Queue->tail = (Queue->tail + 1) % Queue->len;
        Queue->count--;
    }

    if(pthread_cond_signal(&Queue->cond_not_full) != 0)
    {
        perror("Pop: Неуспешное выполнение функции pthread_cond_signal\n");
    } 

    pthread_mutex_unlock(&Queue->mutex);
    return result;
}



/*
@bref: Поток осуществяет приём статических данных с логера, а так же прём дампов.
*/
void* thread_cdc_generic(void* arg)
{
    Thread_CDC_Device_t* Thread_CDC_Device = (Thread_CDC_Device_t*)arg;
    uint32_t ret = 0;

    ret = USB_Com_Init(&Thread_CDC_Device->COM_Ports_Handle[0]);
    if(ret != 0){
        perror("Ошибка: USB_Com_Init - не инициализировался успешно");
        exit(EXIT_FAILURE);
    }

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

    struct epoll_event events[SUPPORT_NUMBER_DEVICE_USB]; // TODO (переделать на задаваемый пользователем параметр)

    DumpData_t DumpData_Rx = {0};
    DumpData_Rx.buffer = (ModulData_t*)calloc(NUMBER_ELLEMENTS_RECESIVE, sizeof(ModulData_t));

    
    while(1)
    {
        int nfds = epoll_wait(epoll_fd, events, SUPPORT_NUMBER_DEVICE_USB, -1);
        if (nfds == -1) {
            perror("Ошибка: epoll_wait\n");
            free(DumpData_Rx.buffer);
            exit(EXIT_FAILURE);
        }

        for(uint16_t i = 0; i < nfds; i++ )
        {
            COM_Ports_Handle_t* COM_Ports_Active = (COM_Ports_Handle_t*)events[i].data.ptr;

            if(events[i].events & EPOLLIN){

                int num_bytes = read(COM_Ports_Active->File_Descriptor, &DumpData_Rx, sizeof(DumpData_Rx.head_frames) + sizeof(DumpData_Rx.count_elements));
                if(num_bytes == -1){
                    perror("Ошибка функции read(): не прочитала файл\n"); 
                    continue;
                }else if(num_bytes == 0){
                    printf("Устройство %s отключено.\n", COM_Ports_Active->path_ttyACM);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, COM_Ports_Active->File_Descriptor, NULL);
                    close(COM_Ports_Active->File_Descriptor);
                    continue;
                }

                if(DumpData_Rx.head_frames != ID_DUMP_FRAME_START){
                    printf("Ошибка прёма данных: неверный ID head frame\n");
                }else if(DumpData_Rx.head_frames == ID_DUMP_FRAME_START){

                    if(DumpData_Rx.count_elements < 1){
                        printf("Ошибка прёма данных: слишком маленькое колличество ожидаемых данных\n");
                    }else if(DumpData_Rx.count_elements >= 2){
                        printf("Успешный приём head сообщания и начало ожидание приёма данных\n");
                        if(USB_Read_COM(COM_Ports_Active, DumpData_Rx.buffer, DumpData_Rx.count_elements * sizeof(ModulData_t), 250) == USB_ERR){
                            printf("Устройство %s отключено.\n", COM_Ports_Active->path_ttyACM);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, COM_Ports_Active->File_Descriptor, NULL);
                            close(COM_Ports_Active->File_Descriptor);
                            continue;
                        }
                        
                        // Если данные успешно прочитаны, кладем их в очередь
                        Queue_Push(&Queue_Dump, DumpData_Rx.buffer, QUEUE_WAIT_STATE);
                    }

                }
            }
        }
    }

    return 0;
}


void* thread_display(void* arg)
{
    while(1)
    {
        ModulData_t* data = Queue_Pop(&Queue_Dump, QUEUE_WAIT_STATE);
        if (data != NULL) {
            // ... Обработка данных ...
            // ВАЖНО: Освободить память, когда данные больше не нужны
            free(data);
        }
    }
}




