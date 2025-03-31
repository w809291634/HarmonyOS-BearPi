#include "apl_delay/delay.h"
#include "apl_utils/apl_utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

void delay_ms(unsigned short times)
{
  LOS_Msleep(times);
}

void delay_us(unsigned short times)
{
  usleep(times);
}
