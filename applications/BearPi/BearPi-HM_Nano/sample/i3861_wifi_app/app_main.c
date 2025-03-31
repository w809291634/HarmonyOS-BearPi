#include "apl_rf/rf_thread.h"
#include "apl_rf/rf_if_app.h"
#include "sensor_thread.h"

static void application_mian(void)
{
  rf_app_init();
  Wifi_task_init();
  sensor_thread_init();
}
APP_FEATURE_INIT(application_mian);
