#ifndef __APL_UTILS_H__
#define __APL_UTILS_H__
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hos_types.h"
#include "los_task.h"
#include "los_task.h"
#include "los_mux.h"
#include "los_swtmr.h"
#include "los_sem.h"
#include "hi_types.h"
#include "hi_nvm.h"
#include "hi_nv.h"
#include "hi_efuse.h"

#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip4_addr.h"
#include "lwip/api_shell.h"

/*********************************************************************************************
* 配置
*********************************************************************************************/
//wifi配置(DEFAULT)
#define WIFI_DEFAULT_SSID             ""
#define WIFI_DEFAULT_PASSWORD         ""

//wifi智云IDK(DEFAULT)
#define ZHIYUN_DEFAULT_ID             ""
#define ZHIYUN_DEFAULT_KEY            ""

//wifi智云配置(可配置网关UDP IP PORT)
#define ZHIYUN_DEFAULT_IP           "api.zhiyun360.com"
#define ZHIYUN_DEFAULT_PORT         28082
#define DEFAULT_UDP_PORT            7003

/*********************************************************************************************
* 宏定义
*********************************************************************************************/
#define INVALID_TASK                ((UINT32)0xffffffff)
#define INVALID_MUTEX               ((UINT32)0xffffffff)
#define INVALID_SEM                 ((UINT32)0xffffffff)
#define INVALID_TIMER               ((UINT16)0xffff)
#define INVALID_SOCKET              (-1)

#define AT_MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define at_mac2str(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

//nv保存参数分区ID
#define NV_BASE_ID                  0x0B
#define NV_WIFI_ID                  0x0C
#define NV_ZHIYUN_ID                0x0D

//关于atan2相关宏
#define asuint(f) ((union{float _f; uint32_t _i;}){f})._i
#define asfloat(i) ((union{uint32_t _i; float _f;}){i})._f
#define asuint64(f) ((union{double _f; uint64_t _i;}){f})._i
#define asdouble(i) ((union{uint64_t _i; double _f;}){i})._f

#define EXTRACT_WORDS(hi,lo,d)                    \
do {                                              \
  uint64_t __u = asuint64(d);                     \
  (hi) = __u >> 32;                               \
  (lo) = (uint32_t)__u;                           \
} while (0)

#define USE_RTOS2_API
#define LOS_PRIORITY_WIN 8
#define OS_TASK_PRIORITY_LOWEST                     31
#define MY_AT_PRINT_ENABLE_HEX

//外部引用
double atan2(double y, double x);
UINT32 Microsecond(void);
UINT32 Millisecond(void);
struct timeval msec2timeval(UINT32 msec);
void read_sys_parameter(void);
void write_sys_parameter(void);
void rf_info_dump(void);
void at_print_raw_cmd_ex(uint8_t dir, const char *name, const char *buf, uint32_t size);
#endif
