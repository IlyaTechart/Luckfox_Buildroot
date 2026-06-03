#include <unistd.h>
#include "threads.h"
#include "usb_com.h"
#include "frames_structure.h"
#include "rw_file.h"

/// @brief 
/////////
Thread_CDC_Device_t Thread_CDC_Device = {0};
pthread_t pthread_display;
pthread_t pthread_filesystem;
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

    if(Queue->count == Queue->len)
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

/// @brief Чтение из буфера запрашивеймого колличества кадров. Если 
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
        while(Queue->count < cnt_read_frame || Queue->count == 0){
            if(pthread_cond_wait(&Queue->cond_not_empty, &Queue->mutex) != 0)
            {
                perror("Pop: Неуспешное выполнение функции pthread_cond_wait\n");
                pthread_cond_signal(&Queue->cond_not_full);
                pthread_mutex_unlock(&Queue->mutex);
                return -1;
            }
        }
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



/// @brief Поток осуществяет приём статических данных с логера, а так же прём дампов.
/// @param arg Указатель на струтуру типа Thread_CDC_Device*
/// @return 0 - если поток завершается 
void* thread_cdc_generic(void* arg)
{
    Thread_CDC_Device_t* Thread_CDC_Device = (Thread_CDC_Device_t*)arg;
    uint32_t ret = 0;

    for(uint8_t i = 0; i < Thread_CDC_Device->TotalNumberOfDevice; i++){
        ret = USB_Com_Init(&Thread_CDC_Device->COM_Ports_Handle[i]);
        if(ret != 0){
            perror("Ошибка: USB_Com_Init - не инициализировался успешно");//TODO
            exit(EXIT_FAILURE);
        }
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

    Package_t DumpData_Rx = {0};
    Package_t AVE_Data_Rx = {0};
    DumpData_Rx.buffer = (ModulData_t*)calloc(NUMBER_ELLEMENTS_RECESIVE, sizeof(ModulData_t));
    AVE_Data_Rx.buffer = (ModulData_t*)calloc( 1 , sizeof(ModulData_t));

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

        int num_bytes = 0;
        for(uint16_t i = 0; i < nfds; i++ )
        {
            COM_Ports_Handle_t* COM_Ports_Active = (COM_Ports_Handle_t*)events[i].data.ptr;

            ReadDataState_t KindOfHead;
            if(events[i].events & EPOLLIN){
                KindOfHead = Read_Head_Frame(COM_Ports_Active, &DumpData_Rx);

                if( KindOfHead == READ_HEAD_DUMP ){
                    if(Read_Count_Frame(COM_Ports_Active, &DumpData_Rx) > 0)
                    {
                        num_bytes = Read_Data_Dump(COM_Ports_Active, &DumpData_Rx);
                        if(num_bytes <= 0){
                            goto err_mrk;
                        }
                        if(Read_Tail_Frame(COM_Ports_Active, &DumpData_Rx) != 0){
                            goto err_mrk;
                        }
                    }
                }else if( KindOfHead == READ_HEAD_AVE ){
                    if(Read_Count_Frame(COM_Ports_Active, &DumpData_Rx) > 0)
                    {
                        num_bytes = Read_AVE_Frame(COM_Ports_Active, &DumpData_Rx);
                        if(num_bytes <= 0){
                            goto err_mrk;
                        }
                        if(Read_Tail_Frame(COM_Ports_Active, &DumpData_Rx) != 0){
                            goto err_mrk;
                        }
                    }
                }else{
                    err_mrk:
                    printf("Устройство %s не прочитало head и было удаленоиз epoll\n", COM_Ports_Active->path_ttyACM);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, COM_Ports_Active->File_Descriptor, NULL);
                    close(COM_Ports_Active->File_Descriptor);
                    continue;
                }

                if(DumpData_Rx.tail_frames == ID_TAIL_FRMES){
                    printf("Устройство %s передало правильный tail\n", COM_Ports_Active->path_ttyACM);
                    for(uint32_t i = 0; i < DumpData_Rx.count_elements; i++){
                        Queue_Push(&Queue_Dump, &DumpData_Rx.buffer[i], QUEUE_WAIT_STATE);
                    }
                }
            }  
        }
        
    }
    free(DumpData_Rx.buffer);
    return 0;
}

/// @brief Поток вывода в консоль информации 
/// @param arg - NULL
/// @return - NON RETURN    
void* thread_display(void* arg)
{
    ModulData_t* ModulData_print = (ModulData_t*)calloc( NUMBER_ELLEMENTS_RECESIVE, sizeof(ModulData_t) );

    printf("Вход в поток вывода информации\n");
    while(1)
    {

        // int count_data = Queue_Pop(&Queue_Dump, ModulData_print, 10, QUEUE_WAIT_STATE);
        // printf("Количество элементов в очереди: %u, head: %u tail: %u\n", Queue_Dump.count, Queue_Dump.head, Queue_Dump.tail);
        // if(count_data < 0){
        //     printf("Ошибка чтения из кольцевого буфера\n");
        // }else{
        //     for(uint32_t i = 0; i < count_data; i++ )
        //     {
        //         if(ModulData_print[i].packet.alarms.raw != 0){
        //             logger_print_one_frame(&ModulData_print[i], i);
        //         }
        //     }
        // }

    }
}


void* thread_filesystem(void* arg)
{
    COM_Ports_Handle_t *COM_Ports_Device = Thread_CDC_Device.COM_Ports_Handle;

    while(1)
    {
        ModulData_t* DataFrameBuff = (ModulData_t*)calloc(NUMBER_ELLEMENTS_RECESIVE, sizeof(ModulData_t));
        int popped_elements = Queue_Pop(&Queue_Dump, DataFrameBuff, NUMBER_ELLEMENTS_RECESIVE, QUEUE_WAIT_STATE);

        if(popped_elements > 0)
        {
            size_t csv_buffer_size = (popped_elements * 300) + 500;
            char* FileFormatBuff = (char*)calloc(csv_buffer_size, sizeof(char));
            int written_bytes = FormatFrameInString(FileFormatBuff, csv_buffer_size, DataFrameBuff, popped_elements, "/ttyACM0");

            if(written_bytes > 0){
                if (File_Wirite(FileFormatBuff, (uint32_t)written_bytes, "/userdata/dumps_log/LogTtyACM0") != FILE_WRITE) {
                    printf("Ошибка записи файла!\n");
                } else {
                    printf("Успешно записано %d байт в CSV файл.\n", written_bytes);
                }
            }
            free(FileFormatBuff);
        }
        free(DataFrameBuff);
        //const char* remote_file  = "ilya73@192.168.1.107:/home/ilya73/ttyACM0.csv";
        const char* remote_file  = "q@172.18.147.195:/home/q/ttyACM0.csv";
        const char* local_path  = "/userdata/dumps_log/LogTtyACM0";
        char scp_buffer[256];
        snprintf(scp_buffer, sizeof(scp_buffer),"scp %s %s", local_path, remote_file);
        printf("Выполняется: %s\n", scp_buffer);
        int ret = system(scp_buffer);
        if (ret == 0) {
            printf("Файл успешно скопирован.\n");
        } else {
            printf("Ошибка scp, код возврата: %d\n", ret);
        // system() возвращает статус оболочки; для детального анализа используйте WEXITSTATUS(ret)
        }

        usleep(10000); 

    }

}





