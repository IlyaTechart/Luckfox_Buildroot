
#define _POSIX_C_SOURCE 199309L
#include "usb_com.h"
#include <time.h>
#include "epoll.h"



COM_Ports_Handle_t COM_Ports_Handle[24] = {0};
Thread_CDC_Device_t Thread_CDC_Device = {0};


/// @brief Сущности пакетных форматов 
Package_t DumpData_Rx = {0};
Package_t AVE_Data_Rx = {0};

/// @brief Буферы для покетных форматов 
ModulData_t Buffer_Dump[TAKE_MEMORY_FOR_ELEMENTS * sizeof(ModulData_t)] = {0};
ModulData_t Buffer_AVE = {0};

/// @brief Инциализируем глобальные буферы 
/// @param  
void USB_Buffers_Init(void)
{
    memset(&DumpData_Rx, 0x00, sizeof(DumpData_Rx));
    DumpData_Rx.buffer = Buffer_Dump;

    memset(&AVE_Data_Rx, 0x00, sizeof(AVE_Data_Rx));
    AVE_Data_Rx.buffer = &Buffer_AVE;

}

/// @brief Добавляет новое USB CDC устройство в массив списка COM_Ports_Handle, инициализирует COM порт, утсанавливает скрость работы. 
/// @param COM_Port Указатель на экземпляр списка который будет применяться для работы с устройством. В экземпляре должен быть путь к устройству ttyACM*.
/// @return Возвращает 0 при успешной инциализации. 
uint32_t USB_Add_New_Device(COM_Ports_Handle_t* COM_Port)
{

    static uint16_t ID_dev = 0;

    int LenPath = strlen(COM_Port->path_ttyACM);
    char path_ttyACM[100];                                              // Обработка длины пути к файлу COM устройства 
    if(LenPath > 99){
        printf("Ошибка: длина пути слишком большая: %s\n", COM_Port->path_ttyACM);
        return 1;
    } else if(LenPath < 4){
        printf("Ошибка: некорректная длина пути: %s\n", COM_Port->path_ttyACM);
        return 1;
    }
    memcpy(path_ttyACM, COM_Port->path_ttyACM, LenPath);
                                                                        // Взятие дискриптора на ..ttyACM*
    COM_Port->File_Descriptor = open(path_ttyACM, O_RDWR | O_NOCTTY | O_NONBLOCK ); // Режим чтения/записи, для COM port, не блокирующий 
    if(COM_Port->File_Descriptor < 0){
        printf("Ошибка: не удлось открыть: %s\n", path_ttyACM);
        return 1;
    }

    if(tcgetattr(COM_Port->File_Descriptor, &COM_Port->old_tty ) != 0)
    {
        printf("Ошибка: не удлось прочитать текущие настройки порта для: %s \n", path_ttyACM);
        return 1;
    }

    memcpy(&COM_Port->tty, &COM_Port->old_tty, sizeof(struct termios));
                                                                        // Включаем АБСОЛЮТНЫЙ RAW-режим одной строчкой!
    cfmakeraw(&COM_Port->tty);

                                                                        // Настраиваем таймауты (VTIME в децисекундах)
    COM_Port->tty.c_cc[VTIME] = 10;    // Ждем до 1 секунды
    COM_Port->tty.c_cc[VMIN] = 0;      

                                                                        // Устанавливаем скорость
    cfsetispeed(&COM_Port->tty, B115200);
    cfsetospeed(&COM_Port->tty, B115200);

                                                                        // Применяем новые настройки к нашему порту немедленно (TCSANOW)
    if (tcsetattr(COM_Port->File_Descriptor, TCSANOW, &COM_Port->tty) != 0) {
        printf("Ошибка: Не удалось применить настройки порта: %s\n", path_ttyACM);
        return 1;
    }

    tcflush(COM_Port->File_Descriptor, TCIOFLUSH);                      // Отчистка буфера перед работой 
    printf("Порт: %s успешно открыт и настроен. Ожидание данных...\n", path_ttyACM);

    Thread_CDC_Device.CurrentNum_Device += 1;
    COM_Port->active = true;
    COM_Port->Device_ID = ID_dev;
    if(ID_dev != UINT16_MAX){ ID_dev++; }else{ ID_dev = 0; }


    return 0;

}

