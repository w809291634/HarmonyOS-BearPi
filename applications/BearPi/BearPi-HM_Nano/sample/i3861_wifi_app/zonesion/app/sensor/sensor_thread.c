#include "hos_types.h"
#include "wifi_device.h"
#include "wifiiot_errno.h"

#include "sensor_thread.h"
#include "sensor.h"

#define DBG_TAG           "sensor_thread"
//#define DBG_LVL           DBG_LOG
#define DBG_LVL           DBG_INFO
//#define DBG_LVL           DBG_NODBG      // 所有信息都不显示
#include <mydbg.h>          // must after of DBG_LVL, DBG_TAG or other options

#ifndef USE_RTOS2_API
UINT32 g_sensor_taskId=INVALID_TASK;
UINT32 g_sensor_check_taskId=INVALID_TASK;
#endif

static void sensor_thread_entry(void *parameter) {
  sensor_init();
  while(1) {
    sensorUpdate();
  }
}

static void sensor_check_thread_entry(void *parameter) {
  while(1) {
    sensorCheck();
  }
}

#ifndef USE_RTOS2_API
void sensor_thread_init(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskParam1 = { 0 };
    TSK_INIT_PARAM_S taskParam2 = { 0 };

    /* 创建线程 */
    LOS_TaskLock();
    // sensor_check_thread,仅创建,处于挂起状态
    taskParam1.pfnTaskEntry = (TSK_ENTRY_FUNC)sensor_check_thread_entry;
    taskParam1.usTaskPrio = RT_SENSOR_CHECK_THREAD_PRIORITY;
    taskParam1.pcName = "sensor_check";
    taskParam1.uwStackSize = RT_SENSOR_CHECK_THREAD_STACK_SIZE;
    taskParam1.uwResved = LOS_TASK_STATUS_DETACHED; /* detach 属性 */
    ret = LOS_TaskCreateOnly(&g_sensor_check_taskId, &taskParam1);
    if (ret != LOS_OK) {
        LOS_TaskUnlock();
        LOG_E("g_sensor_check_taskId create Failed!");
        return;
    }
    LOG_D("g_sensor_check_taskId create Success!");

    // sensor_thread
    taskParam2.pfnTaskEntry = (TSK_ENTRY_FUNC)sensor_thread_entry;
    taskParam2.usTaskPrio = RT_SENSOR_THREAD_PRIORITY;
    taskParam2.pcName = "sensor_task";
    taskParam2.uwStackSize = RT_SENSOR_THREAD_STACK_SIZE;
    taskParam2.uwResved = LOS_TASK_STATUS_DETACHED; /* detach 属性 */
    ret = LOS_TaskCreate(&g_sensor_taskId, &taskParam2);
    if (ret != LOS_OK) {
        LOS_TaskUnlock();
        LOG_E("g_sensor_taskId create Failed!");
        return;
    }
    LOG_D("g_sensor_taskId create Success!");
    
    LOS_TaskUnlock();
}
#else
void sensor_thread_init(void)
{
    osThreadAttr_t attr;

    osKernelLock();
    attr.name = "sensor_check";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = RT_SENSOR_CHECK_THREAD_STACK_SIZE;
    attr.priority = OS_TASK_PRIORITY_LOWEST - RT_SENSOR_CHECK_THREAD_PRIORITY + LOS_PRIORITY_WIN;
    if (osThreadNew((osThreadFunc_t)sensor_check_thread_entry, NULL, &attr) == NULL)
    {
        osKernelUnlock();
        LOG_E("g_sensor_check_taskId create Failed!");
        return;
    }
    LOG_D("g_sensor_check_taskId create Success!");

    attr.name = "sensor_task";
    attr.stack_size = RT_SENSOR_THREAD_STACK_SIZE;
    attr.priority = OS_TASK_PRIORITY_LOWEST - RT_SENSOR_THREAD_PRIORITY + LOS_PRIORITY_WIN;
    if (osThreadNew((osThreadFunc_t)sensor_thread_entry, NULL, &attr) == NULL)
    {
        osKernelUnlock();
        LOG_E("g_sensor_taskId create Failed!");
        return;
    }
    LOG_D("g_sensor_taskId create Success!");

    osKernelUnlock();
}
#endif
