#ifndef _FML_LED_H_
#define _FML_LED_H_

typedef enum {
  LED_NET=0, LED_DAT
} e_LED;

void led_on(e_LED led);
void led_off(e_LED led);
void led_toggle(e_LED led);
void led_Init(void);

#endif
