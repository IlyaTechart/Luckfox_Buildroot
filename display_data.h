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
    LOG_COLOR_DEFAULT = 0,
    LOG_COLOR_RED,
    LOG_COLOR_GREEN,
    LOG_COLOR_YELLOW,
    LOG_COLOR_BLUE,
    LOG_COLOR_MAGENTA,
    LOG_COLOR_CYAN,
    LOG_COLOR_WHITE
} logger_color_t;






void logger_print_one_frame(const ModulData_t *m, size_t frame_index, logger_color_t color);


