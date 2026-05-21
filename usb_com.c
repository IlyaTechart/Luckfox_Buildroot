
#define _POSIX_C_SOURCE 199309L
#include "usb_com.h"
#include <time.h>


COM_Ports_Handle_t COM_Ports_Handle[24] = {0};


uint32_t USB_Com_Init(COM_Ports_Handle_t* COM_Port)
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

USB_State_t USB_Read_COM(COM_Ports_Handle_t* COMPort, void* buffer, uint32_t size_bytes, uint32_t Timeout)
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

    printf("Общее колличество принятых данных: %d Байт\n", count_bytse);


    return USB_OK;
}


void USB_Com_DeInit(int File_Descriptor, uint8_t NumberDevice)
{
    close(File_Descriptor);
}







