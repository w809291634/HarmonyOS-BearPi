/*********************************************************************************************
* 文件：sensor.c
* 作者：
* 说明：通用传感器控制接口程序
* 修改：wangh 2024.3.4
* 注释：
*********************************************************************************************/
#include "wifi_device.h"
#include "wifiiot_errno.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sensor.h"
#include <unistd.h>

#include "apl_rf/rf_thread.h"
#include "apl_rf/rf_if_hw.h"
#include "apl_rf/rf_if_app.h"
#include "apl_utils/apl_utils.h"

#include "drv_key/key.h"
#include "E53_IA1.h"

#define DBG_TAG           "sensor"
#define DBG_LVL           DBG_LOG
// #define DBG_LVL           DBG_INFO
//#define DBG_LVL           DBG_NODBG      // 所有信息都不显示
#include <mydbg.h>          // must after of DBG_LVL, DBG_TAG or other options

/*********************************************************************************************
* 外部变量
*********************************************************************************************/
#ifndef USE_RTOS2_API
extern UINT32 g_sensor_taskId;
extern UINT32 g_sensor_check_taskId;
#else
static uint8_t sensor_init_ok = 0;
#endif

/*********************************************************************************************
* 全局变量
*********************************************************************************************/
static uint8_t D0 = 0xFF;                                         // 默认打开主动上报功能
static uint8_t D1 = 0;                                            // 继电器初始状态为全关

static float A0 = 0.0;                                            // A0存储温度值
static float A1 = 0.0;                                            // A1存储湿度值
static uint16_t A2 = 0;                                           // A2存储光照度

static uint16_t V0 = 30;                                          // V0设置为上报时间间隔，默认为30s
static uint8_t V1 = 0;                                            // V1模式设置,手动：0(默认),自动：1

#ifndef USE_RTOS2_API
static UINT16 sensor_poll_timer=INVALID_TIMER;
static UINT32 sensor_poll_sem=INVALID_SEM;
UINT32 i2cbus_mutex=INVALID_MUTEX;                                /* I2C总线互斥量 */
#else
static osTimerId_t sensor_poll_timer = NULL;
static osSemaphoreId_t sensor_poll_sem = NULL;
static osMutexId_t i2cbus_mutex = NULL;                           /* I2C总线互斥量 */
#endif
static void sensor_poll_timer_cb(UINT32 parameter);

