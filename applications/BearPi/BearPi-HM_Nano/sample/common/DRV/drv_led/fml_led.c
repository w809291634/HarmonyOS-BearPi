#include "drv_led/fml_led.h"
#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"

/* led指示灯初始化 */
void led_Init(void)
{
  //初始化GPIO
  GpioInit();
  //设置GPIO_2的复用功能为普通GPIO
  IoSetFunc(WIFI_IOT_IO_NAME_GPIO_2, WIFI_IOT_IO_FUNC_GPIO_2_GPIO);
  //设置GPIO_2为输出模式
  GpioSetDir(WIFI_IOT_GPIO_IDX_2, WIFI_IOT_GPIO_DIR_OUT);

  led_off(LED_NET);
  led_off(LED_DAT);
}

void led_on(e_LED led) {
  switch(led) {
    case LED_NET: GpioSetOutputVal(WIFI_IOT_GPIO_IDX_2, 1); break;
    case LED_DAT: break;
  }
}

void led_off(e_LED led) {
  switch(led) {
    case LED_NET: GpioSetOutputVal(WIFI_IOT_GPIO_IDX_2, 0); break;
    case LED_DAT: break;
  }
}
