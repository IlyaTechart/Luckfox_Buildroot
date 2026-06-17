#include "epoll.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

// Скрытая реализация контекста
struct Epoll_Context {
    int epoll_fd;        // Системный дескриптор epoll
    uint32_t max_events; // Максимальное количество событий
};

Epoll_Context_t* Epoll_Create(uint32_t max_events) 
{
    Epoll_Context_t* ctx = (Epoll_Context_t*)malloc(sizeof(Epoll_Context_t));
    if (!ctx) {
        perror("Epoll_Create: malloc failed");
        return NULL;
    }

    // Создаем системный epoll
    ctx->epoll_fd = epoll_create1(0);
    if (ctx->epoll_fd == -1) {
        perror("Epoll_Create: epoll_create1 failed");
        free(ctx);
        return NULL;
    }

    ctx->max_events = max_events;
    return ctx;
}

void Epoll_Destroy(Epoll_Context_t* ctx) 
{
    if (ctx) {
        if (ctx->epoll_fd >= 0) {
            close(ctx->epoll_fd);
        }
        free(ctx);
    }
}

int Epoll_Add(Epoll_Context_t* ctx, int fd, uint32_t events_mask, void* user_data) 
{
    if (!ctx || fd < 0) return -1;

    struct epoll_event event = {0};
    event.events = events_mask;
    event.data.ptr = user_data; // Сохраняем пользовательский указатель

    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        perror("Epoll_Add: epoll_ctl failed");
        return -1;
    }
    
    return 0;
}

int Epoll_Modify(Epoll_Context_t* ctx, int fd, uint32_t events_mask, void* user_data) 
{
    if (!ctx || fd < 0) return -1;

    struct epoll_event event = {0};
    event.events = events_mask;
    event.data.ptr = user_data;

    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) {
        perror("Epoll_Modify: epoll_ctl failed");
        return -1;
    }
    
    return 0;
}

int Epoll_Remove(Epoll_Context_t* ctx, int fd) 
{
    if (!ctx || fd < 0) return -1;

    // В ядре 2.6.9+ параметр event может быть NULL для EPOLL_CTL_DEL
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        // Ошибка EBADF возникает, если fd уже был закрыт функцией close() до вызова Epoll_Remove.
        // Ошибка ENOENT возникает, если fd не был добавлен в этот epoll_fd.
        // Это не всегда критично, но может указывать на логические проблемы.
        if (errno != EBADF && errno != ENOENT) {
            perror("Epoll_Remove: epoll_ctl failed");
        }
        return -1;
    }
    
    return 0;
}

int Epoll_Wait(Epoll_Context_t* ctx, struct epoll_event* events_array, int maxevents, int timeout_ms) 
{
    if (!ctx || !events_array || maxevents <= 0) return -1;

    int nfds = epoll_wait(ctx->epoll_fd, events_array, maxevents, timeout_ms);
    if (nfds == -1) {
        if (errno == EINTR) {
            // Вызов прерван системным сигналом (например, отладчиком).
            // Это нормальная ситуация, просто возвращаем 0 событий.
            return 0;
        }
        perror("Epoll_Wait: epoll_wait failed");
        return -1;
    }
    
    return nfds;
}