E53_IA1_Data_TypeDef E53_IA1_Data;
/*********************************************************************************************
* 名称：updateV0()
* 功能：更新V0的值
* 参数：pval:V0的值
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
static void updateV0(char *pval) {
  //将字符串变量val解析转换为整型变量赋值
  int value = atoi(pval); 
  if(value == 0 || value > 65535) return; //数据超出限制：不保存
  if(V0 != value) {
    V0 = value;
#ifndef USE_RTOS2_API
    //更新定时器间隔
    if(sensor_poll_timer!=INVALID_TIMER){
      LOS_SwtmrModify(sensor_poll_timer,LOS_MS2Tick(V0*1000),LOS_SWTMR_MODE_PERIOD, sensor_poll_timer_cb, 0);
    }
#else
    //更新定时器间隔
    if(sensor_poll_timer!=NULL){
      SWTMR_CTRL_S *pstSwtmr;
      pstSwtmr = (SWTMR_CTRL_S *)sensor_poll_timer;
      LOS_SwtmrModify(pstSwtmr->usTimerID,LOS_MS2Tick(V0*1000),LOS_SWTMR_MODE_PERIOD, sensor_poll_timer_cb, 0);
    }
#endif
  }
}

/*********************************************************************************************
* 名称：updateV1()
* 功能：更新V1的值
* 参数：pval:V1的值
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
static void updateV1(char *pval) {
  V1 = atoi(pval);
}

/*********************************************************************************************
* 名称：updateA0()
* 功能：更新A0的值
* 参数：无
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
static void updateA0(void) {
#ifndef USE_RTOS2_API
  if(i2cbus_mutex==INVALID_MUTEX) return;
  if(LOS_MuxPend(i2cbus_mutex, LOS_MS2Tick(1000)) != LOS_OK) return;
  // float temp,hum;
  // E53_IA1_Read_SHT3X(&temp,&hum);
  // A0 = temp; 
  E53_IA1_Read_Data();
  A0 = E53_IA1_Data.Temperature;
  LOS_MuxPost(i2cbus_mutex);
#else
  if(i2cbus_mutex==NULL) return;
  if(osMutexAcquire(i2cbus_mutex, LOS_MS2Tick(1000)) != LOS_OK) return;
  // float temp,hum;
  // E53_IA1_Read_SHT3X(&temp,&hum);
  // A0 = temp; 
  E53_IA1_Read_Data();
  A0 = E53_IA1_Data.Temperature;
  osMutexRelease(i2cbus_mutex);
#endif
}

/*********************************************************************************************
* 名称：updateA1()
* 功能：更新A1的值
* 参数：无
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
static void updateA1(void) {
#ifndef USE_RTOS2_API
  if(i2cbus_mutex==INVALID_MUTEX) return;
  if(LOS_MuxPend(i2cbus_mutex, LOS_MS2Tick(1000)) != LOS_OK) return;
  // float temp,hum;
  // E53_IA1_Read_SHT3X(&temp,&hum);
  // A1 = hum; 
  E53_IA1_Read_Data();
  A1 = E53_IA1_Data.Humidity;
  LOS_MuxPost(i2cbus_mutex);
#else
  if(i2cbus_mutex==NULL) return;
  if(osMutexAcquire(i2cbus_mutex, LOS_MS2Tick(1000)) != LOS_OK) return;
  // float temp,hum;
  // E53_IA1_Read_SHT3X(&temp,&hum);
  // A1 = hum; 
  E53_IA1_Read_Data();
  A1 = E53_IA1_Data.Humidity;
  osMutexRelease(i2cbus_mutex);
#endif
}

/*********************************************************************************************
* 名称：updateA2()
* 功能：更新A2的值
* 参数：无
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
static void updateA2(void) {
#ifndef USE_RTOS2_API
  if(i2cbus_mutex==INVALID_MUTEX) return;
  if(LOS_MuxPend(i2cbus_mutex, LOS_MS2Tick(1000)) != LOS_OK) return;
  // float hux = E53_IA1_Read_BH1750();
  // A2 = (uint16_t)hux; 
  E53_IA1_Read_Data();
  A2 = (uint16_t)E53_IA1_Data.Lux;
  LOS_MuxPost(i2cbus_mutex);
#else
  if(i2cbus_mutex==NULL) return;
  if(osMutexAcquire(i2cbus_mutex, LOS_MS2Tick(1000)) != LOS_OK) return;
  // float hux = E53_IA1_Read_BH1750();
  // A2 = (uint16_t)hux; 
  E53_IA1_Read_Data();
  A2 = (uint16_t)E53_IA1_Data.Lux;
  osMutexRelease(i2cbus_mutex);
#endif
}

/*********************************************************************************************
* 名称：sensor_init()
* 功能：传感器初始化
* 参数：无
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
void sensor_init(void) {
#ifndef USE_RTOS2_API
  /*******创建定时发送定时器，定时间隔为V0*****/
  if (LOS_SwtmrCreate(LOS_MS2Tick(V0*1000), LOS_SWTMR_MODE_PERIOD, 
        sensor_poll_timer_cb, &sensor_poll_timer, 0) != LOS_OK) 
    LOG_E("[%s] LOS_SwtmrCreate Failed!",__func__);

  if (LOS_SemCreate(0, &sensor_poll_sem) != LOS_OK) 
    LOG_E("[%s] LOS_SemCreate Failed!",__func__);

  LOS_MuxCreate(&i2cbus_mutex);
  if( i2cbus_mutex == INVALID_MUTEX ){
    LOG_E("[%s] LOS_MuxCreate fail,Mux1:%d",__func__,i2cbus_mutex);
  }
  /***************初始化各传感器***************/
  LOS_Msleep(500);                                      // 等待部分传感器上电
  /* 自行添加需要初始化的传感器 */
  key_init();                                           // 按键初始化
  E53_IA1_Init();

  if(sensor_poll_timer!=INVALID_TIMER && \
        LOS_SwtmrStart(sensor_poll_timer))              // 开启定时器，每隔V0主动上报一次数据
    LOG_E("[%s] LOS_SwtmrStart Failed!",__func__);
  if(g_sensor_check_taskId!=INVALID_TASK && \
        LOS_OK!=LOS_TaskResume(g_sensor_check_taskId))  // 存在安防类传感器时，开启安防类传感器线程
    LOG_E("[%s] LOS_TaskResume Failed!",__func__);
