#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "apl_rf/rf_thread.h"
#include "apl_rf/rf_if_hw.h"
#include "apl_rf/rf_if_app.h"

#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip4_addr.h"
#include "lwip/api_shell.h"

#include "wifi_device.h"
#include "wifiiot_errno.h"
#include "drv_led/fml_led.h"

#define DBG_TAG           "rf_thread"
//#define DBG_LVL           DBG_LOG
#define DBG_LVL           DBG_INFO
//#define DBG_LVL           DBG_NODBG      // 所有信息都不显示
#include <mydbg.h>          // must after of DBG_LVL, DBG_TAG or other options

//外部引用变量
extern int8_t g_rf_Link_status;
extern int8_t g_led_Link_status;
extern int sock_fd;

//全局变量
#ifndef USE_RTOS2_API
UINT32 rf_zxbee_mutex=INVALID_MUTEX;                            /* zxbee处理锁 */
UINT32 rf_send_mutex=INVALID_MUTEX;                             /* 无线发送锁 */
UINT8 g_LedEvent;                                               /* led事件 */
UINT32 g_link_taskId=0;
UINT32 g_recv_taskId=0;
UINT16 timer1_id;                                               // 定时器id1
#else
osMutexId_t rf_zxbee_mutex=NULL;                                /* zxbee处理锁 */
osMutexId_t rf_send_mutex=NULL;                                 /* 无线发送锁 */
UINT8 g_LedEvent;                                               /* led事件 */
osTimerId_t timer1_id;                                          // 定时器id1
#endif
char tcp_client_recvBuf[TCP_CLIENT_RECV_SIZE];                  // 接收数据缓存

static void rf_link_timer_cb(UINT32 parameter) {
  (void)parameter;
  static u_int8_t count=0;
  static uint8_t led_flag = 0;
  UINT32 event;

  switch(g_rf_Link_status) {
    case 0: led_flag = 0; break;
    case 1:
      count++;
      if(count>5){
        count=0;
        led_flag = !led_flag;
      }
    break;
    case 2: 
      count++;
      if(count>1){
        count=0;
        led_flag = !led_flag;
      }
    break;
    case 3: led_flag = 1; break;
    default:break;
  }

  if(g_LedEvent) {
#ifndef USE_RTOS2_API
    LOS_TaskLock();
    led_flag=0;
    g_LedEvent=0;
    LOS_TaskUnlock();
#else
    osKernelLock();
    led_flag=0;
    g_LedEvent=0;
    osKernelUnlock();
#endif
  }

  if(led_flag) led_on(LED_NET);
  else led_off(LED_NET);
}

// rf 连接智云的线程，处理连接
static void rf_link_thread_entry(void *parameter) {
    (void)parameter;
    while(1) {
        if(rf_if_Link()!=0) break;
    }
}

static void rf_recv_thread_entry(void *parameter) {
    while(1) {
        if(rf_if_recv()!=0) break;
    }
}

