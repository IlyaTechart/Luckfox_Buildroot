#pragma once

#include <stdint.h>
#include <sys/epoll.h>

// Типы сущностей, которые мы слушаем в epoll.
// Это поможет вам отличать события USB от событий Pipe при выходе из Epoll_Wait.
typedef enum {
    EPOLL_SOURCE_USB_CDC,
    EPOLL_SOURCE_HOTPLUG_PIPE,
    EPOLL_SOURCE_UNKNOWN
} Epoll_Source_Type_t;

// Единая обертка для передачи данных через event.data.ptr
// Вы будете создавать её экземпляр для каждого USB-порта и для Pipe
typedef struct {
    Epoll_Source_Type_t type; // Тип источника (USB или Pipe)
    int fd;                   // Файловый дескриптор
    void* custom_data;        // Указатель на ваши данные (например, на COM_Ports_Handle_t)
} Epoll_User_Data_t;

// Опрозрачный (opaque) тип контекста epoll (сама структура скрыта в .c файле)
typedef struct Epoll_Context Epoll_Context_t;

/**
 * @brief Создать новый экземпляр epoll
 * @param max_events Максимальное количество событий (для информации)
 * @return Указатель на контекст или NULL при ошибке
 */
Epoll_Context_t* Epoll_Create(uint32_t max_events);

/**
 * @brief Удалить экземпляр epoll и освободить память
 * @param ctx Указатель на контекст
 */
void Epoll_Destroy(Epoll_Context_t* ctx);

/**
 * @brief Добавить дескриптор в epoll
 * @param ctx Указатель на контекст
 * @param fd Файловый дескриптор (USB или Pipe)
 * @param events_mask Флаги событий (например, EPOLLIN)
 * @param user_data Указатель на пользовательские данные (обычно на Epoll_User_Data_t)
 * @return 0 при успехе, -1 при ошибке
 */
int Epoll_Add(Epoll_Context_t* ctx, int fd, uint32_t events_mask, void* user_data);

/**
 * @brief Изменить настройки слежения за дескриптором
 * @param ctx Указатель на контекст
 * @param fd Файловый дескриптор
 * @param events_mask Новые флаги событий
 * @param user_data Указатель на пользовательские данные
 * @return 0 при успехе, -1 при ошибке
 */
int Epoll_Modify(Epoll_Context_t* ctx, int fd, uint32_t events_mask, void* user_data);

/**
 * @brief Удалить дескриптор из epoll
 * @param ctx Указатель на контекст
 * @param fd Файловый дескриптор
 * @return 0 при успехе, -1 при ошибке
 */
int Epoll_Remove(Epoll_Context_t* ctx, int fd);

/**
 * @brief Ждать события от epoll
 * @param ctx Указатель на контекст
 * @param events_array Указатель на массив структур epoll_event (куда запишутся результаты)
 * @param maxevents Размер массива events_array
 * @param timeout_ms Таймаут в миллисекундах (-1 для бесконечного ожидания)
 * @return Количество произошедших событий, 0 при таймауте/EINTR, -1 при ошибке
 */
int Epoll_Wait(Epoll_Context_t* ctx, struct epoll_event* events_array, int maxevents, int timeout_ms);