/// @brief Основная функция чтания данных из COM порта 
/// @param COMPort Структура на читаемое устройсвто 
/// @param buffer Указатель на буфер куда будут записаны данные с читаемого устройства 
/// @param size_bytes Колличесвто байт 
/// @param Timeout Тайм-аут для выхода из функции 
/// @return Колличесвто прочитаных БАЙТ из COM порта 
int USB_Read_COM(COM_Ports_Handle_t* COMPort, void* buffer, uint32_t size_bytes, uint32_t Timeout)
{
    int num_bytes_rx = 0;
    uint32_t count_bytse = 0;
    uint8_t *ptr = (uint8_t*)buffer;

    struct timespec start_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time); // Засекаем время старта

    while(count_bytse < size_bytes){

        num_bytes_rx = read(COMPort->File_Descriptor, ptr + count_bytse, size_bytes - count_bytse);
        if(num_bytes_rx == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                clock_gettime(CLOCK_MONOTONIC, &current_time);
                uint32_t elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 +
                                      (current_time.tv_nsec - start_time.tv_nsec) / 1000000;
                
                if (elapsed_ms >= Timeout) {
                    printf("Тайм-аут при чтении из %s\n", COMPort->path_ttyACM);
                    return USB_TIMEOUT;
                }
                usleep(1000); 
                continue;
            }
            perror("Ошибка функции read(): не прочитала файл\n"); 
            return USB_ERR;
        }else if(num_bytes_rx == 0){
            printf("Устройство %s отключено, ожидание тайм-аута\n", COMPort->path_ttyACM);
            return USB_ERR;
        }else if(num_bytes_rx > 0){
            printf("Принято %d Байт\n", num_bytes_rx);
            count_bytse += num_bytes_rx;
        }

    }
    return count_bytse;
}


/// @brief Функция ищет в массиве списков неактивный (свободный) список 
/// @param COMPort Принимает ТОЛЬКО НУЛЕВОЙ эллемент списка.
/// @return 
int USB_Finde_Free_Device(COM_Ports_Handle_t* COMPort)
{
    int NumberFreeDev = -1;
    uint8_t cnt = 0;

    if(COMPort != COM_Ports_Handle)
    {
        return -2;
    }

    while(cnt < SUPPORT_NUMBER_DEVICE_USB)
    {
        if( COMPort[NumberFreeDev].active == false )
        {
            return NumberFreeDev;
        }

        NumberFreeDev = (NumberFreeDev + 1) % SUPPORT_NUMBER_DEVICE_USB;
        cnt++;
    }

    return NumberFreeDev;
}

/// @brief Ищет устройство в массиве списка по пути.
/// @param path Путь искомого устройства. 
/// @param COMPort Глобальный массив списков.
/// @return Вернёт n списка в массиве, в противном случае вернёт -1.
int USB_Finde_Device_Of_Path(char *path, COM_Ports_Handle_t* COMPort)
{
    if (path == NULL || COMPort == NULL) {
        return -2;
    }

    
    if (path[0] == '\0') {
        return -2;
    }
    for(uint16_t i = 0; i < SUPPORT_NUMBER_DEVICE_USB; i++ )
    {
        if(strncmp(path, COMPort[i].path_ttyACM, strlen(path)) == 0)
        {
            return i;
        }
    }

    return -1;

}

