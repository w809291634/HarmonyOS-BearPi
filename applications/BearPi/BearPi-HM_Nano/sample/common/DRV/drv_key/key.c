/*********************************************************************************************
*文件: key.c
*作者: zonesion   2015.12.18
*说明: 按键处理程序
*修改:
*注释:
*********************************************************************************************/
#include "drv_key/key.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"

void key_init(void)
{
  GpioInit();

  //初始化F1按键
  IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_GPIO);
  GpioSetDir(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_GPIO_DIR_IN);
  IoSetPull(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_PULL_UP);

  //初始化F2按键
  IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_GPIO);
  GpioSetDir(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_GPIO_DIR_IN);
  IoSetPull(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_PULL_UP);
}

unsigned char get_key_status(void)
{
  unsigned char key_status = 0;
  WifiIotGpioValue key1;
  WifiIotGpioValue key2;
  GpioGetInputVal(WIFI_IOT_IO_NAME_GPIO_11, &key1);
  GpioGetInputVal(WIFI_IOT_IO_NAME_GPIO_12, &key2);

  if(key1 == 0){
    key_status |= 0x01;
  }
  else{
    key_status &= ~0x01;
  }
  if(key2 == 0){
    key_status |= 0x02;
  }
  else{
    key_status &= ~0x02;
  }
  return key_status;
}