#ifndef USE_RTOS2_API
void Wifi_task_init(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskParam1 = { 0 };
    TSK_INIT_PARAM_S taskParam2 = { 0 };

    /* 创建互斥锁 */
    LOS_MuxCreate(&rf_zxbee_mutex);
    LOS_MuxCreate(&rf_send_mutex);
    if( rf_zxbee_mutex == INVALID_MUTEX ||
        rf_send_mutex == INVALID_MUTEX ){
        LOG_E("[%s] LOS_MuxCreate fail,Mux1:%d,Mux2:%d",__func__,rf_zxbee_mutex,rf_send_mutex);
        return;
    }

    /* 创建软件定时器 */
    LOS_SwtmrCreate(LOS_MS2Tick(100), LOS_SWTMR_MODE_PERIOD, rf_link_timer_cb, &timer1_id, 0);
    ret = LOS_SwtmrStart(timer1_id);
    if (ret != LOS_OK) {
        LOS_TaskUnlock();
        LOG_E("[%s] timer1_id Start Failed!",__func__);
        return;
    }

    /* 创建线程 */
    LOS_TaskLock();
    // rf_link线程
    taskParam1.pfnTaskEntry = (TSK_ENTRY_FUNC)rf_link_thread_entry;
    taskParam1.usTaskPrio = RT_RF_LINK_THREAD_PRIORITY;
    taskParam1.pcName = "rf_link_task";
    taskParam1.uwStackSize = RT_RF_LINK_THREAD_STACK_SIZE;
    taskParam1.uwResved = LOS_TASK_STATUS_DETACHED; /* detach 属性 */
    ret = LOS_TaskCreate(&g_link_taskId, &taskParam1);
    if (ret != LOS_OK) {
        LOS_TaskUnlock();
        LOG_E("g_link_taskId create Failed!");
        return;
    }
    LOG_D("g_link_taskId create Success!");

    // rf_接收数据线程
    taskParam2.pfnTaskEntry = (TSK_ENTRY_FUNC)rf_recv_thread_entry;
    taskParam2.usTaskPrio = RT_RF_THREAD_PRIORITY;
    taskParam2.pcName = "rf_recv_task";
    taskParam2.uwStackSize = RT_RF_THREAD_STACK_SIZE;
    taskParam2.uwResved = LOS_TASK_STATUS_DETACHED; /* detach 属性 */
    ret = LOS_TaskCreate(&g_recv_taskId, &taskParam2);
    if (ret != LOS_OK) {
        LOS_TaskUnlock();
        LOG_E("g_recv_taskId create Failed!");
        return;
    }
    LOG_D("g_recv_taskId create Success!");

    LOS_TaskUnlock();
}
#else
void Wifi_task_init(void)
{
    UINT32 ret;
    osThreadAttr_t attr;

    /* 创建互斥锁 */
    rf_zxbee_mutex = osMutexNew(NULL);
    rf_send_mutex = osMutexNew(NULL);
    if( rf_zxbee_mutex == NULL ||
        rf_send_mutex == NULL ){
        LOG_E("[%s] osMutexNew fail,Mux1:%p,Mux2:%p",__func__,rf_zxbee_mutex,rf_send_mutex);
        return;
    }

    /* 创建软件定时器 */
    uint32_t argument=0;
    timer1_id = osTimerNew((osTimerFunc_t)rf_link_timer_cb, 
                                    osTimerPeriodic, &argument, NULL);
    if (timer1_id == NULL){
        LOG_E("[%s] osTimerNew Failed!",__func__);
        return;
    }

    if(timer1_id != NULL && \
          osTimerStart(timer1_id,LOS_MS2Tick(100)))
      LOG_E("[%s] osTimerStart Failed!",__func__);

    /* 创建线程 */
    osKernelLock();
    // rf_link线程
    attr.name = "rf_link_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = RT_RF_LINK_THREAD_STACK_SIZE;
    attr.priority = OS_TASK_PRIORITY_LOWEST - RT_RF_LINK_THREAD_PRIORITY + LOS_PRIORITY_WIN;
    if (osThreadNew((osThreadFunc_t)rf_link_thread_entry, NULL, &attr) == NULL)
    {
        osKernelUnlock();
        LOG_E("g_link_taskId create Failed!");
        return;
    }
    LOG_D("g_link_taskId create Success!");

    // rf_接收数据线程
    attr.name = "rf_recv_task";
    attr.stack_size = RT_RF_THREAD_STACK_SIZE;
    attr.priority = OS_TASK_PRIORITY_LOWEST - RT_RF_THREAD_PRIORITY + LOS_PRIORITY_WIN;
    if (osThreadNew((osThreadFunc_t)rf_recv_thread_entry, NULL, &attr) == NULL)
    {
        osKernelUnlock();
        LOG_E("g_recv_taskId create Failed!");
        return;
    }
    LOG_D("g_recv_taskId create Success!");

    osKernelUnlock();
}
#endif