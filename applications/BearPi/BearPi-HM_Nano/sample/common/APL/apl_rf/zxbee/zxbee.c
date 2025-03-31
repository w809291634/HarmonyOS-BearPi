#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "apl_rf/zxbee/zxbee.h"
#include "apl_rf/rf_thread.h"
#include "apl_rf/rf_if_hw.h"
#include "apl_rf/rf_if_app.h"

#include "cmsis_os2.h"
#include "hos_types.h"
#include "wifi_device.h"
#include "wifiiot_errno.h"
#include "ohos_init.h"

#ifdef CONFIG_SENSOR_NODE
#include "sensor/sensor_node.h"
#else
#include "sensor/sensor.h"
#endif

//外部引用变量
#ifndef USE_RTOS2_API
extern UINT32 rf_zxbee_mutex;                               /* zxbee处理锁 */
extern UINT32 rf_send_mutex;                                /* 无线发送锁 */
#else
extern osMutexId_t rf_zxbee_mutex;                          /* zxbee处理锁 */
extern osMutexId_t rf_send_mutex;                           /* 无线发送锁 */
#endif

//变量
#define WBUF_LEN    200
static char wbuf[WBUF_LEN];

static void zxbeeBegin(void) {
  wbuf[0] = '{';
  wbuf[1] = '\0';
}

static char* zxbeeEnd(void) {
  int offset = strlen(wbuf);
  wbuf[offset-1] = '}';
  wbuf[offset] = '\0';
  if (offset > 2) return wbuf;
  return NULL;
}

static int zxbeeAdd(char* tag, char* val) {
  int offset = strlen(wbuf);
  if(offset > WBUF_LEN-1) return -1;
  sprintf(&wbuf[offset],"%s=%s,", tag, val);
  return 0;
}

/* 获取节点类型；成功返回0，失败返回-1 */
static int8_t _node_type(char *pval) {
  extern t_rf_info rf_info;
  uint8_t radio_type, device_type;
  
  radio_type = WIFI_CONFIG_RADIO_TYPE;
  device_type = WIFI_CONFIG_DEVICE_TYPE;
#ifdef CONFIG_SENSOR_NODE
  sprintf(pval, "%u%u%03u", radio_type, device_type, rf_info.sensor_type);
#else
  sprintf(pval, "%u%u%03u", radio_type, device_type, SENSOR_TYPE);
#endif
  return 0;
}

static void sensor_process_tag(char *ptag, char *pval) {
  char buf[50];
  if(strcmp(ptag, "TYPE") == 0) {
    if (pval[0] == '?') {   //{TYPE=?}
      if(_node_type(buf) == 0) {
        zxbeeAdd(ptag, buf);
      }
    }
  }
  else 
  {
#ifndef WIFI_SERIAL      /*终端节点*/
#ifdef CONFIG_SENSOR_NODE
      if(sensor_fun_p.z_process_command_call(ptag, pval, buf) > 0) {
#else
      if(z_process_command_call(ptag, pval, buf) > 0) {
#endif
      if(strlen(ptag) == 3) ptag += 1;
      zxbeeAdd(ptag, buf);
    }
#endif
  }
}

static void zxbee_decode(char *pkg, int len) {
  char *pnow = NULL;
  char *ptag = NULL, *pval = NULL;
  
  if (pkg[0] != '{' || pkg[len-1] != '}') return ;
  pkg[len-1] = 0;
  pnow = pkg+1; 
  do {
    ptag = pnow;
    pnow = (char *)strchr(pnow, '=');
    if (pnow != NULL) {
      *pnow++ = 0;
      pval = pnow;
      pnow = strchr(pnow, ',');
      if (pnow != NULL) *pnow++ = 0;
      sensor_process_tag(ptag, pval);
    }
  } while (pnow != NULL);
}

#ifndef USE_RTOS2_API
void zxbee_onrecv_fun(char *pkg, int len) {
  char *psnd;
  
  if(rf_zxbee_mutex==INVALID_MUTEX) return;

  if(LOS_MuxPend(rf_zxbee_mutex, LOS_MS2Tick(1000)) != LOS_OK) return;
  zxbeeBegin();
  zxbee_decode(pkg, len);
  psnd = zxbeeEnd();
  if(psnd != NULL){
    rf_send((uint8_t *)psnd, strlen(psnd), 200);
  }
  LOS_MuxPost(rf_zxbee_mutex);
}
#else
void zxbee_onrecv_fun(char *pkg, int len) {
  char *psnd;
  
  if(rf_zxbee_mutex==NULL) return;

  if(osMutexAcquire(rf_zxbee_mutex, LOS_MS2Tick(1000)) != LOS_OK) return;
  zxbeeBegin();
  zxbee_decode(pkg, len);
  psnd = zxbeeEnd();
  if(psnd != NULL){
    rf_send((uint8_t *)psnd, strlen(psnd), 200);
  }
  osMutexRelease(rf_zxbee_mutex);
}
#endif