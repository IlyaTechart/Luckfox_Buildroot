#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "frames_structure.h"

typedef enum{
    FILE_WRITE = 0,
    FILE_NOT_WRITE = -1,
    FILE_NO_OPEN = -2,
    FILE_WRONG_ARGUMENT = -3
}FILE_enm_type_t;

int FormatFrameInString(char *FileBuffer, size_t buffer_size, ModulData_t *ModulDataMassive, uint32_t count_frame, char *NameDevice);
int Get_Descriptor_File_Open(char* pathFILE);
FILE_enm_type_t File_Wirite(char *buffer, uint32_t count, char* pathFILE);