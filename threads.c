#include <unistd.h>
#include "threads.h"
#include "usb_com.h"
#include "frames_structure.h"
#include "rw_file.h"
#include "epoll.h"
#include "sockets.h"




/// @brief Привязанные структуры к их потокам 
pthread_t pthread_kernel_events;
pthread_t pthread_heandler_karnel_event;
pthread_t pthread_cdc_generic;
pthread_t pthread_display;
pthread_t pthread_filesystem;

/// @brief Экземпляры очередей 
Queue_Handle_t Queue_dump;
Queue_Handle_t Queue_ave;

/// @brief Для режимов работы thread_display 
Print_Mode_t Print_Mode = SHOW_NONE;


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

/// @brief Поток читающий собтия от ядра Linux 
/// @param arg 
/// @return 
void* thread_kernel_events(void* arg)
{
    int fd;
    Socket_Netlink_Init(&fd);

    printf("[HOTPLUG] Поток слежения Netlink запущен. Ждем события USB...\n");

    // Буфер для приема данных от ядра
    char buffer[4096]; 

    while(1)
    {

        // 1. Читаем данные из сокета. 
        // Поток здесь ЗАСНЕТ и будет ждать, пока не произойдет событие.
        int len = recv(fd, buffer, sizeof(buffer), 0);
        
        if (len <= 0) continue; // Ошибка чтения или пустой пакет, читаем заново

        // Переменные, куда мы сохраним найденные значения
        char *action = NULL;
        char *devname = NULL;
        char *subsystem = NULL;

        int i = 0;
        // Парсинг сообщения от ядра 
        while(i < len)
        {
            char *current_string = &buffer[i];

            if (strncmp(current_string, "ACTION=", 7) == 0) {
                action = current_string + 7; // Сохраняем указатель на само значение (после "ACTION=")
            } 
            else if (strncmp(current_string, "DEVNAME=", 8) == 0) {
                devname = current_string + 8;
            } 
            else if (strncmp(current_string, "SUBSYSTEM=", 10) == 0) {
                subsystem = current_string + 10;
            }

            i += strlen(current_string) + 1;
        }

                // 3. Анализируем то, что нашли
        // Нам интересны только устройства, где SUBSYSTEM=tty и имя начинается на "ttyACM"
        if (subsystem && strcmp(subsystem, "tty") == 0 && 
            devname && strncmp(devname, "ttyACM", 6) == 0) 
        {
            HotplugMsg_t msg;
            
            // Ядро присылает DEVNAME="ttyACM0". Нам для функции open() нужен полный путь.
            // Поэтому клеим приставку "/dev/"
            snprintf(msg.device_path, sizeof(msg.device_path), "/dev/%s", devname);

            // Если кабель воткнули
            if (action && strcmp(action, "add") == 0) 
            {
                msg.action = USB_ACTION_ADD;
                printf("[HOTPLUG] Подключено устройство: %s. Отправляем команду в Конвейер.\n", msg.device_path);

                int FreeDev = USB_Finde_Free_Device(COM_Ports_Handle);
                if(strlen(msg.device_path) > sizeof(COM_Ports_Handle[FreeDev].path_ttyACM) ){
                    printf("strlen не смог найти терминальный ноль");
                    continue;
                }
                memcpy( COM_Ports_Handle[FreeDev].path_ttyACM,  msg.device_path, strlen(msg.device_path) );
                
                // Кидаем записку в нашу "трубу" для epoll-потока
                write(hotplug_pipe[1], &msg, sizeof(HotplugMsg_t));
            } 
            // Если кабель выдернули
            else if (action && strcmp(action, "remove") == 0) 
            {
                msg.action = USB_ACTION_REMOVE;
                printf("[HOTPLUG] Отключено устройство: %s. Отправляем команду в Конвейер.\n", msg.device_path);

                int Number = USB_Finde_Device_Of_Path(msg.device_path, COM_Ports_Handle);
                
                // Кидаем записку в нашу "трубу"
                write(hotplug_pipe[1], &msg, sizeof(HotplugMsg_t));
            }
        }
    }
    close(fd);
    return NULL;
}

/// @brief Поток для обработки событий от thread_kernel_events
/// @param arg 
/// @return 
// void* thread_heandler_karnel_event(void* arg)
// {


//     struct epoll_event events_pipe;

//     int nfds = Epoll_Wait(Epoll_Context_Pipe, &events_pipe, sizeof(events_pipe) / sizeof(struct epoll_event), -1);

//     if(nfds > 0) 
//     {
//         Epoll_User_Data_t* event_data = (Epoll_User_Data_t*)events_pipe.data.ptr;

//         if(event_data->type == EPOLL_SOURCE_HOTPLUG_PIPE)
//         {
//             HotplugMsg_t msg;
//             read(hotplug_pipe[0], &msg, sizeof(HotplugMsg_t));

//             if (msg.action == USB_ACTION_ADD) {
//                 printf("[READER] Получил команду добавить %s\n", msg.device_path);
                
