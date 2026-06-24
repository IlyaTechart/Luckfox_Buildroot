#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <ncurses.h>
#include "frames_structure.h"





// Коды клавиш
// enum {
//     KEY_ESC   = 27,
//     KEY_ENTER = 10,
//     KEY_UP    = 256,
//     KEY_DOWN,
//     KEY_LEFT,
//     KEY_RIGHT
// };



typedef enum {
    UI_STATE_MAIN_MENU,
    UI_STATE_LOG_VIEWER,       // Режим 1: Просмотр логов
    UI_STATE_CSV_CONTROL,      // Режим 2: Управление записью CSV
    UI_STATE_LIVE_AVE,         // Режим 3: Просмотр Live данных
    UI_STATE_USB_DEVICES,      // Режим 4: Список USB устройств
    UI_STATE_EXIT              // Состояние для выхода из меню 
} UI_State_t;

typedef enum {
    LOG_COLOR_DEFAULT = 0,
    LOG_COLOR_RED,
    LOG_COLOR_GREEN,
    LOG_COLOR_YELLOW,
    LOG_COLOR_BLUE,
    LOG_COLOR_MAGENTA,
    LOG_COLOR_CYAN,
    LOG_COLOR_WHITE
} logger_color_t;


// ============================================================
// 1. Базовые управляющие коды ASCII
// ============================================================
#define ANSI_BELL     "\x07"  // Звуковой сигнал
#define ANSI_BS       "\x08"  // Возврат на позицию (Backspace)
#define ANSI_TAB      "\x09"  // Горизонтальная табуляция
#define ANSI_LF       "\x0A"  // Перевод строки (Line Feed)
#define ANSI_VT       "\x0B"  // Вертикальная табуляция
#define ANSI_FF       "\x0C"  // Перевод страницы (Form Feed)
#define ANSI_CR       "\x0D"  // Возврат каретки в начало строки

// ============================================================
// 2. Управление курсором
// ============================================================
// Перемещение курсора
#define ANSI_HOME     "\x1B[H"      // В начало экрана (0,0)
#define ANSI_CURSOR   "\x1B[%d;%dH" // Строка;Колонка (требует формат)
#define ANSI_UP       "\x1B[%dA"    // Вверх на N строк
#define ANSI_DOWN     "\x1B[%dB"    // Вниз на N строк
#define ANSI_RIGHT    "\x1B[%dC"    // Вправо на N колонок
#define ANSI_LEFT     "\x1B[%dD"    // Влево на N колонок
#define ANSI_NEXTLINE "\x1B[%dE"    // В начало следующей строки (на N вниз)
#define ANSI_PREVLINE "\x1B[%dF"    // В начало предыдущей строки (на N вверх)
#define ANSI_COLUMN   "\x1B[%dG"    // В указанную колонку N

// Сохранение и восстановление позиции курсора (DEC - стандарт xterm)
#define ANSI_SAVE_DEC "\x1B 7"      // Сохранить позицию (DEC)
#define ANSI_REST_DEC "\x1B 8"      // Восстановить позицию (DEC)

// Сохранение и восстановление позиции курсора (SCO - совместимость)
#define ANSI_SAVE_SCO "\x1B[s"      // Сохранить позицию (SCO)
#define ANSI_REST_SCO "\x1B[u"      // Восстановить позицию (SCO)

// Запрос позиции курсора (терминал ответит ESC[#;#R)
#define ANSI_GETPOS   "\x1B[6n"     // Запрос позиции

// Сдвиг на одну строку вверх с прокруткой
#define ANSI_SCROLLUP "\x1B M"      // Переместить курсор вверх на 1 строку

// Очистка экрана / линий
#define ANSI_CLRSCREEN "\x1B[2J"    // Очистить весь экран
#define ANSI_CLRLINE  "\x1B[2K"     // Очистить всю строку
#define ANSI_CLREOL   "\x1B[0K"     // Очистить от курсора до конца строки
#define ANSI_CLRBOL   "\x1B[1K"     // Очистить от начала строки до курсора

// ============================================================
// 3. Стили текста (SGR - Select Graphic Rendition)
// ============================================================
// --- Стили ---
#define ANSI_RESET    "\x1B[0m"     // Сбросить все стили
#define ANSI_BOLD     "\x1B[1m"     // Жирный текст
#define ANSI_DIM      "\x1B[2m"     // Тусклый (пониженная яркость)
#define ANSI_ITALIC   "\x1B[3m"     // Курсив
#define ANSI_UNDER    "\x1B[4m"     // Подчёркнутый
#define ANSI_BLINKSLOW "\x1B[5m"    // Медленное мигание
#define ANSI_BLINKFAST "\x1B[6m"    // Быстрое мигание
#define ANSI_REVERSE  "\x1B[7m"     // Инвертировать цвета
#define ANSI_HIDE     "\x1B[8m"     // Скрытый (невидимый)
#define ANSI_STRIKE   "\x1B[9m"     // Зачёркнутый

