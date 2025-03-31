#ifndef __SENSOR_THREAD_H_
#define __SENSOR_THREAD_H_
#include "apl_utils/apl_utils.h"

#define RT_SENSOR_THREAD_STACK_SIZE   3072  /* 线程栈大小 */
#define RT_SENSOR_THREAD_PRIORITY     15     /* 线程优先级 */

#define RT_SENSOR_CHECK_THREAD_STACK_SIZE   3072  /* 线程栈大小 */
#define RT_SENSOR_CHECK_THREAD_PRIORITY     16     /* 线程优先级 */

void sensor_thread_init(void);
#endif // __SENSOR_THREAD_H_