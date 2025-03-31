#ifndef __RF_THREAD_H_
#define __RF_THREAD_H_
#include "apl_rf/rf_if_hw.h"
#include "apl_utils/apl_utils.h"

#define RT_RF_LINK_THREAD_STACK_SIZE   4640                 /* 线程栈大小 */
#define RT_RF_LINK_THREAD_PRIORITY     17                   /* 线程优先级 */

#define RT_RF_THREAD_STACK_SIZE   3072                      /* 线程栈大小 */
#define RT_RF_THREAD_PRIORITY     18                        /* 线程优先级 */

void Wifi_task_init(void);

#endif // __RF_THREAD_H_