//                 int FreeDev = USB_Finde_Free_Device(COM_Ports_Handle);
//                 if(strlen(msg.device_path) > sizeof(COM_Ports_Handle[FreeDev].path_ttyACM) ){
//                     printf("strlen не смог найти терминальный ноль");
//                     continue;
//                 }
//                 memcpy( COM_Ports_Handle[FreeDev].path_ttyACM,  msg.device_path, strlen(msg.device_path) );
//                 USB_Add_New_Device(&COM_Ports_Handle[FreeDev]);
//                 Epoll_Add(Epoll_Context_USB, COM_Ports_Handle[FreeDev].File_Descriptor , EPOLLIN, &COM_Ports_Handle[FreeDev].Epoll_User_Data);
                
//             } 
//             else if (msg.action == USB_ACTION_REMOVE) {
//                 printf("[READER] Получил команду удалить %s\n", msg.device_path);
//                 int Number = USB_Finde_Device_Of_Path(msg.device_path, COM_Ports_Handle);   
//                 Epoll_Remove(Epoll_Context_USB, COM_Ports_Handle[Number].File_Descriptor);
//                 USB_Remove_Device(&COM_Ports_Handle[Number], &Thread_CDC_Device);
//             }
            
//         }
//     }


// }



/// @brief Поток осуществяет приём статических данных с логера, а так же прём дампов.
/// @param arg Указатель на струтуру типа Thread_CDC_Device*
/// @return 0 - если поток завершается 
void* thread_cdc_generic(void* arg)
{

    Push_tread_arg_t* Push_tread_arg = (Push_tread_arg_t*)arg;

    Thread_CDC_Device_t* Thread_CDC_Device = (Thread_CDC_Device_t*)Push_tread_arg->Thread_CDC_Device_p;
    Epoll_Context_t* Epoll_Context_USB = (Epoll_Context_t*)Push_tread_arg->Epoll_Context_Pipe_p;

    uint32_t ret = 0;

    for(uint8_t i = 0; i < Thread_CDC_Device->NumberDev_of_Init; i++){
        ret = USB_Add_New_Device(&Thread_CDC_Device->COM_Ports_Handle[i]);
        if(ret != 0){
            perror("Ошибка: USB_Com_Init - не инициализировался успешно\n");//TODO
            exit(EXIT_FAILURE);
        }
    }

    for(uint8_t i = 0; i < Thread_CDC_Device->NumberDev_of_Init; i++)
    {
        COM_Ports_Handle_t* COM_Ports_Handle = &Thread_CDC_Device->COM_Ports_Handle[i];

        if(Epoll_Add(Epoll_Context_USB, COM_Ports_Handle->File_Descriptor, EPOLLIN, &COM_Ports_Handle->Epoll_User_Data) != 0)
        {
            continue;
        }
    }

    Queue_Init(&Queue_dump, TAKE_MEMORY_FOR_ELEMENTS);
    Queue_Init(&Queue_ave, SIZE_QUEUE_DISPLAY_ELEMENTS);

    struct epoll_event events_usb_array[SUPPORT_NUMBER_DEVICE_USB + NUMBERS_EVENTS_PIPE];

    printf("Вход в поток приёма данных\n");
    while(1)
    {

        int nfds = Epoll_Wait(Epoll_Context_USB, events_usb_array, SUPPORT_NUMBER_DEVICE_USB, -1);
        if(nfds < 0){
            continue;
        }

        for(uint16_t i = 0; i < nfds; i++ )
        {
            if(events_usb_array[i].events != EPOLLIN) continue;

            Epoll_User_Data_t* event_data = events_usb_array[i].data.ptr;

            if(event_data->type == EPOLL_SOURCE_HOTPLUG_PIPE)
            {
                HotplugMsg_t msg;
                read(hotplug_pipe[0], &msg, sizeof(HotplugMsg_t));

                if (msg.action == USB_ACTION_ADD) {
                    printf("[READER] Получил команду добавить %s\n", msg.device_path);
                    
                    int FreeDev = USB_Finde_Free_Device(COM_Ports_Handle);
                    if(strlen(msg.device_path) > sizeof(COM_Ports_Handle[FreeDev].path_ttyACM) ){
                        printf("strlen не смог найти терминальный ноль");
                        continue;
                    }
                    memcpy( COM_Ports_Handle[FreeDev].path_ttyACM,  msg.device_path, strlen(msg.device_path) );
                    USB_Add_New_Device(&COM_Ports_Handle[FreeDev]);
                    Epoll_Add(Epoll_Context_USB, COM_Ports_Handle[FreeDev].File_Descriptor , EPOLLIN, &COM_Ports_Handle[FreeDev].Epoll_User_Data);
                    
                } 
                else if (msg.action == USB_ACTION_REMOVE) {
                    printf("[READER] Получил команду удалить %s\n", msg.device_path);
                    int Number = USB_Finde_Device_Of_Path(msg.device_path, COM_Ports_Handle);   
                    Epoll_Remove(Epoll_Context_USB, COM_Ports_Handle[Number].File_Descriptor);
                    USB_Remove_Device(&COM_Ports_Handle[Number], Thread_CDC_Device);
                }

            }

            if(event_data->type == EPOLL_SOURCE_USB_CDC)
            {
                COM_Ports_Handle_t* port = (COM_Ports_Handle_t*)event_data->custom_data;
                Monitor_Msg_t Monitor_Msg = {0};

                // Читаем данные от Device 
                Receive_msg(port, events_usb_array[i].events, &Monitor_Msg);

                // Проверяем, не сломался ли порт в процессе чтения
                if (Monitor_Msg.States & (ERR_READ_DATA_PAYLOAD | ERR_READ_TAIL | ERR_COUNT_FRAME | NOT_EPOLLIN_FROM_EPOLL)) 
                {
                    printf("Устройство %s вызвало ошибку или было отключено. Удаляем.\n", port->path_ttyACM);
                    Epoll_Remove(Epoll_Context_USB, port->File_Descriptor);
                    USB_Remove_Device(port, Thread_CDC_Device);
                    continue; // Переходим к следующему событию
                }

                // Если все хорошо, раскидываем данные по очередям
                if (Monitor_Msg.KindeOfFrame == READ_HEAD_AVE) {
                    Print_Mode = SHOW_AVE_MODE;
                    Queue_Push(&Queue_ave, AVE_Data_Rx.buffer, QUEUE_WAIT_STATE);
                }
                else if (Monitor_Msg.KindeOfFrame == READ_HEAD_DUMP) {
                    printf("Устройство %s передало правильный DUMP tail\n", Monitor_Msg.NameDev);
                    Print_Mode = SHOW_DUMP_MODE;
                    for (uint32_t t = 0; t < DumpData_Rx.count_elements; t++) {
                        Queue_Push(&Queue_dump, &DumpData_Rx.buffer[t], QUEUE_WAIT_STATE);
                    }
                }
                
            }

        }

    }

    return 0;
}

