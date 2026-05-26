#include <unistd.h>
#include "threads.h"
#include "usb_com.h"
#include "frames_structure.h"

/// @brief 
/////////
Thread_CDC_Device_t Thread_CDC_Device = {0};
pthread_t pthread_display;
Queue_Handle_t Queue_Dump;


/// @brief Инициализацияя очереди, передающей между потоками дамп 
/// @param Queue Указатель на очередь
/// @param len Коллиечство кадров в очереди 
/// @return Если инциализация успешна - 0, в противном случае - 1 
uint8_t Queue_Init(Queue_Handle_t* Queue, uint32_t len)
{
    Queue->head = 0;
    Queue->tail = 0;
    Queue->data = (ModulData_t*)calloc(len, sizeof(ModulData_t));
    Queue->len = len;
    Queue->count = 0;
    snprintf( Queue->name, sizeof(Queue->name), "Dump queue");
    
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

/// @brief Функция записи ондого кадра в буфер. Если очередь переполнена, старые данные стираются.
/// @param Queue Указатель на очередь 
/// @param data_ptr Указатель на адрес типа ModulData_t одного кадра 
/// @param Mode Режим работы функции (с ожиданием принимающей стороны - QUEUE_WAIT_STATE или без - QUEUE_PASS_STATE)
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

    uint32_t next_head = (Queue->head + 1) % Queue->len;

    if(next_head == Queue->tail)
    {
        memcpy(&Queue->data[Queue->head], data_ptr, sizeof(ModulData_t));
        Queue->tail = (Queue->tail + 1) % Queue->len;
        Queue->head = next_head;
        if(Queue->count < Queue->len){
            Queue->count++;
        }
    }else{
        memcpy(&Queue->data[Queue->head], data_ptr, sizeof(ModulData_t));
        Queue->head = next_head;
        Queue->count++;
    }

    if(pthread_cond_signal(&Queue->cond_not_empty) != 0)
    {
        perror("Push: Неуспешное выполнение функции pthread_cond_signal\n");
    } 

    pthread_mutex_unlock(&Queue->mutex);
}

