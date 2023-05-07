#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/libsais.h"

int32_t outputs[8196];

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    libsais(Data, outputs, Size, 0, NULL);
    return 0;
}