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
#include "epoll.h"



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
        if(found_devices > 0){
            for(uint8_t WriteDev = 0; WriteDev < found_devices; WriteDev++ ){
                snprintf( (char*)Thread_CDC_Device.COM_Ports_Handle[WriteDev].path_ttyACM, 
                sizeof(Thread_CDC_Device.COM_Ports_Handle[WriteDev].path_ttyACM), "%s", glob_result.gl_pathv[WriteDev]
                );
            }
            // Задаём количество устройств для инициализации потоков
            Thread_CDC_Device.NumberDev_of_Init = found_devices;
        }
        
    } else if (return_value == GLOB_NOMATCH) {
        printf("Устройства /dev/ttyACM* не найдены!\n");
        Thread_CDC_Device.NumberDev_of_Init = 0;
    } else {
        printf("Произошла ошибка при поиске устройств (код %d)\n", return_value);
        Thread_CDC_Device.NumberDev_of_Init = 0;
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

    if (pipe(hotplug_pipe) == -1) {    // Делайет 2 фаловых дискриптора из массива hotplug_pipe[0] — дескриптор для чтения, hotplug_pipe[1] — дескриптор для записи
        perror("Ошибка создания pipe");
        return EXIT_FAILURE;
    }

    Epoll_User_Data_t pipe_epoll_data = {
    .type = EPOLL_SOURCE_HOTPLUG_PIPE,
    .fd = hotplug_pipe[0],
    .custom_data = NULL
    };
    Epoll_Context_t* Epoll_Context_Pipe = Epoll_Create(NUMBERS_EVENTS_PIPE);
    if(Epoll_Add(Epoll_Context_Pipe, hotplug_pipe[0], EPOLLIN, &pipe_epoll_data) != 0)
    {
        printf("Pipe не был добавлен в epoll\n");
    }


    USB_Buffers_Init();



    int result;

    result = pthread_create(&pthread_kernel_events, NULL, thread_kernel_events, NULL);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: kernel_events\n");
    return EXIT_FAILURE;
    }
    printf("Поток kernel_events создан\n");

    result = pthread_create(&pthread_heandler_karnel_event, NULL, thread_heandler_karnel_event, Epoll_Context_Pipe);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: heandler_karnel_event\n");
    return EXIT_FAILURE;
    }
    printf("Поток heandler_karnel_event создан\n");

    result = pthread_create(&pthread_cdc_generic, NULL, thread_cdc_generic , &Thread_CDC_Device);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: generic\n");
    return EXIT_FAILURE;
    }
    printf("Поток cdc_generic создан\n");

    result = pthread_create(&pthread_display, NULL, thread_display , NULL);
    if (result != 0) {
    fprintf(stderr,"Не удалось создать поток: display\n");
    return EXIT_FAILURE;
    }
    printf("Поток display созадн\n");

    // result = pthread_create(&pthread_filesystem, NULL, thread_filesystem , NULL);
    // if (result != 0) {
    // fprintf(stderr,"Не удалось создать поток: display\n");
    // return EXIT_FAILURE;
    // }
    // printf("Поток вывода информации созадн\n");


    // const char *green = "\033[32m";  // зелёный текст
    // const char *white = "\033[37m";  // белый текст
    // const char *blue = "\033[35m";  // белый текст
    // const char *reset = "\033[0m";   // сброс всех атрибутов

    // const char *setcursor = "\x1b[s"; // Сохранить текущую позицию курсора
    // const char *restorecursor = "\x1b[u"; // Восстановить сохраненную позицию курсора
    // const char *setcornercursor = "\x1b[H"; // Переместить курсор в левый верхний угол (1,1)
  
    // const char *clearsring = "\x1b[2K"; // Очистить всю строку, где находится курсор



    // sleep(2);
    // char down_dispay[30];
    // memset(down_dispay,'\n', sizeof(down_dispay));
    // printf("%s", down_dispay);
    // printf("%s", setcornercursor);
    // printf("%s", blue);
    // printf("%s", setcursor);
    // printf("%s", clearsring);


    // int sumbol[4];
    // sumbol[0] = getchar();
    // sumbol[1] = getchar();
    // sumbol[2] = getchar();
    // sumbol[3] = getchar();
    // printf("Первый %d, Второй %d, Третий %d, Четвёртый %d\n", sumbol[0], sumbol[1], sumbol[2], sumbol[3]);
    while(1)
    {

        // printf("========================================================================\n");
        // printf("                             Главное меню                               \n");
        // printf("========================================================================\n");
        // printf("\n");
        // printf("1) Показать поток данных\n");
        // printf("2) Зайти в меню дампа\n");

        // printf("%s", restorecursor);

        sleep(2);

    }
    printf("Программа завершилась\n");
    return 0;
}