// --- Цвета текста (стандартные) ---
#define ANSI_BLACK    "\x1B[30m"
#define ANSI_RED      "\x1B[31m"
#define ANSI_GREEN    "\x1B[32m"
#define ANSI_YELLOW   "\x1B[33m"
#define ANSI_BLUE     "\x1B[34m"
#define ANSI_MAGENTA  "\x1B[35m"
#define ANSI_CYAN     "\x1B[36m"
#define ANSI_WHITE    "\x1B[37m"

// --- Яркие цвета текста ---
#define ANSI_BBLACK   "\x1B[90m"
#define ANSI_BRED     "\x1B[91m"
#define ANSI_BGREEN   "\x1B[92m"
#define ANSI_BYELLOW  "\x1B[93m"
#define ANSI_BBLUE    "\x1B[94m"
#define ANSI_BMAGENTA "\x1B[95m"
#define ANSI_BCYAN    "\x1B[96m"
#define ANSI_BWHITE   "\x1B[97m"

// --- Цвета фона (стандартные) ---
#define ANSI_BG_BLACK   "\x1B[40m"
#define ANSI_BG_RED     "\x1B[41m"
#define ANSI_BG_GREEN   "\x1B[42m"
#define ANSI_BG_YELLOW  "\x1B[43m"
#define ANSI_BG_BLUE    "\x1B[44m"
#define ANSI_BG_MAGENTA "\x1B[45m"
#define ANSI_BG_CYAN    "\x1B[46m"
#define ANSI_BG_WHITE   "\x1B[47m"

// --- Яркие цвета фона ---
#define ANSI_BG_BBLACK   "\x1B[100m"
#define ANSI_BG_BRED     "\x1B[101m"
#define ANSI_BG_BGREEN   "\x1B[102m"
#define ANSI_BG_BYELLOW  "\x1B[103m"
#define ANSI_BG_BBLUE    "\x1B[104m"
#define ANSI_BG_BMAGENTA "\x1B[105m"
#define ANSI_BG_BCYAN    "\x1B[106m"
#define ANSI_BG_BWHITE   "\x1B[107m"

// ============================================================
// 4. Другие полезные последовательности
// ============================================================
#define ANSI_RESET_TERM  "\x1Bc"       // Полный сброс терминала (RIS)
#define ANSI_SETFONT_ASCII "\x1B(B"    // Выбрать шрифт US ASCII

// Показать/скрыть курсор (DECTCEM)
#define ANSI_CURSOR_SHOW "\x1B[?25h"   // Показать курсор
#define ANSI_CURSOR_HIDE "\x1B[?25l"   // Скрыть курсор

// Альтернативный буфер экрана (используется в vim, less, htop)
#define ANSI_ALT_SCREEN_SAVE "\x1B[?1049h"  // Переключиться в альт. буфер + сохранить
#define ANSI_ALT_SCREEN_REST "\x1B[?1049l"  // Вернуться из альт. буфера + восстановить

// Включение/выключение переноса строк (DECAWM)
#define ANSI_WRAP_ON  "\x1B[?7h"  // Включить перенос строк (по умолчанию)
#define ANSI_WRAP_OFF "\x1B[?7l"  // Отключить перенос строк

// ============================================================
// 5. Вспомогательные макросы для удобного форматирования
// ============================================================
#define ANSI_FMT(styled_str)  ANSI_RESET styled_str ANSI_RESET
#define ANSI_FMT_COLOR(color, str)  color str ANSI_RESET

// Пример использования:
// printf(ANSI_RED "Это красный текст" ANSI_RESET "\n");
// printf(ANSI_CURSOR, 5, 10);  // Переместить курсор на 5 строку, 10 колонку
// printf(ANSI_UP, 3);          // Переместить курсор на 3 строки вверх




// Инициализация графической подсистемы
void Display_Init(void);

// Очистка перед выходом
void Display_Deinit(void);

// Главная функция-обработчик экрана (вызывается в цикле)
// Возвращает false, если пользователь выбрал выход из программы
bool Display_Task_Loop(void);

// Очередь для логов (заглушка для будущего)
void Logger_Print(const char* format, ...);

void logger_print_one_frame(const ModulData_t *m, size_t frame_index, logger_color_t color);


