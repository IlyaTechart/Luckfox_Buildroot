
#define _POSIX_C_SOURCE 199309L
#include "usb_com.h"
#include <time.h>
#include "epoll.h"

COM_Ports_Handle_t COM_Ports_Handle[24] = {0};


/// @brief 
/// @param COM_Port 
/// @return 
uint32_t USB_Add_New_Device(COM_Ports_Handle_t* COM_Port)
{

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

    if(tcgetattr(COM_Port->File_Descriptor, &COM_Port->tty ) != 0)
    {
        printf("Ошибка: не удлось прочитать текущие настройки порта для: %s \n", path_ttyACM);
        return 1;
    }
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

    return 0;

}

/// @brief Удаляет девайс из глобального массива 
/// @param PathDevice Путь удаляемого устройства 
/// @param len Длина пути 
/// @param Thread_CDC_Device Указатель на глобальную структуру с устройствами 
/// @return 
uint32_t USB_Remove_Device(char *PathDevice, uint8_t len, Thread_CDC_Device_t *Thread_CDC_Device)
{

    uint8_t HowMuchDev = Thread_CDC_Device->TotalNumberOfDevice;
    char path[256];

    for(uint8_t i = 0; i < HowMuchDev; i++)
    {
        COM_Ports_Handle_t* COM_Port = &Thread_CDC_Device->COM_Ports_Handle[i];

        memcpy( path, COM_Port->path_ttyACM, strlen(COM_Port->path_ttyACM));


        if(strncmp(PathDevice, path, len) == 0 )
        {
            Epoll_Delete(COM_Port);
            close(COM_Port->File_Descriptor);
            memset( COM_Port, 0x00, sizeof(COM_Ports_Handle_t) );
            return 0;  
        }
    }

    return -1;

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


void USB_Com_DeInit(int File_Descriptor, uint8_t NumberDevice)
{
    close(File_Descriptor);
}

/// @brief 
/// @param COMPort 
/// @param DumpData_Rx 
/// @return 
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

int Read_Data_Dump(COM_Ports_Handle_t* COMPort, Package_t *DumpData_Rx)
{
    int number_read_data = 0;
    number_read_data = USB_Read_COM(COMPort, DumpData_Rx->buffer, DumpData_Rx->count_elements * sizeof(ModulData_t), 7000);
    if( number_read_data < (DumpData_Rx->count_elements * sizeof(ModulData_t)) ) {
        tcflush(COMPort->File_Descriptor, TCIFLUSH);
        return -1; // Ошибка чтения
    }
    printf("Общее колличество принятых полезных данных: %d Байт %d.%d элементов\n", number_read_data, 
                        number_read_data / sizeof(ModulData_t), number_read_data % sizeof(ModulData_t));

    return number_read_data;
}

int Read_AVE_Frame(COM_Ports_Handle_t* COMPort, Package_t *AVE_Data_Rx)
{
    int number_read_data = 0;
    number_read_data = USB_Read_COM(COMPort, AVE_Data_Rx->buffer, AVE_Data_Rx->count_elements * sizeof(ModulData_t), 7000);
    if( number_read_data < (AVE_Data_Rx->count_elements * sizeof(ModulData_t)) ) {
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









