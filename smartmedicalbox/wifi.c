#include <stdio.h>

#include <ohos_init.h>
#include <securec.h>
#include <los_base.h>
#include <cmsis_os.h>
#include <cmsis_os2.h>

#include "wifi_scan_info.h"
#include "wifi_event.h"
#include "wifi_error_code.h"
#include "wifi_device_config.h"
#include "lwip/api_shell.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip_addr.h"
#include "header.h"

static bool connected;
bool wificonnected;

void onwificonnectionchange(int state, void *userdata) {
    if (state == 1) {
        printf("[WiFiService] wifi connected\n");
        connected = true;
    } else {
        printf("[WiFiService] wifi disconnected\n");
        connected = false;
    }
}

void onwifiscanstatechanged(int state, void *userdata) {
    printf("[WiFiService] wifi scan state changed: %d\n", state);
}
    


static bool *wifi(void) {
    wificonnected = false;
    WifiScanInfo *wifiScanInfo = NULL;
    unsigned int apCount = 10;
    WifiDeviceConfig config = {0};
    static struct netif *sta_netif = NULL;

    printf("[WiFiService] wifi init\n");
    WifiEvent wifiEnvHandler = {
        .OnWifiConnectionChanged = onwificonnectionchange,
        .OnWifiScanStateChanged = onwifiscanstatechanged
    };
    
    int errnum = RegisterWifiEvent(&wifiEnvHandler);
    if (errnum != WIFI_SUCCESS) {
        printf("[WiFiService] Failed to register wifi event!\n");
        return false;
    }
    if (EnableWifi() != WIFI_SUCCESS) {
        printf("[WiFiService] Failed to enable wifi!\n");
        return false;
    }

    while (!IsWifiActive()) {
        printf("[WiFiService] Waiting for wifi to be active!\n");
        osDelay(100);
    }

    config.securityType = WIFI_SEC_TYPE_PSK;
    memcpy(config.ssid, SSID, strlen(SSID));
    memcpy(config.preSharedKey, PASSWORD, strlen(PASSWORD));

    int result;
    if (AddDeviceConfig(&config, &result) != WIFI_SUCCESS) {
        printf("[WiFiService] Failed to add device config!\n");
        return false;
    }
    if (ConnectTo(result) != WIFI_SUCCESS) {
        printf("[WiFiService] Failed to connect to wifi!\n");
        return false;
    }
    sta_netif = netifapi_netif_find("wlan0");
    //wait connection result
    printf("[WiFiService] Waiting for wifi to connect!\n");
    int ConnectTimeout = 1000;
    while (ConnectTimeout > 0) {
        if (connected) {
            break;
        }
        osDelay(1);
        ConnectTimeout -= 1;
    }
    if (ConnectTimeout <= 0) {
        printf("[WiFiService] Failed to connect to wifi!\n");
        return false;
    }

    printf("[WiFiService] wifi connected\n");
    printf("[WiFiService] begin DHCP\n");
    dhcp_start(sta_netif);
    printf("[WiFiService] DHCP started\n");
    for (;;) {
        if (dhcp_is_bound(sta_netif) == ERR_OK) {
            printf("<-- DHCP state:OK -->\r\n");
            // 打印获取到的IP信息
            netifapi_netif_common(sta_netif, dhcp_clients_info_show, NULL);
            break;
        }
        osDelay(100);
    }
    wificonnected = true;
    while(1) {
        osDelay(1000);
    }
    return true;
}   


static void wifiEntry(void) {
    osThreadAttr_t attr;

    attr.name = "wifi";
    attr.priority = (osPriority_t) osPriorityNormal;
    attr.attr_bits = 0;
    attr.cb_mem = NULL;
    attr.cb_size = 0;
    attr.stack_mem = NULL;
    attr.stack_size = 4096;

    if (osThreadNew((osThreadFunc_t) wifi, NULL, &attr) == NULL) {
        printf("[WiFiService] Failed to create wifi!\n");
    }
}



APP_FEATURE_INIT(wifiEntry);