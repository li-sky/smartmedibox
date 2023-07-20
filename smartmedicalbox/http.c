#include "hi_stdlib.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "cmsis_os2.h"
#include "lwip/sockets.h"
#include "lwip/netifapi.h"
#include "lwip/netdb.h"
#include "lwip/netifapi.h"
#include <string.h>
#include <stdlib.h>
#include "lwip/sockets.h"
#include "hi_mem.h"
#include "hi_config.h"
#include "http_parser.h"
#include "hi_i2s.h"
#include "hi_time.h"
#include "header.h"
#include "mp3.h"
#include "mp3dec.h"
#include "http_client.h"
#include "iot_i2c.h"
#define HTTPC_DEMO_RECV_BUFSIZE 2048*5
#define SOCK_TARGET_PORT  80

//#define ADDRESS "192.168.1.91" //"192.168.0.200"

bool bParsed = false;
int total_len = 40000;
int frame_len = 6000;
int actual_size = 0;

static char current_header_key[64];
static char get_request[512];
static char header_bytes[256];
static char recv_buf[HTTPC_DEMO_RECV_BUFSIZE];

void audio_http_parser_init();
void audio_http_parser_exec(char *buf,int len);
/*****************************************************************************
* Func description: demo for http get action
*****************************************************************************/
unsigned int audio_http_clienti_get(char* dir, int start, int end)
{
    struct sockaddr_in addr = {0};
    int s, r;
    addr.sin_family = AF_INET;
    addr.sin_port = PP_HTONS(SOCK_TARGET_PORT);
    //addr.sin_port =  my_htons(SOCK_TARGET_PORT);
    addr.sin_addr.s_addr = ipaddr_addr(ENDPOINTIP);
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return 1;
    }
    printf("... allocated socket %d\r\n",s);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("... socket connect failed.\r\n");
        lwip_close(s);
        return 1;
    }
    printf("... connected\r\n");

    int length = frame_len -1;

    int body_recv_len = 0;

    int down_start_time = hi_get_milli_seconds();
    //printf("******start********\r\n");
    sprintf(header_bytes,"Range:bytes=%d-%d\r\n",start,end);
    sprintf(get_request,"GET %s HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded;charset=UTF-8\r\nConnection: Keep-Alive\r\nHost: smartmedibox.skyli.xyz\r\n%s\r\n",
    dir,
    header_bytes);
    int count = 0;
    while (lwip_write(s, get_request, strlen(get_request)) < 0) {
        count++;
        // try reconnect
        lwip_close(s);
        s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            continue;
        }
        if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        {
            continue;
        }
        if (count > 10)
        {
            printf("... socket send failed.\r\n");
            lwip_close(s);
            return 1;
        }
    }
    struct timeval receiving_timeout;
    
    /* 5S Timeout */
    /*
    receiving_timeout.tv_sec = 1;
    receiving_timeout.tv_usec = 0;
    if (my_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
        printf("setsockopt = %d,%d,%d\r\n",s,SOL_SOCKET,SO_RCVTIMEO);
        printf("... failed to set socket receiving timeout\r\n");
        lwip_close(s);
        return 1;
    }
    printf("... set socket receiving timeout success\r\n");
    */
    audio_http_parser_init();
    /* Read HTTP response */
    do {
        (void)memset_s(recv_buf, sizeof(recv_buf), 0, sizeof(recv_buf));
        r = lwip_read(s, recv_buf, sizeof(recv_buf) - 1);
        
        //for (int i = 0; i < r; i++) {
        //    putchar(recv_buf[i]);
        //}
        printf("-%d-",r);
        audio_http_parser_exec(recv_buf,r);
        if (bParsed)
        {
            bParsed = false;
            break;
        }
    } while (r > 0);
    //printf("... done reading from socket. Last read return=%d\r\n", r);
    lwip_close(s);
    int down_end_time = hi_get_milli_seconds();
    printf("down load time = %d ms\r\n",down_end_time - down_start_time);
    return 0;
}

int on_header_field(http_parser* _, const char* at, size_t length) {
    (void)_;
    //printf("Header field: %.*s\n", (int)length, at);
    strncpy(current_header_key,at,length);
    current_header_key[length] = '\0';
    return 0;
}
int on_header_value(http_parser* _, const char* at, size_t length) {
    (void)_;
    //printf("Header %s value: %.*s\n",current_header_key, (int)length, at);
    if (strcmp(current_header_key, "Content-Range") == 0) 
    {
        int s=0,e=0,t=0;
        sscanf(at,"bytes %d-%d/%d",&s,&e,&t);
        actual_size = e - s + 1;
        printf("actual_size:%d\r\n",actual_size);
        //printf("parser=%d,%d,%d\r\n",s,e,t);
        total_len = t;
    }
    return 0;
}

