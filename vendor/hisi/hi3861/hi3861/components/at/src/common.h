#ifndef __COMMON_H__
#define __COMMON_H__
#include "hi_types.h"
#include "hi_types_base.h"
#include "apl_rf/rf_if_app.h"

extern t_rf_info rf_info;
extern int8_t g_rf_Link_status;
void read_sys_parameter(void);
void write_sys_parameter(void);
#endif
