#ifndef __RF_IF_APP_H__
#define __RF_IF_APP_H__
#include <stdint.h>
#include "apl_utils/apl_utils.h"

#define TCP_CLIENT_RECV_SIZE        256                         // TCP客户端缓存区大小

typedef struct {
  hi_u32 boot_mode;
  hi_u32 app_en;
  hi_u32 flag;
  hi_u32 sensor_type;           // sensor_node类型
  hi_u32 link_type;             //网络连接类型 0：连接智云 1：UDP连接网关  
} t_nv_base_info;

typedef struct {
  char mac[40];                 //模块mac
  char wifi_ssid[40];           //wifi ssid
  char wifi_key[20];            //wifi key
} t_nv_wifi_info;

typedef struct {
  char ip[40];                  //连接IP
  char zhiyun_id[40];           //智云id
  char zhiyun_key[120];         //智云key
  hi_u32 port;                  //连接Port
} t_nv_zhiyun_info;

typedef struct {
  t_nv_base_info base;
  t_nv_wifi_info wifi;
  t_nv_zhiyun_info zhiyun;
} t_rf_info;

extern t_rf_info rf_info;

void rf_app_init(void);
void rf_info_init(void);
int rf_if_Link(void);
int rf_if_recv(void);
void rf_set_link_status(uint8_t new);
int16_t rf_send(uint8_t *pdata, uint16_t length, uint16_t timeout_ms);

#endif  //__RF_IF_APP_H__
