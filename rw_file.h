#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "frames_structure.h"



void FormatFrameInString(char *FileBuffer, size_t buffer_size, ModulData_t *ModulDataMassive, uint32_t count_frame, char *NameDevice);