/// @brief Закрывает устройство и восстанавливает исходные настройки терминала.
/// @param COM_Port Указатель на структуру устройства.
/// @param Thread_Devices Указатель на глобальную структуру с устройствами.
/// @return 0 при успехе, иначе -1.
int USB_Remove_Device(COM_Ports_Handle_t* COM_Port, Thread_CDC_Device_t* Thread_Devices)
{
    if (COM_Port == NULL || !COM_Port->active) {
        return -1;
    }

    if(Thread_Devices->CurrentNum_Device == 0){
        return -1;
    }

    // 1. Восстанавливаем оригинальные настройки (сбрасываем буферы)
    if (tcsetattr(COM_Port->File_Descriptor, TCSAFLUSH, &COM_Port->old_tty) != 0) {
        perror("tcsetattr восстановление");
        // Ошибка не фатальна, продолжаем закрывать
    }

    // 2. Закрываем дескриптор
    if (close(COM_Port->File_Descriptor) != 0) {
        perror("close");
        return -1;
    }

    // 3. Обнуляем состояние
    memset(COM_Port, 0x00, sizeof(COM_Ports_Handle_t));

    Thread_Devices->CurrentNum_Device -= 1;

    printf("Порт %s закрыт и настройки восстановлены.\n", COM_Port->path_ttyACM);
    return 0;
}

/// @brief Читает Head индивидульный фрейма 
/// @param COMPort Указатель на струтуру COM_Ports_Handle_t
/// @param read_head Указатель на переменную куда схораниться head фрейма 
/// @return Возвращает значение состояния enum ReadDataState_t
ReadDataState_t Read_Head_Frame(COM_Ports_Handle_t* COMPort, uint32_t *read_head)
{

    if(USB_Read_COM(COMPort, read_head, sizeof(read_head), 100) != sizeof(uint32_t)){
        tcflush(COMPort->File_Descriptor, TCIFLUSH);
        return READ_ERROR;
    }
    
    if(*read_head == ID_DUMP_FRAME_START)
    {
        return READ_HEAD_DUMP;
    }

    if(*read_head == ID_AVE_FRAME_START)
    {
        return READ_HEAD_AVE;
    }

    tcflush(COMPort->File_Descriptor, TCIFLUSH);
    return READ_NONE;
}

/// @brief 
/// @param COMPort 
/// @param DumpData_Rx 
/// @return 
int Read_Count_Frame(COM_Ports_Handle_t* COMPort, Package_t *Data_Rx)
{
    if(USB_Read_COM(COMPort, &Data_Rx->count_elements, sizeof(Data_Rx->count_elements), 100) != sizeof(Data_Rx->head_frames)) {
        tcflush(COMPort->File_Descriptor, TCIFLUSH);
        return -1; // Ошибка чтения
    }
    if(Data_Rx->count_elements < 1){
        printf("Ошибка приёма данных: слишком маленькое количество ожидаемых данных\n");
        return -1;
    }
    printf("Успешный приём head сообщения и начало ожидания приёма данных\n");
    printf("Ожидаемое количество принимаемых frame-ов %u - байт %u\n", 
           Data_Rx->count_elements, 
           Data_Rx->count_elements * sizeof(ModulData_t));
    
    return Data_Rx->count_elements;
}

int Read_Data_Payload(COM_Ports_Handle_t* COMPort, Package_t *Data_Rx)
{
    size_t bytes_to_read = Data_Rx->count_elements * sizeof(ModulData_t);
    int number_read_data = USB_Read_COM(COMPort, Data_Rx->buffer, bytes_to_read, 7000);
    if( number_read_data < (Data_Rx->count_elements * sizeof(ModulData_t)) ) {
        tcflush(COMPort->File_Descriptor, TCIFLUSH);
        return -1; // Ошибка чтения
    }
    printf("Общее колличество принятых полезных данных: %d Байт %d.%d элементов\n", number_read_data, 
                        number_read_data / sizeof(ModulData_t), number_read_data % sizeof(ModulData_t));

    return number_read_data;
}