#else
  /*******创建定时发送定时器，定时间隔为V0*****/
  uint32_t ret = 1;

  uint32_t argument=0;
  sensor_poll_timer = osTimerNew((osTimerFunc_t)sensor_poll_timer_cb, 
                                  osTimerPeriodic, &argument, NULL);
  if (sensor_poll_timer == NULL){
    LOG_E("[%s] osTimerNew Failed!",__func__);
    ret = 0;
  }
  sensor_poll_sem = osSemaphoreNew(INT32_MAX, 0, NULL);
  if (sensor_poll_sem == NULL){
    LOG_E("[%s] osSemaphoreNew Failed!",__func__);
    ret = 0;
  }
  i2cbus_mutex = osMutexNew(NULL);
  if( i2cbus_mutex == NULL ){
    LOG_E("[%s] osMutexNew fail,Mux1:%p",__func__,i2cbus_mutex);
    ret = 0;
    return;
  }
  /***************初始化各传感器***************/
  usleep(500000);                                       // 等待部分传感器上电
  /* 自行添加需要初始化的传感器 */
  key_init();                                           // 按键初始化
  E53_IA1_Init();

  if(sensor_poll_timer!=NULL && \
        osTimerStart(sensor_poll_timer, \
            LOS_MS2Tick(V0*1000))){                     // 开启定时器，每隔V0主动上报一次数据
    LOG_E("[%s] osTimerStart Failed!",__func__);
    ret = 0;
  }

  sensor_init_ok = ret;
#endif
}

/*********************************************************************************************
* 名称：sensorLinkOn()
* 功能：传感器节点入网成功调用函数
* 参数：无
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
void sensorLinkOn(void) {
#ifndef USE_RTOS2_API
  if(sensor_poll_sem==INVALID_SEM)return;
  LOS_SemPost(sensor_poll_sem);         //发送信号，令sensorUpdate主动上报一次数据
#else
  if(sensor_poll_sem==NULL)return;
  osSemaphoreRelease(sensor_poll_sem);  //发送信号，令sensorUpdate主动上报一次数据
#endif
}

/*********************************************************************************************
* 名称：sensorUpdate()
* 功能：传感器数据更新
* 参数：无
* 返回：无
* 修改：
* 注释：查询各传感器值，达到V0设定时间时上报
*********************************************************************************************/
void sensorUpdate(void) {
  char data_send[100] = "{";
  char temp[16];
  
#ifndef USE_RTOS2_API
  if(sensor_poll_sem==INVALID_SEM) {
    LOS_Msleep(100);
    return;
  }

  UINT32 err = LOS_SemPend(sensor_poll_sem, LOS_MS2Tick(1000));
  if(err != LOS_OK) return;
#else
  if(sensor_poll_sem==NULL) {
    usleep(100000);
    return;
  }

  UINT32 err = osSemaphoreAcquire(sensor_poll_sem, LOS_MS2Tick(1000));
  if(err != osOK) return;
#endif

  // 根据D0的位状态判定需要主动上报的数值
  if ((D0 & 0x01) == 0x01){                                      
    updateA0();
    snprintf(temp,sizeof(temp), "A0=%.1f,", A0);
    strcat(data_send, temp);
  }
  if ((D0 & 0x02) == 0x02){                                     
    updateA1();
    snprintf(temp,sizeof(temp), "A1=%.1f,", A1);
    strcat(data_send, temp);
  }
  if ((D0 & 0x04) == 0x04){                                     
    updateA2();
    snprintf(temp,sizeof(temp), "A2=%u,", A2);
    strcat(data_send, temp);
  }

  snprintf(temp,sizeof(temp), "D1=%u,", D1);
  strcat(data_send, temp);
  
  if(strlen(data_send) > 2) {
    data_send[strlen(data_send)-1] = '}';
    rf_send((uint8_t *)data_send, strlen(data_send), 200);
  }
}
/*********************************************************************************************
* 名称：sensorCheck()
* 功能：传感器监测，安防类传感器异常时快速发送消息
* 参数：无
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
void sensorCheck(void) {
  char data_send[100] = "{";
  char temp[20];
  uint8_t send_flag = 0;

#ifdef USE_RTOS2_API
  if(!sensor_init_ok){
    usleep(100000);
    return;
  }
#endif
  /* 检查传感器值变化 */
  static char lastA0=0;
  static uint32_t ct0=0;

  // 发送数据
  if(strlen(data_send) > 2) {
    if(send_flag == 1) {
      data_send[strlen(data_send)-1] = '}';
      rf_send((uint8_t *)data_send, strlen(data_send), 200);
    }
  }
