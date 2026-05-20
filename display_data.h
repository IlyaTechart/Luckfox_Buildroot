#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>



void logger_print_one_frame(const ModulData_t *m, size_t frame_index);