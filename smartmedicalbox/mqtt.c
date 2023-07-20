#include <stdio.h>

#include <ohos_init.h>
#include <securec.h>
#include <los_base.h>
#include <cmsis_os.h>
#include <cmsis_os2.h>

#include "wifi_error_code.h"
#include "lwip/api_shell.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip_addr.h"
#include "MQTTClient.h"
#include "header.h"
#include "hi_io.h"
#include "hi_gpio.h"
#include "http_client.h"
#include "iot_gpio.h"
#include "iot_errno.h"
#include "iot_gpio_ex.h"
#include "iot_i2c.h"
#include "hi_i2s.h"

#define MQTT_CMD_TIMEOUT_MS 2000
#define MQTT_KEEP_ALIVE_MS 2000
#define MQTT_DELAY_2S 200
#define MQTT_DELAY_500_MS 50
#define MQTT_VERSION 3
#define MQTT_QOS 2
#define MQTT_TASK_STACK_SIZE (1024 * 10)

static unsigned char sendbuf[128], readbuf[128];
static unsigned char msgBuf[128];
Network network;
void messageArrived(MessageData *data)
{
    printf("Message arrived on topic %.*s: %.*s\n", data->topicName->lenstring.len,
        data->topicName->lenstring.data, data->message->payloadlen, data->message->payload);
    sscanf(data->message->payload, "%s", msgBuf);
    printf("msgBuf: %s\n", msgBuf);
    if (strcmp(msgBuf, "open") == 0) {
        printf("open\n");
    }
    if (strcmp(msgBuf, "play") == 0) {
        printf("play\n");
        sscanf(data->message->payload, "%*s %s", msgBuf);
        playmp3(msgBuf);
    }
}

static void mqtt(void)
{
    IoTGpioSetFunc(9, IOT_GPIO_FUNC_GPIO_9_I2S0_MCLK);
    IoTGpioSetFunc(10, IOT_GPIO_FUNC_GPIO_10_I2S0_TX);
    IoTGpioSetFunc(11, IOT_GPIO_FUNC_GPIO_11_I2S0_RX);
    IoTGpioSetFunc(12, IOT_GPIO_FUNC_GPIO_12_I2S0_BCLK);
    IoTGpioSetFunc(8, IOT_GPIO_FUNC_GPIO_8_I2S0_WS);
    /* BCLK */
    IoTGpioSetDir(12, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(12, IOT_GPIO_VALUE1);
    hi_i2s_attribute i2s_cfg = {
        .sample_rate = HI_I2S_SAMPLE_RATE_48K,
        .resolution = HI_I2S_RESOLUTION_16BIT,
    };
    hi_u32 err = hi_i2s_init(&i2s_cfg);
        if (err != HI_ERR_SUCCESS) {
        printf("Failed to init i2s %u!\n", err);
        return;
    }
    printf("I2s init success!\n");
    while (!wificonnected) {
        printf("[MQTTService] Waiting for wifi to be active!\n");
        osDelay(10);
    }
    printf("Starting ...\n");
    int rc, count = 0;
    MQTTClient client;

    NetworkInit(&network);
    printf("NetworkConnect  ...\n");

    NetworkConnect(&network, ENDPOINTIP, 1883);
    printf("MQTTClientInit  ...\n");
    MQTTClientInit(&client, &network, MQTT_CMD_TIMEOUT_MS, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));

    MQTTString clientId = MQTTString_initializer;
    clientId.cstring = "smb";

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.clientID = clientId;
    data.willFlag = 0;
    data.MQTTVersion = MQTT_VERSION;
    data.keepAliveInterval = MQTT_KEEP_ALIVE_MS;
    data.cleansession = 1;

    printf("MQTTConnect  ...\n");
    rc = MQTTConnect(&client, &data);
    if (rc != 0) {
        printf("MQTTConnect: %d\n", rc);
        NetworkDisconnect(&network);
        MQTTDisconnect(&client);
        osDelay(MQTT_DELAY_2S);
    }

    printf("MQTTSubscribe  ...\n");
    rc = MQTTSubscribe(&client, "command", MQTT_QOS, messageArrived);
    if (rc != 0) {
        printf("MQTTSubscribe: %d\n", rc);
        osDelay(MQTT_DELAY_2S);
    }
    while (++count) {
        MQTTMessage message;
        char payload[30];

        message.qos = MQTT_QOS;
        message.retained = 0;
        message.payload = payload;
        (void)sprintf_s(payload, sizeof(payload), "message number %d", count);
        message.payloadlen = strlen(payload);

        if ((rc = MQTTPublish(&client, "pubtopic", &message)) != 0) {
            printf("Return code from MQTT publish is %d\n", rc);
            NetworkDisconnect(&network);
            MQTTDisconnect(&client);
            NetworkConnect(&network, "120.46.33.128", 1883);
            MQTTConnect(&client, &data);
            rc = MQTTSubscribe(&client, "command", MQTT_QOS, messageArrived);
            if (rc != 0) {
                printf("MQTTSubscribe: %d\n", rc);
                osDelay(MQTT_DELAY_2S);
            }
        }
        osDelay(100);
    }
}

static void mqttEntry(void) {
    osThreadAttr_t attr;
    
    attr.name = "mqtt";
    attr.priority = (osPriority_t) osPriorityNormal;
    attr.attr_bits = 0;
    attr.cb_mem = NULL;
    attr.cb_size = 0;
    attr.stack_mem = NULL;
    attr.stack_size = 1024*10;

    if (osThreadNew((osThreadFunc_t) mqtt, NULL, &attr) == NULL) {
        printf("[MQTTService] Falied to create mqtt!\n");
    }
}

APP_FEATURE_INIT(mqttEntry); 