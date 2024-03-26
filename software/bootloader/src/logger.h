#ifndef LOGGER_H
#define LOGGER_H

#include "printf.h"

#define BL_PRINTF(...) printf_(__VA_ARGS__)
#define BL_SPRINTF(...) sprintf_(__VA_ARGS__)

#endif /* LOGGER_H */
