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










void logger_print_one_frame(const ModulData_t *m, size_t frame_index);


