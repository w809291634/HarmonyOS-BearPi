#ifndef __ZXBEE_H__
#define __ZXBEE_H__

#define ZIGBEE_CONFIG_RADIO_TYPE        1
#define ZIGBEE_CONFIG_DEVICE_TYPE       2

#define WIFI_CONFIG_RADIO_TYPE          3
#define WIFI_CONFIG_DEVICE_TYPE         2       //end device

#define LORA_CONFIG_RADIO_TYPE          8
#define LORA_CONFIG_DEVICE_TYPE         2       //end device
    
#define NB_CONFIG_RADIO_TYPE            7
#define NB_CONFIG_DEVICE_TYPE           2       //end device

#define LTE_CONFIG_RADIO_TYPE           6
#define LTE_CONFIG_DEVICE_TYPE          2

void zxbee_onrecv_fun(char *pkg, int len);
void zxbee_update_fun(void);

#endif