#include <hi_early_debug.h>
#include <hi_task.h>
#include <hi_types_base.h>
#include "hi_spi.h"
#include <hi_time.h>
#include <hi_stdlib.h>
#include <stdlib.h>
#include <hi_gpio.h>
#include "hi_types_base.h"
#include "math.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "spi_flash.h"

// spi flash: w25q32
const int dummyByte=0xA5;
hi_spi_idx id = HI_SPI_ID_0;
hi_spi_cfg_basic_info spi_cfg_basic_info;

void spiInit() {
    IoTGpioInit(9);
    IoTGpioInit(10);
    IoTGpioInit(11);
    IoTGpioInit(12);
    IoTGpioSetFunc(9, IOT_GPIO_FUNC_GPIO_9_SPI0_TXD);
    IoTGpioSetFunc(10, IOT_GPIO_FUNC_GPIO_10_SPI0_CK);
    IoTGpioSetFunc(11, IOT_GPIO_FUNC_GPIO_11_SPI0_RXD);
    IoTGpioSetFunc(12, IOT_GPIO_FUNC_GPIO_12_SPI0_CSN);
    IoTGpioSetDir(9, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(10, IOT_GPIO_DIR_OUT);
    IoTGpioSetDir(11, IOT_GPIO_DIR_IN);
    IoTGpioSetDir(12, IOT_GPIO_DIR_OUT);

    // setup spi

    spi_cfg_basic_info.cpha = 1;
    spi_cfg_basic_info.cpol = 1;
    spi_cfg_basic_info.data_width = HI_SPI_CFG_DATA_WIDTH_E_8BIT;
    spi_cfg_basic_info.endian = 0;
    spi_cfg_basic_info.fram_mode = 0;
    spi_cfg_basic_info.freq = 1000000;
    hi_spi_cfg_init_param spi_init_param = {0};
    spi_init_param.is_slave = 0;
    spi_init_param.pad = 0;
    int ret = hi_spi_init(id, spi_init_param, &spi_cfg_basic_info); // 基本参数配置


    if (ret != HI_ERR_SUCCESS) {
        printf("spi init failed\n");
        return;
    }
}

void readID() {
    int ret, datasent, datarecv;
    datasent = 0x9F;
    hi_spi_host_write(id, &datasent, 1);
    hi_spi_host_writeread(id, &dummyByte, &datarecv, 1);
    printf("spi read id: %#010x", datarecv);
    hi_spi_host_writeread(id, &dummyByte, &datarecv, 1);
    printf("%#010x", datarecv);
    hi_spi_host_writeread(id, &dummyByte, &datarecv, 1);
    printf("%#010x\n", datarecv);
}

void readUniqueID() {
    printf("------------UNIQUE ID------------\n");
    int datasent = 0x4B;
    hi_spi_host_write(id, &dummyByte, 1);

    for (int i = 0; i < 12; i++) {
        hi_spi_host_writeread(id, &dummyByte, &datasent, 1);
        printf("%#010x\n", datasent);
    }

}