/// @brief Поток вывода в консоль информации 
/// @param arg - NULL
/// @return - NON RETURN    
void* thread_display(void* arg)
{
    ModulData_t* ModulDatPrintDump = (ModulData_t*)calloc( TAKE_MEMORY_FOR_ELEMENTS, sizeof(ModulData_t) );

    

    uint32_t index_count = 0;
    printf("Вход в поток вывода информации\n");
    while(1)
    {
        
        
        ModulData_t ModulDataPrintAVE;
        switch (Print_Mode)
        {
        case SHOW_AVE_MODE:{

            int count_data = Queue_Pop(&Queue_ave, &ModulDataPrintAVE, 1, QUEUE_WAIT_STATE);
            logger_print_one_frame(&ModulDataPrintAVE, index_count, LOG_COLOR_DEFAULT);
            if (index_count < UINT32_MAX) {
                index_count++;
            } else {
                index_count = 0;
            }

            Print_Mode = SHOW_NONE;
            break;
        }
        
        case SHOW_DUMP_MODE:{

            int count_data = Queue_Pop(&Queue_dump, ModulDatPrintDump, NUMBER_ELLEMENTS_RECESIVE, QUEUE_WAIT_STATE);
            printf("Количество элементов в очереди: %u, head: %u tail: %u\n", Queue_dump.count, Queue_dump.head, Queue_dump.tail);
            for(uint32_t i = 0; i < count_data; i++)
            {
                if(ModulDatPrintDump[i].packet.alarms.raw != 0){
                    logger_print_one_frame(&ModulDataPrintAVE, i, LOG_COLOR_RED);
                }
            }
            Print_Mode = SHOW_NONE;
            break;

        }
            
        default:
            break;
        }

    }
}


void* thread_filesystem(void* arg)
{
    COM_Ports_Handle_t *COM_Ports_Device = Thread_CDC_Device.COM_Ports_Handle;

    while(1)
    {
        ModulData_t* DataFrameBuff = (ModulData_t*)calloc(NUMBER_ELLEMENTS_RECESIVE, sizeof(ModulData_t));
        int popped_elements = Queue_Pop(&Queue_dump, DataFrameBuff, NUMBER_ELLEMENTS_RECESIVE, QUEUE_WAIT_STATE);

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
        // //const char* remote_file  = "ilya73@192.168.1.107:/home/ilya73/ttyACM0.csv";
        // const char* remote_file  = "q@172.18.147.195:/home/q/ttyACM0.csv";
        // const char* local_path  = "/userdata/dumps_log/LogTtyACM0";
        // char scp_buffer[256];
        // snprintf(scp_buffer, sizeof(scp_buffer),"scp %s %s", local_path, remote_file);
        // printf("Выполняется: %s\n", scp_buffer);
        // int ret = system(scp_buffer);
        // if (ret == 0) {
        //     printf("Файл успешно скопирован.\n");
        // } else {
        //     printf("Ошибка scp, код возврата: %d\n", ret);
        // // system() возвращает статус оболочки; для детального анализа используйте WEXITSTATUS(ret)
        // }

        usleep(10000); 

    }

}





