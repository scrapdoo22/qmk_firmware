#pragma once

#include_next <halconf.h>

#undef HAL_USE_SERIAL
#define HAL_USE_SERIAL TRUE

#undef HAL_USE_I2C
#define HAL_USE_I2C TRUE