/// @brief Чтение из буфера запрашивеймого колличества кадров 
/// @param Queue Указатель на очередь 
/// @param data_ptr Указатель на массив куда будут записаны данные 
/// @param cnt_read_frame Количесвто запрашиваемых кадров 
/// @param Mode Режим работы функции (с ожиданием принимающей стороны - QUEUE_WAIT_STATE или без - QUEUE_PASS_STATE)
/// @return Колличество прочитанных кадров N
int Queue_Pop(Queue_Handle_t *Queue, ModulData_t* data_ptr, uint32_t cnt_read_frame, Queue_state_t Mode)
{
    uint32_t count_copyed_frame = 0;
    pthread_mutex_lock(&Queue->mutex);

    if(Mode == QUEUE_WAIT_STATE){
        while(Queue->count == 0){
            if(pthread_cond_wait(&Queue->cond_not_empty, &Queue->mutex) != 0)
            {
                perror("Pop: Неуспешное выполнение функции pthread_cond_wait\n");
            }
        }
    }
    if(cnt_read_frame > Queue->count){
        printf("Отсутвует запрашиваемый объём данных в буфере\n");
        pthread_cond_signal(&Queue->cond_not_full);
        pthread_mutex_unlock(&Queue->mutex);
        return -1;
    }

    while (count_copyed_frame < cnt_read_frame && Queue->count > 0){

        memcpy(data_ptr + count_copyed_frame, &Queue->data[Queue->tail], sizeof(ModulData_t));
        memset(&Queue->data[Queue->tail], 0x00, sizeof(ModulData_t));
        Queue->tail = (Queue->tail + 1) % Queue->len;
        Queue->count--;
        count_copyed_frame++;
        
    }

    //memcpy(data_ptr, Queue->data[Queue->tail], cnt_read_frame);

    if(pthread_cond_signal(&Queue->cond_not_full) != 0)
    {
        perror("Pop: Неуспешное выполнение функции pthread_cond_signal\n");
    } 

    pthread_mutex_unlock(&Queue->mutex);
    return count_copyed_frame;
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

    Queue_Init(&Queue_Dump, NUMBER_ELLEMENTS_RECESIVE);

    DumpData_t DumpData_Rx = {0};
    DumpData_Rx.buffer = (ModulData_t*)calloc(NUMBER_ELLEMENTS_RECESIVE, sizeof(ModulData_t));

    printf("Вход в поток приёма данных\n");
    while(1)
    {
        int nfds = epoll_wait(epoll_fd, events, SUPPORT_NUMBER_DEVICE_USB, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                // Вызов прерван сигналом (например, отладчиком GDB).
                // Это нормально, просто игнорируем и ждем данные дальше.
                continue;
            }
            perror("Ошибка: epoll_wait\n");
            free(DumpData_Rx.buffer);
            exit(EXIT_FAILURE);
        }

        for(uint16_t i = 0; i < nfds; i++ )
        {
            COM_Ports_Handle_t* COM_Ports_Active = (COM_Ports_Handle_t*)events[i].data.ptr;

            if(events[i].events & EPOLLIN){

                int num_bytes = read(COM_Ports_Active->File_Descriptor, &DumpData_Rx.head_frames, sizeof(DumpData_Rx.head_frames));
                if(num_bytes == -1){
                    error_label:
                    perror("Ошибка функции read(): не прочитала файл\n"); 
                    continue;
                }else if(num_bytes == 0){
                    printf("Устройство %s отключено после чтения head\n", COM_Ports_Active->path_ttyACM);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, COM_Ports_Active->File_Descriptor, NULL);
                    close(COM_Ports_Active->File_Descriptor);
                    continue;
                }
                num_bytes = read(COM_Ports_Active->File_Descriptor, &DumpData_Rx.count_elements, sizeof(DumpData_Rx.count_elements));
                if(num_bytes == - 1) goto error_label;


                if(DumpData_Rx.head_frames != ID_DUMP_FRAME_START){
                    printf("Ошибка прёма данных: неверный ID head frame\n");
                }else if(DumpData_Rx.head_frames == ID_DUMP_FRAME_START){

                    if(DumpData_Rx.count_elements < 1){
                        printf("Ошибка прёма данных: слишком маленькое колличество ожидаемых данных\n");
                    }else if(DumpData_Rx.count_elements >= 1){
                        printf("Успешный приём head сообщания и начало ожидание приёма данных\n");
                        printf("Ожидаемое колличество принемаемых frame-ов %u - байт %u\n", DumpData_Rx.count_elements, DumpData_Rx.count_elements * sizeof(ModulData_t));
                        if(USB_Read_COM(COM_Ports_Active, DumpData_Rx.buffer, DumpData_Rx.count_elements * sizeof(ModulData_t), 10000) != USB_OK){
                            printf("Устройство %s отключено после тчения data\n", COM_Ports_Active->path_ttyACM);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, COM_Ports_Active->File_Descriptor, NULL);
                            close(COM_Ports_Active->File_Descriptor);
                            continue;
                        }

                        num_bytes = read(COM_Ports_Active->File_Descriptor, &DumpData_Rx.tail_frames, sizeof(DumpData_Rx.tail_frames));
                        if(num_bytes == -1) goto error_label;
                        
                        if(DumpData_Rx.tail_frames == ID_TAIL_FRMES){
                            for(uint32_t i = 0; i < DumpData_Rx.count_elements; i++){
                                Queue_Push(&Queue_Dump, &DumpData_Rx.buffer[i], QUEUE_WAIT_STATE);
                            }
                        }
                    }

                }
            }
        }
    }
    free(DumpData_Rx.buffer);
    return 0;
}


void* thread_display(void* arg)
{
    ModulData_t* ModulData_print = (ModulData_t*)calloc( NUMBER_ELLEMENTS_RECESIVE, sizeof(ModulData_t) );

    printf("Вход в поток вывода информации\n");
    while(1)
    {

        int count_data = Queue_Pop(&Queue_Dump, ModulData_print, Queue_Dump.count, QUEUE_WAIT_STATE);
        if(count_data < 0){
            printf("Ошибка чтения из кольцевого буфера\n");
        }else{
            for(uint32_t i = 0; i < count_data; i++ )
            {
                if(ModulData_print[i].packet.alarms.raw != 0){
                    logger_print_one_frame(&ModulData_print[i], i);
                }
            }

        }
    }
}




