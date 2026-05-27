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
#include <glob.h>
#include "threads.h"
#include "usb_com.h"

/*
 * ============================================================
 * Программа захвата дампов с 24 USB-устройств (ttyACM*)
 * для одноплатного компьютера Luckfox Pico Ultra.
 *
 * 1. Место хранения дампов:
 *    - Выбран каталог /userdata/dump_log/
 *    - /userdata — раздел пользовательских данных, доступный для записи
 *      и сохраняющийся между перезагрузками.
 *    - Каталог создаётся программой, если отсутствует (mkdir).           ?
 *    - Каждый дамп-файл открывается в режиме добавления (O_APPEND),
 *      чтобы при перезапуске данные не терялись.
 *
 * 2. Сопоставление устройств и файлов:
 *    - Устройства обнаруживаются по маске /dev/ttyACM*
 *    - Из имени извлекается номер: /dev/ttyACM0 -> номер 0
 *    - Формируется имя дамп-файла:
 *        /userdata/dump_log/dump_ACM<номер>
 *    - Примеры:
 *        /dev/ttyACM0  -> /userdata/dump_log/dump_ACM0
 *        /dev/ttyACM5  -> /userdata/dump_log/dump_ACM5
 *        /dev/ttyACM23 -> /userdata/dump_log/dump_ACM23
 *    - Логика реализуется через сканирование glob("/dev/ttyACM*")
 *      и snprintf() для формирования пути.
 *
 * ============================================================
 */



// Вернём количество найденных устройств (0, если ничего не найдено)  // TODO (сделать входщие значения функции в виде Thread_CDC_Device)
int Get_Discription_Connected_Devices(void)
{
    glob_t glob_result;
    int return_value;
    int found_devices = 0;
    // Ищем все файлы, подходящие под маску "/dev/ttyACM*"
    // Флаг 0 означает стандартное поведение (без специальных опций)
    return_value = glob("/dev/ttyACM*", 0, NULL, &glob_result);
    
    if (return_value == 0) {
        // glob_result.gl_pathc содержит количество найденных совпадений
        found_devices = glob_result.gl_pathc;
        printf("Найдено %d устройств(а) ttyACM.\n", found_devices);
        
        // Выводим все найденные устройства для информации
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            printf(" - %s\n", glob_result.gl_pathv[i]);
        }
        // Т.к. сейчас мы делаем функционал только для ОДНОГО устройства,
        // берём первое найденное (с индексом 0) и копируем его путь
        if (found_devices > 0) {
            snprintf(
                (char*)Thread_CDC_Device.COM_Ports_Handle[0].path_ttyACM, 
                sizeof(Thread_CDC_Device.COM_Ports_Handle[0].path_ttyACM),
                "%s", 
                glob_result.gl_pathv[0]
            );
            
            // Задаём количество устройств для инициализации потоков
            Thread_CDC_Device.TotalNumberOfDevice = 1;
        }
        
    } else if (return_value == GLOB_NOMATCH) {
        printf("Устройства /dev/ttyACM* не найдены!\n");
        Thread_CDC_Device.TotalNumberOfDevice = 0;
    } else {
        printf("Произошла ошибка при поиске устройств (код %d)\n", return_value);
        Thread_CDC_Device.TotalNumberOfDevice = 0;
    }
    
    // Обязательно освобождаем память, которую выделила функция glob
    globfree(&glob_result);
    return found_devices;
}


int main(int argc, char * argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);// Отключение буферизации вывода в терминал. 

    Thread_CDC_Device.COM_Ports_Handle = COM_Ports_Handle;
    int devices_count = Get_Discription_Connected_Devices();
    if (devices_count == 0) {
        fprintf(stderr, "Ошибка: Не найдено ни одного устройства для подключения.\n");
        return EXIT_FAILURE; // Завершаем программу, если логгер не подключен
    }
    Thread_CDC_Device.threads_cdc = thread_cdc_generic;

    int result;
    result = pthread_create(&Thread_CDC_Device.pthread, NULL, Thread_CDC_Device.threads_cdc , &Thread_CDC_Device);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: generic\n");
    return EXIT_FAILURE;
    }
    printf("Поток прёма данных создан\n");

    result = pthread_create(&pthread_display, NULL, thread_display , NULL);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: display\n");
    return EXIT_FAILURE;
    }
    printf("Поток вывода информации созадн\n");

    result = pthread_create(&pthread_filesystem, NULL, thread_filesystem , NULL);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: display\n");
    return EXIT_FAILURE;
    }
    printf("Поток вывода информации созадн\n");


    while(1)
    {

        usleep(1000000);

    }
    printf("Программа завершилась\n");
    return 0;
}