#ifndef USE_RTOS2_API
  LOS_Msleep(100);
#else
  usleep(100000);
#endif
}

/*********************************************************************************************
* 名称：sensorControl()
* 功能：传感器控制
* 参数：cmd - 控制命令
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
void sensorControl(uint8_t cmd) {
  // 根据cmd参数处理对应的控制程序
  uint32_t ct = LOS_Tick2MS((UINT32)LOS_TickCountGet());
  
  if(cmd & 0x01){ 
    Light_StatusSet(ON);
  }
  else{
    Light_StatusSet(OFF);
  }
  if(cmd & 0x02){ 
    Motor_StatusSet(ON);
  }
  else{
    Motor_StatusSet(OFF);
  }
}

/*********************************************************************************************
* 名称：z_process_command_call()
* 功能：处理上层应用发过来的指令
* 参数：ptag: 指令标识 D0，D1， A0 ...
*       pval: 指令值， “?”表示读取，
*       obuf: 指令处理结果存放地址
* 返回：>0指令处理结果返回数据长度，0：没有返回数据，<0：不支持指令。
* 修改：
* 注释：
*********************************************************************************************/
int z_process_command_call(char* ptag, char* pval, char* obuf) {
  int val;
  int ret = 0;	
  
  char *p = obuf;
  
  // 将字符串变量pval解析转换为整型变量赋值
  val = atoi(pval);	
  // 控制命令解析
  if (0 == strcmp("CD0", ptag)){                                // 对D0的位进行操作，CD0表示位清零操作
    D0 &= ~val;
  }
  if (0 == strcmp("OD0", ptag)){                                // 对D0的位进行操作，OD0表示位置一操作
    D0 |= val;
  }
  if (0 == strcmp("D0", ptag)){                                 // 查询上报使能编码
    if (0 == strcmp("?", pval)){
      ret = sprintf(p, "%u", D0);  
    } 
  }
  if (0 == strcmp("CD1", ptag)){                                // 对D1的位进行操作，CD1表示位清零操作
    D1 &= ~val;
    sensorControl(D1);                                          // 处理执行命令
  }
  if (0 == strcmp("OD1", ptag)){                                // 对D1的位进行操作，OD1表示位置一操作
    D1 |= val;
    sensorControl(D1);                                          // 处理执行命令
  }
  if (0 == strcmp("D1", ptag)){                                 // 查询执行器命令编码
    if (0 == strcmp("?", pval)){
      ret = sprintf(p, "%u", D1);
    } 
  }
  if (0 == strcmp("A0", ptag)){ 
    if (0 == strcmp("?", pval)){
      updateA0();
      ret = sprintf(p, "%.1f", A0);     
    } 
  }
  if (0 == strcmp("A1", ptag)){ 
    if (0 == strcmp("?", pval)){
      updateA1();
      ret = sprintf(p, "%.1f", A1);  
    } 
  }
  if (0 == strcmp("A2", ptag)){ 
    if (0 == strcmp("?", pval)){
      updateA2();
      ret = sprintf(p, "%u", A2);  
    } 
  }
 
  if (0 == strcmp("V0", ptag)){
    if (0 == strcmp("?", pval)){
      ret = sprintf(p, "%u", V0);                         	// 上报时间间隔
    }else{
      updateV0(pval);
    }
  }
  if (0 == strcmp("V1", ptag)){
    if (0 == strcmp("?", pval)){
      ret = sprintf(p, "%u", V1);                         	// 上报时间间隔
    }else{
      updateV1(pval);
    }
  }
  return ret;
}
  
/*********************************************************************************************
* 名称：sensor_poll_timer_cb()
* 功能：sensor_update 的触发更新函数
* 参数：parameter 回调函数参数
* 返回：无
* 修改：
* 注释：
*********************************************************************************************/
static void sensor_poll_timer_cb(UINT32 parameter) {
  //定时器达到V0时间间隔后，发送信号，让sensorUpdate函数发送数据
  sensorLinkOn();
}
