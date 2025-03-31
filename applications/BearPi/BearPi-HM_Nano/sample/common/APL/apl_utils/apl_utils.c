#include "apl_utils/apl_utils.h"
#include "apl_rf/rf_if_app.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

#include "cmsis_os2.h"
#include "hos_types.h"
#include "ohos_init.h"
#include "los_task.h"

#define DBG_TAG           "apl_utils"
// #define DBG_LVL           DBG_LOG
//#define DBG_LVL           DBG_INFO
#define DBG_LVL           DBG_WARNING
//#define DBG_LVL           DBG_NODBG      // 所有信息都不显示
#include <mydbg.h>          // must after of DBG_LVL, DBG_TAG or other options

static const double
pi     = 3.1415926535897931160E+00, /* 0x400921FB, 0x54442D18 */
pi_lo  = 1.2246467991473531772E-16; /* 0x3CA1A626, 0x33145C07 */

double atan2(double y, double x)
{
	double z;
	uint32_t m,lx,ly,ix,iy;

	if (isnan(x) || isnan(y))
		return x+y;
	EXTRACT_WORDS(ix, lx, x);
	EXTRACT_WORDS(iy, ly, y);
	if (((ix-0x3ff00000) | lx) == 0)  /* x = 1.0 */
		return atan(y);
	m = ((iy>>31)&1) | ((ix>>30)&2);  /* 2*sign(x)+sign(y) */
	ix = ix & 0x7fffffff;
	iy = iy & 0x7fffffff;

	/* when y = 0 */
	if ((iy|ly) == 0) {
		switch(m) {
		case 0:
		case 1: return y;   /* atan(+-0,+anything)=+-0 */
		case 2: return  pi; /* atan(+0,-anything) = pi */
		case 3: return -pi; /* atan(-0,-anything) =-pi */
		}
	}
	/* when x = 0 */
	if ((ix|lx) == 0)
		return m&1 ? -pi/2 : pi/2;
	/* when x is INF */
	if (ix == 0x7ff00000) {
		if (iy == 0x7ff00000) {
			switch(m) {
			case 0: return  pi/4;   /* atan(+INF,+INF) */
			case 1: return -pi/4;   /* atan(-INF,+INF) */
			case 2: return  3*pi/4; /* atan(+INF,-INF) */
			case 3: return -3*pi/4; /* atan(-INF,-INF) */
			}
		} else {
			switch(m) {
			case 0: return  0.0; /* atan(+...,+INF) */
			case 1: return -0.0; /* atan(-...,+INF) */
			case 2: return  pi;  /* atan(+...,-INF) */
			case 3: return -pi;  /* atan(-...,-INF) */
			}
		}
	}
	/* |y/x| > 0x1p64 */
	if (ix+(64<<20) < iy || iy == 0x7ff00000)
		return m&1 ? -pi/2 : pi/2;

	/* z = atan(|y/x|) without spurious underflow */
	if ((m&2) && iy+(64<<20) < ix)  /* |y/x| < 0x1p-64, x<0 */
		z = 0;
	else
		z = atan(fabs(y/x));
	switch (m) {
	case 0: return z;              /* atan(+,+) */
	case 1: return -z;             /* atan(-,+) */
	case 2: return pi - (z-pi_lo); /* atan(+,-) */
	default: /* case 3 */
		return (z-pi_lo) - pi; /* atan(-,-) */
	}
}

static int GetCurrentTime(struct timeval* now)
{
    unsigned int tickPerSec = 0;
    unsigned int sysPerSec = 0;
    unsigned int sysPerTick = 0;
    if (now == NULL) {
        return -1;
    }
    tickPerSec = osKernelGetTickFreq();
    sysPerSec = osKernelGetSysTimerFreq();
    sysPerTick = sysPerSec / tickPerSec;

    unsigned int tickCount = osKernelGetTickCount();
    now->tv_sec = tickCount / tickPerSec;
    now->tv_usec = (tickCount % tickPerSec) * (1000*1000 / tickPerSec);

    unsigned int sysCount = osKernelGetSysTimerCount() % sysPerTick;
    now->tv_usec += sysCount * 1000*1000 / sysPerSec;
    return 0;
}

//当前系统经过的时间，单位us
UINT32 Microsecond(void)
{
    struct timeval now;
    GetCurrentTime(&now);
    // LOG_D("s:%ld us:%ld",now.tv_sec,now.tv_usec);

    UINT32 us = (UINT32)now.tv_sec*1000*1000+now.tv_usec;
    return us;
}

//当前系统经过的时间，单位ms
UINT32 Millisecond(void)
{
    UINT32 ms = (UINT32)Microsecond()/1000;
    return ms;
}

// 毫秒转换timeval
struct timeval msec2timeval(UINT32 msec)
{
    struct timeval tvp;
    tvp.tv_sec = msec / 1000;
    tvp.tv_usec = (msec % 1000) * 1000;
    return tvp;
}

void rf_info_dump(void)
{
  LOG_D("*************");
  LOG_D("MAC:%s",rf_info.wifi.mac);
  LOG_D("ssid:%s",rf_info.wifi.wifi_ssid);
  LOG_D("ssid_key:%s",rf_info.wifi.wifi_key);
  LOG_D("zhiyun_id:%s",rf_info.zhiyun.zhiyun_id);
  LOG_D("zhiyun_key:%s",rf_info.zhiyun.zhiyun_key);
  LOG_D("*************");
}

// at_print_raw_cmd的扩展版本
void at_print_raw_cmd_ex(uint8_t dir, const char *name, const char *buf, uint32_t size)
{
#define __is_print(ch)       ((unsigned int)((ch) - ' ') < 127u - ' ')
#define WIDTH_SIZE           32

    uint32_t i, j;
    if(!size) return;
      
    if(dir==0){
      /* 发送 */
      log_raw("%s <-- ",name);
    }else{
      /* 接收 */
      log_raw("%s --> ",name);
    }

#ifdef MY_AT_PRINT_ENABLE_HEX
    for (i = 0; i < size; i += WIDTH_SIZE)
    {
        /* 打印一行，一行打印 WIDTH_SIZE 个字节 */
        log_raw("\r\n  [hex]%04X-%04X: ", i, i + WIDTH_SIZE);
        /* 打印HEX格式 */
        for (j = 0; j < WIDTH_SIZE; j++)
        {
            if (i + j < size)
            {
                log_raw("%02X ", (unsigned char)buf[i + j]);    // 有数据打印出来
            }
            else
            {
                log_raw("   ");    // 没有数据打印空格
            }
            if ((j + 1) % 8 == 0)
            {
                log_raw(" ");   // 每 8 字节 多空一格
            }
        }
        log_raw("\r\n  [dec]%04d-%04d: ", i, i + WIDTH_SIZE);
        /* 打印dec格式，字符串格式 */
        for (j = 0; j < WIDTH_SIZE; j++)
        {
            if (i + j < size)
            {
                log_raw("%c", __is_print(buf[i + j]) ? buf[i + j] : '.');   // 是字符打印字符，不是打印空格
            }
        }
        log_raw("\r\n");
    }
#else
    for (i = 0; i < size; i ++)
    {
        /* 打印dec格式，字符串格式 */
        log_raw("%c", __is_print(buf[i]) ? buf[i] : '.');   // 是字符打印字符，不是打印空格
    }
    log_raw("\r\n");
#endif
}