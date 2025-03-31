#ifndef __RF_IF_HW_H__
#define __RF_IF_HW_H__
#include <stdint.h>
#include "apl_utils/apl_utils.h"

INT8 Wifi_Connecting_hotspots(void);
INT8 Wifi_DisConnect_hotspots(void);
void rf_if_get_mac(void);
void rf_if_init_mac(void);
#endif  //__RF_IF_APP_H__
