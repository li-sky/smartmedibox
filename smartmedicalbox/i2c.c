#include <stdio.h>

#include <ohos_init.h>
#include <securec.h>
#include <los_base.h>
#include <cmsis_os.h>
#include <cmsis_os2.h>


#include "iot_gpio.h"
#include "iot_errno.h"
#include "iot_gpio_ex.h"

#include "iot_i2c.h"
#include "header.h"


char i2csendbuffer[3072], i2creceivebuffer[128];
osMutexId_t i2cMutex;

static void *i2c(void) {
    printf("[I2CService] i2c init\n");
    unsigned int err;
    IoTGpioInit(0);
    IoTGpioInit(1);
    IoTGpioSetFunc(0, 6);
    IoTGpioSetFunc(1, 6);
    i2cMutex = osMutexNew(NULL);
    err = IoTI2cInit(1, 400000);
    printf("[I2CService] err = %#010x, i2cinit\n", err, i2csendbuffer);
    while (1) {
        // check if buffer is not empty
        if (i2csendbuffer[0] != '\0') {
            err = IoTI2cWrite(1, (0x50<<1), i2csendbuffer, strlen(i2csendbuffer));
            printf("[I2CService] err = %#010x, i2c send: %s\n", err, i2csendbuffer);
            memset(i2csendbuffer, 0, sizeof(i2csendbuffer));
            osMutexRelease(i2cMutex);
        } 
        osDelay(10);
    }
}

static void i2cEntry(void) {
    osThreadAttr_t attr;
    
    attr.name = "i2c";
    attr.priority = (osPriority_t) osPriorityNormal;
    attr.attr_bits = 0;
    attr.cb_mem = NULL;
    attr.cb_size = 0;
    attr.stack_mem = NULL;
    attr.stack_size = 2048;

    if (osThreadNew((osThreadFunc_t) i2c, NULL, &attr) == NULL) {
        printf("[I2CService] Falied to create i2c!\n");
    }
}

APP_FEATURE_INIT(i2cEntry);