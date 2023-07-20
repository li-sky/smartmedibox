
#include <cmsis_os2.h>

#define SSID "DEBUG"
#define PASSWORD "fuckyou!"
#define ENDPOINTIP "192.168.1.100"

extern bool wificonnected;
extern char i2csendbuffer[4096], i2creceivebuffer[4096];
extern osMutexId_t i2cMutex;