uint8_t *audio_data_buff;
int audio_data_len = 0; 

int on_body(http_parser* _, const char* at, size_t length) {
  (void)_;
  //printf("Body: %.*s\n", (int)length, at);
  memcpy(audio_data_buff,at,length);
  audio_data_len = length;
  //printf("Body: %d\n", (int)length);
  return 0;
}
int on_message_begin(http_parser* _) {
  (void)_;
  //printf("\n***MESSAGE BEGIN***\n\n");
  return 0;
}

int on_message_complete(http_parser* _) {
  (void)_;
  //printf("\n***MESSAGE COMPLETE***\n\n");
  bParsed = true;
  return 0;
}

int on_headers_complete(http_parser* parser) {
    printf("Content-Length: %llu\n", parser->content_length);
    return 0;
}

http_parser_settings settings;
http_parser parser;  
void audio_http_parser_init(char *buf,int len)
{
    http_parser_settings_init(&settings);
    settings.on_header_field = on_header_field;
    settings.on_header_value = on_header_value;
    settings.on_body = on_body;
    settings.on_headers_complete = on_headers_complete;
	settings.on_message_complete = on_message_complete;

    http_parser_init(&parser, HTTP_RESPONSE);
}

void audio_http_parser_exec(char *buf,int len)
{
    http_parser_execute(&parser, &settings, buf,len);  //执行解析过程
    if (bParsed)
    {
        printf("#");
        audio_data_len = frame_len;
        //bParsed = false;
    }
}

static uint32_t fd_fetch(void *parameter, uint8_t *buffer, uint32_t length)
{
    struct fetch_param *fetchpar = (struct fetch_param *)parameter;
    audio_data_buff = buffer;
    if (fetchpar->start >= total_len) {
        return 0;
    }
    int ret = audio_http_clienti_get(fetchpar->dir, fetchpar->start, fetchpar->start + length - 1);
    fetchpar->start = fetchpar->start + length;
    if (ret != 0) {
        printf("Error: audio_http_clienti_get failed!\n");
        return 0;
    }
    return actual_size;
}

int audio_is_playing = 0;

typedef struct  {
    void* samples;
    int numsamples;
}audiobufferparam;

void ProvideAudioBuffer(void *samples, hi_u32 numsamples) {
    printf("providing audio buffer\n");
    hi_u8* buffer;
    buffer = (hi_u8 *)malloc(numsamples * 2);
    for (int i = 0; i < numsamples; i++) {
        buffer[i * 2] = ((hi_u8 *)samples)[i * 2 + 1];
        buffer[i * 2 + 1] = ((hi_u8 *)samples)[i * 2];
    }
    printf("error hex %d\n",hi_i2s_write(buffer, numsamples*2, 10));
    audio_is_playing = 0;
}

static uint32_t mp3_callback(MP3FrameInfo *header,
                             int16_t *buffer,
                             uint32_t length) {
    while (audio_is_playing == 1);
    audio_is_playing = 1;
    
    osThreadAttr_t attr;
    attr.priority = (osPriority_t) osPriorityNormal;
    attr.attr_bits = 0;
    attr.cb_mem = NULL;
    attr.cb_size = 0;
    attr.stack_mem = NULL;
    attr.stack_size = 2048;
    audiobufferparam buff;
    buff.samples = buffer;
    buff.numsamples = length;
    osThreadNew((osThreadFunc_t)ProvideAudioBuffer, &buff, &attr);

    return 0;
}

int playmp3(char *url)
{
    struct mp3_decoder *decoder = mp3_decoder_create();
    if (!decoder) {
        printf("Error: mp3_decoder_create failed!\n");
        return -1;
    }
    struct fetch_param fetchpar;
    fetchpar.dir = url;
    fetchpar.start = 0;
    decoder->fetch_data = fd_fetch;
    decoder->fetch_parameter = &fetchpar;
    decoder->output_cb = mp3_callback;
    while (mp3_decoder_run(decoder) != -1);
    mp3_decoder_delete(decoder);
}