int Read_Tail_Frame(COM_Ports_Handle_t* COMPort, Package_t *Data_Rx)
{
    if(USB_Read_COM(COMPort, &Data_Rx->tail_frames, sizeof(Data_Rx->tail_frames), 100) != sizeof(Data_Rx->head_frames)) {
        tcflush(COMPort->File_Descriptor, TCIFLUSH);
        return -1; // Ошибка чтения
    }

    if(Data_Rx->tail_frames != ID_TAIL_FRMES)
    {
        printf("Ошибка чтения tail сообщения");
        return -2;
    }

    return 0;
}


void Receive_msg(int nfds, Monitor_Msg_t *Monitor_Debug)
{
    int num_bytes = 0;
    memset(Monitor_Debug, 0x00, sizeof(Monitor_Msg_t) * SUPPORT_NUMBER_DEVICE_USB);
    for(uint16_t i = 0; i < nfds; i++ )
    {
        COM_Ports_Handle_t* COM_Ports_Active = (COM_Ports_Handle_t*)events[i].data.ptr;

        Monitor_Debug[i].activeate = true;
        Monitor_Debug[i].ID_Dev_Who_From = COM_Ports_Active->Device_ID;
        memcpy(&Monitor_Debug[i].NameDev, &COM_Ports_Active->path_ttyACM, 20);

        memset(DumpData_Rx.buffer, 0x00, TAKE_MEMORY_FOR_ELEMENTS * sizeof(ModulData_t));

        ReadDataState_t KindOfHead;
        if(events[i].events & EPOLLIN){
            uint32_t Head_Frame = 0;
            KindOfHead = Read_Head_Frame(COM_Ports_Active, &Head_Frame);

            switch (KindOfHead)
            {
            case READ_HEAD_DUMP: 
                Monitor_Debug[i].KindeOfFrame = READ_HEAD_DUMP;
                if(Read_Count_Frame(COM_Ports_Active, &DumpData_Rx) > 0)
                {
                    num_bytes = Read_Data_Payload(COM_Ports_Active, &DumpData_Rx);
                    if(num_bytes <= 0){
                        Epoll_Delete(COM_Ports_Active);
                        Monitor_Debug[i].States |= ERR_READ_DATA_PAYLOAD;
                        continue;
                    }
                    if(Read_Tail_Frame(COM_Ports_Active, &DumpData_Rx) != 0){
                        Epoll_Delete(COM_Ports_Active);
                        Monitor_Debug[i].States |= ERR_READ_TAIL;
                        continue;
                    }
                }else{
                    Monitor_Debug[i].States |= ERR_COUNT_FRAME;
                }

                if(DumpData_Rx.tail_frames != ID_TAIL_FRMES) continue;
                break;
            
            case READ_HEAD_AVE:
                Monitor_Debug[i].KindeOfFrame = READ_HEAD_AVE;
                if(Read_Count_Frame(COM_Ports_Active, &AVE_Data_Rx) > 0)
                {
                    num_bytes = Read_Data_Payload(COM_Ports_Active, &AVE_Data_Rx);
                    if(num_bytes <= 0){
                        Epoll_Delete(COM_Ports_Active);
                        Monitor_Debug[i].States |= ERR_READ_DATA_PAYLOAD;
                        continue;
                    }
                    if(Read_Tail_Frame(COM_Ports_Active, &AVE_Data_Rx) != 0){
                        Epoll_Delete(COM_Ports_Active);
                        Monitor_Debug[i].States |= ERR_READ_TAIL;
                        continue;
                    }
                }else{
                    Monitor_Debug[i].States |= ERR_COUNT_FRAME;
                }
                if(AVE_Data_Rx.tail_frames != ID_TAIL_FRMES) continue;
                break;
            
            default:
                break;
            }

        }else{
            Monitor_Debug[i].States |= NOT_EPOLLIN_FROM_EPOLL;
        }

    }

}











