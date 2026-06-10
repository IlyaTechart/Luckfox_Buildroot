#include <unistd.h>
#include "threads.h"
#include "usb_com.h"
#include "frames_structure.h"
#include "rw_file.h"
#include "epoll.h"


/// @brief Привязанные структуры к их потокам 
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

void* thread_hotpug_connect(void* arg)
{
    int fd;
    struct sockaddr_nl addr;

    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if (fd < 0)
    { 
        perror("Ошибка создания сокета Netlink"); 
        return NULL; 
    }

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 1;
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    { 
        perror("Ошибка bind Netlink"); 
        return NULL; 
    }
    
    Epoll_Add_Pipe(hotplug_pipe);

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
                
                // Кидаем записку в нашу "трубу" для epoll-потока
                write(hotplug_pipe[1], &msg, sizeof(HotplugMsg_t));
            } 
            // Если кабель выдернули
            else if (action && strcmp(action, "remove") == 0) 
            {
                msg.action = USB_ACTION_REMOVE;
                printf("[HOTPLUG] Отключено устройство: %s. Отправляем команду в Конвейер.\n", msg.device_path);
                
                // Кидаем записку в нашу "трубу"
                write(hotplug_pipe[1], &msg, sizeof(HotplugMsg_t));
            }
        }
    }
    close(fd);
    return NULL;
}



/// @brief Поток осуществяет приём статических данных с логера, а так же прём дампов.
/// @param arg Указатель на струтуру типа Thread_CDC_Device*
/// @return 0 - если поток завершается 
void* thread_cdc_generic(void* arg)
{
    Thread_CDC_Device_t* Thread_CDC_Device = (Thread_CDC_Device_t*)arg;
    uint32_t ret = 0;

    USB_Buffers_Init();

    for(uint8_t i = 0; i < Thread_CDC_Device->NumberDev_of_Init; i++){
        ret = USB_Add_New_Device(&Thread_CDC_Device->COM_Ports_Handle[i]);
        if(ret != 0){
            perror("Ошибка: USB_Com_Init - не инициализировался успешно\n");//TODO
            exit(EXIT_FAILURE);
        }
    }

    Epoll_Add_InitUSB(Thread_CDC_Device);

    Queue_Init(&Queue_dump, TAKE_MEMORY_FOR_ELEMENTS);
    Queue_Init(&Queue_ave, SIZE_QUEUE_DISPLAY_ELEMENTS);

    printf("Вход в поток приёма данных\n");
    while(1)
    {

        int nfds = Epoll_Wait();
        if(nfds < 0){
            continue;
        }

        int num_bytes = 0;
        for(uint16_t i = 0; i < nfds; i++ )
        {
            COM_Ports_Handle_t* COM_Ports_Active = (COM_Ports_Handle_t*)events[i].data.ptr;

            ReadDataState_t KindOfHead;
            if(events[i].events & EPOLLIN){
                uint32_t Head_Frame = 0;
                KindOfHead = Read_Head_Frame(COM_Ports_Active, &Head_Frame);

                switch (KindOfHead)
                {
                case READ_HEAD_DUMP:
                    if(Read_Count_Frame(COM_Ports_Active, &DumpData_Rx) > 0)
                    {
                        num_bytes = Read_Data_Payload(COM_Ports_Active, &DumpData_Rx);
                        if(num_bytes <= 0){
                            Epoll_Delete(COM_Ports_Active);
                            continue;
                        }
                        if(Read_Tail_Frame(COM_Ports_Active, &DumpData_Rx) != 0){
                            Epoll_Delete(COM_Ports_Active);
                            continue;
                        }
                    }
                    break;
                
                case READ_HEAD_AVE:
                    if(Read_Count_Frame(COM_Ports_Active, &AVE_Data_Rx) > 0)
                    {
                        num_bytes = Read_Data_Payload(COM_Ports_Active, &DumpData_Rx);
                        if(num_bytes <= 0){
                            Epoll_Delete(COM_Ports_Active);
                            continue;
                        }
                        if(Read_Tail_Frame(COM_Ports_Active, &AVE_Data_Rx) != 0){
                            Epoll_Delete(COM_Ports_Active);
                            continue;
                        }
                    }
                    break;
                
                default:
                    break;
                }

                if(AVE_Data_Rx.tail_frames == ID_TAIL_FRMES)
                {
                    Print_Mode = SHOW_AVE_MODE;
                    Queue_Push(&Queue_ave, AVE_Data_Rx.buffer, QUEUE_WAIT_STATE);
                    
                }


                if(DumpData_Rx.tail_frames == ID_TAIL_FRMES){
                    printf("Устройство %s передало правильный tail\n", COM_Ports_Active->path_ttyACM);
                    for(uint32_t i = 0; i < DumpData_Rx.count_elements; i++){
                        Print_Mode = SHOW_DUMP_MODE;
                        Queue_Push(&Queue_dump, &DumpData_Rx.buffer[i], QUEUE_WAIT_STATE);
                    }
                
                }
            }  
        }
 
        memset(&DumpData_Rx, 0x00, sizeof(DumpData_Rx));
        memset(&AVE_Data_Rx, 0x00, sizeof(AVE_Data_Rx));
    }

    return 0;
}

/// @brief Поток вывода в консоль информации 
/// @param arg - NULL
/// @return - NON RETURN    
// void* thread_display(void* arg)
// {
//     //ModulData_t* ModulDatPrintDump = (ModulData_t*)calloc( TAKE_HEAP_MEMORY_FOR_ELEMENTS, sizeof(ModulData_t) );

    

//     uint32_t index_count = 0;
//     printf("Вход в поток вывода информации\n");
//     while(1)
//     {
        
        
//         ModulData_t ModulDataPrintAVE;
//         switch (Print_Mode)
//         {
//         case SHOW_NONE:{
//             usleep(100000);
//             break;
//         }
//         case SHOW_AVE_MODE:{

//             int count_data = Queue_Pop(&Queue_ave, &ModulDataPrintAVE, 1, QUEUE_WAIT_STATE);
//             logger_print_one_frame(&ModulDataPrintAVE, index_count);
//             if (index_count < UINT32_MAX) {
//                 index_count++;
//             } else {
//                 index_count = 0;
//             }

//             Print_Mode = SHOW_NONE;
//             break;
//         }
        
//         case SHOW_DUMP_MODE:{

//             int count_data = Queue_Pop(&Queue_dump, ModulDatPrintDump, NUMBER_ELLEMENTS_RECESIVE, QUEUE_PASS_STATE);
//             printf("Количество элементов в очереди: %u, head: %u tail: %u\n", Queue_dump.count, Queue_dump.head, Queue_dump.tail);
//             for(uint32_t i = 0; i < count_data; i++)
//             {
//                 if(ModulDatPrintDump[i].packet.alarms.raw != 0){
//                     logger_print_one_frame(&ModulDataPrintAVE, i);
//                 }
//             }
//             Print_Mode = SHOW_NONE;
//             break;

//         }
            
//         default:
//         sleep(2);
//             break;
//         }

//     }
// }


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





