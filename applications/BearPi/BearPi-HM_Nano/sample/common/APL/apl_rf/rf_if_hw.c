#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "apl_rf/rf_if_hw.h"
#include "apl_rf/rf_if_app.h"

#include "wifi_device.h"
#include "wifiiot_errno.h"
#include "ohos_init.h"
#include "los_task.h"

#include <hi_wifi_api.h>
#include <hi_types_base.h>

#define DBG_TAG           "rf_if_hw"
// #define DBG_LVL           DBG_LOG
// #define DBG_LVL           DBG_INFO
#define DBG_LVL           DBG_WARNING
// #define DBG_LVL           DBG_NODBG      // 所有信息都不显示
#include <mydbg.h>          // must after of DBG_LVL, DBG_TAG or other options

// 连接wifi的标志位
#define SELECT_WLAN_PORT            "wlan0"

#define WIFI_SCAN_MAX_TIMES         2               // 扫描wifi重试次数
#define WIFI_SCAN_INTERVAL          ((UINT32)5000)  // 扫描间隔时间

#define WIFI_CONNECT_MAX_TIMES      3               // 一个配置的连接重试次数

#define WIFI_DHCP_MAX_TIMES         20              // DHCP获取配置次数

//外部引用变量
extern t_rf_info rf_info;                                          //通讯模块信息
extern int8_t g_rf_Link_status;
extern int sock_fd;

// 连接wifi的标志位
static int g_staScanSuccess = 0;
static int g_ConnectSuccess = 0;
static int g_ssid_count = 0;
static int g_Connect_count = 0;
int g_wifi_rssi = 0;                                // 信号强度

// 全局标志位
WifiEvent g_wifiEventHandler = {0};
WifiErrorCode error;
struct netif *g_lwip_netif = NULL;

static void OnWifiScanStateChangedHandler(int state, int size)
{
    (void)state;
    if (size > 0){
        g_ssid_count = size;
        g_staScanSuccess = 1;
    }
    return;
}

static void OnWifiConnectionChangedHandler(int state, WifiLinkedInfo *info)
{
    (void)info;
    
    if (state == WIFI_CONNECTED){
        g_ConnectSuccess = 1;
        g_Connect_count = 0;
        LOG_D("callback function for wifi connect");
    }else{
        g_Connect_count++;
        rf_set_link_status(0);
        LOG_W("WiFi connection disconnected");
    }
    return;
}

static void OnHotspotStaJoinHandler(StationInfo *info)
{
    (void)info;
    LOG_D("STA join AP");
    return;
}

static void OnHotspotStaLeaveHandler(StationInfo *info)
{
    (void)info;
    LOG_D("HotspotStaLeave:info is null.");
    return;
}

static void OnHotspotStateChangedHandler(int state)
{
    LOG_D("HotspotStateChanged:state is %d.", state);
    return;
}

static int WiFiInit(void)
{
    g_wifiEventHandler.OnWifiScanStateChanged = OnWifiScanStateChangedHandler;
    g_wifiEventHandler.OnWifiConnectionChanged = OnWifiConnectionChangedHandler;
    g_wifiEventHandler.OnHotspotStaJoin = OnHotspotStaJoinHandler;
    g_wifiEventHandler.OnHotspotStaLeave = OnHotspotStaLeaveHandler;
    g_wifiEventHandler.OnHotspotStateChanged = OnHotspotStateChangedHandler;
    error = RegisterWifiEvent(&g_wifiEventHandler);
    if (error != WIFI_SUCCESS) LOG_E( "register wifi event fail!");
    else LOG_D( "register wifi event succeed!");

    //使能WIFI
    if (EnableWifi() != WIFI_SUCCESS){
        LOG_E( "EnableWifi failed");
        return -1;
    }

    //判断WIFI是否激活
    if (IsWifiActive() == 0){
        LOG_E( "Wifi station is not actived.");
        return -1;
    }
    return 0;
}

// 返回：
// 0: 连接热点正常
//-1: 没有找到指定wifi
//-2: 连接wifi失败
INT8 Wifi_Connecting_hotspots(void)
{
    #define LEVEL   1
    #define STEPS   4
    int steps=0;
    int FoundExpectWifi=0;
    WifiScanInfo *info = NULL;
    unsigned int size = WIFI_SCAN_HOTSPOT_LIMIT;
    WifiDeviceConfig select_ap_config = {0};

    //等待1S
    LOS_Msleep(1000);

    //初始化WIFI
    if(WiFiInit()!=0){
        LOG_E( "WiFiInit fail");
        return -1;
    }
    steps++;
    LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);

    //分配空间，保存WiFi信息
    info = malloc(sizeof(WifiScanInfo) * WIFI_SCAN_HOTSPOT_LIMIT);
    if (info == NULL){
        return -1;
    }

    /* 轮询查找WiFi列表,寻找需要的网络名 */
    int scan_times=0;
    do{
        //重置标志位
        size = WIFI_SCAN_HOTSPOT_LIMIT;
        g_ssid_count = 0;
        g_staScanSuccess = 0;

        //开始扫描
        Scan();

        //等待扫描结果
        while(g_staScanSuccess != 1){
            LOS_Msleep(100);
        }

        //获取扫描列表
        error = GetScanInfoList(info, &size);
        if(error!=WIFI_SUCCESS) {
            LOG_E("Failed to GetScanInfoList!");
            continue;
        }

        //查找是否存在指定的wifi ssid
        for(uint8_t i = 0; i < g_ssid_count; i++){
            if (strcmp(rf_info.wifi.wifi_ssid, info[i].ssid) == 0 && strlen(info[i].ssid)>0 ){
                FoundExpectWifi = 1;
                LOG_I("Search for specified wifi Succeed");
                
                steps++;
                LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);
                break;
            }
        }
        if(FoundExpectWifi) break;                  //找到特定wifi,进行下一步

        scan_times++;
        LOG_W("Failed to Search for specified wifi,times:%d,max_times:%d",scan_times,WIFI_SCAN_MAX_TIMES);
        if(scan_times < WIFI_SCAN_MAX_TIMES) LOS_Msleep(WIFI_SCAN_INTERVAL);
    }while(scan_times < WIFI_SCAN_MAX_TIMES);

    /* 打印WiFi列表 */
    LOG_D("********************");
    for(uint8_t i = 0; i < g_ssid_count; i++)
    {
        LOG_D("no:%03d, ssid:%-30s, rssi:%5d", i+1, info[i].ssid, info[i].rssi/100);
    }
    LOG_D("********************");

    //没有查找到需要的wifi
    if(FoundExpectWifi!=1) {
        LOG_E("Failed to Search for specified wifi!");
        free(info);
        if(WIFI_SCAN_INTERVAL-1000>0)
          LOS_Msleep(WIFI_SCAN_INTERVAL-1000);
        return -1;
    }
    
    /* 连接指定的WiFi热点 */
    for(uint8_t i = 0; i < g_ssid_count; i++){
        if (strcmp(rf_info.wifi.wifi_ssid, info[i].ssid) == 0){
            // 复位变量值
            int result;
            g_ConnectSuccess = 0;
            g_Connect_count = 0;

            LOG_D("Select:%3d wireless, Waiting...", i+1);

            //拷贝要连接的热点信息
            strcpy(select_ap_config.ssid, rf_info.wifi.wifi_ssid);
            strcpy(select_ap_config.preSharedKey, rf_info.wifi.wifi_key);
            select_ap_config.securityType = WIFI_SEC_TYPE_PSK;

            //添加配置并连接
            if (AddDeviceConfig(&select_ap_config, &result) == WIFI_SUCCESS){
                if (ConnectTo(result) == WIFI_SUCCESS){
                    // 等待连接结果和重试
                    while (g_Connect_count < WIFI_CONNECT_MAX_TIMES)
                    {
                        LOS_Msleep(100);
                        if(g_ConnectSuccess) break;
                    }

                    if(g_ConnectSuccess){
                        // WIFI 连接成功
                        LOG_I("WiFi connect succeed!");
                        g_lwip_netif = netifapi_netif_find(SELECT_WLAN_PORT);

                        steps++;
                        LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);

                        g_wifi_rssi = info[i].rssi/100;
                        break;
                    }else{
                        // WIFI 连接失败,移除当前配置
                        error = RemoveDevice(result);
                        if(error!=WIFI_SUCCESS){
                            LOG_E("WiFi Remove Device Config Fail!");
                        }
                    }
                }
            }
        }
    }
    //连接失败
    if(g_ConnectSuccess!=1 || g_lwip_netif==NULL){
        Disconnect();
        LOG_E("Connection to WiFi failed");
        free(info);
        return -2;
    }

    /* 启动DHCP */
    if (g_lwip_netif){
        if(ERR_OK!=dhcp_start(g_lwip_netif)){
          LOG_E("[%s] dhcp_start error",__func__);
          free(info);
          return -3;
        }
        LOG_D("begain to dhcp");
    }

    //等待DHCP
    int dhcp_cnts=0;
    for(;;){
        if(dhcp_is_bound(g_lwip_netif) == ERR_OK){
            LOG_D("DHCP state:OK");
            
            ip4_addr_t ipaddr;
            ip4_addr_t netmask;
            ip4_addr_t gw;
            char ip[40];
            err_t err;
            err = netifapi_netif_get_addr(g_lwip_netif,&ipaddr,&netmask,&gw);
            if (err == ERR_OK) {
                char *str = ip4addr_ntoa_r(&ipaddr,ip,sizeof(ip));
                if (str != NULL) {
                    LOG_I("[%s] ipaddress:%s",__func__,ip);
                } 
            }else LOG_E("Error: Failed to get IP address.!");

            steps++;
            LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);

            #if (DBG_LVL >= DBG_LOG) && NODBG!=1
                //打印获取到的IP信息
                netifapi_netif_common(g_lwip_netif, dhcp_clients_info_show, NULL);
            #endif
            break;
        }

        LOG_D("DHCP state:Inprogress");
        LOS_Msleep(1000);
        if(++dhcp_cnts >= WIFI_DHCP_MAX_TIMES){
            LOG_E("DHCP state:error!");
            free(info);
            return -4;
        }
    }
    free(info);
    return 0;
}

// 清理wifi连接的所有配置项
INT8 Wifi_RemoveAllDevice(void)
{
    for(int i=0;i<WIFI_MAX_CONFIG_SIZE;i++){
        if(RemoveDevice(i)!=WIFI_SUCCESS){
            LOG_E( "Wifi_RemoveAllDevice failed");
            return ERROR_WIFI_UNKNOWN;
        }
    }
    return WIFI_SUCCESS;
}

// wifi断开热点链接
INT8 Wifi_DisConnect_hotspots(void)
{
    //断开连接
    if(Disconnect()!=WIFI_SUCCESS){
        LOG_E( "Disconnect failed");
        return -1;
    }
    
    //断开wifi
    if (IsWifiActive()==WIFI_STA_ACTIVE){
        if( DisableWifi()!=WIFI_SUCCESS || 
            UnRegisterWifiEvent(&g_wifiEventHandler)!=WIFI_SUCCESS ||
            Wifi_RemoveAllDevice()!=WIFI_SUCCESS ){
            LOG_E( "DisableWifi failed");
            return -2;
        }
    }
    return WIFI_SUCCESS;
}

/* 获取硬件mac地址 */
void rf_if_get_mac(void) 
{
  hi_uchar mac_addr[6] = {0}; /* 6 mac len */
  char mac[40] = {0};

  if (hi_wifi_get_macaddr((hi_char*)mac_addr, 6) != HI_ERR_SUCCESS) { /* 6 mac len */
    LOG_E("hi_wifi_get_macaddr error");
    return ;
  }
  snprintf(mac,40,"WIFI:"AT_MACSTR, at_mac2str(mac_addr));
  if(strcmp(rf_info.wifi.mac, mac) != 0) {
    strcpy(rf_info.wifi.mac, mac);
    write_sys_parameter();
  }
}

/* wifi初始化MAC地址,原始MAC地址随机 */
void rf_if_init_mac(void)
{
  hi_u32 die2_bits_size = hi_efuse_get_id_size(HI_EFUSE_DIE_2_RW_ID);
  hi_u8 die2_size = die2_bits_size/8 + ((die2_bits_size%8)?1:0);
  // LOG_D("die2 bits:%d bytes:%d\r\n",die2_bits_size,die2_size);
  hi_u8 *p = malloc(die2_size);

  if(hi_efuse_read(HI_EFUSE_DIE_2_RW_ID, p, die2_size)==HI_ERR_SUCCESS){
    hi_uchar mac_addr[6]; /* 6 mac len */
    memcpy(mac_addr,p,6);
    mac_addr[0]=0;
    if (hi_wifi_set_macaddr((hi_char*)mac_addr, 6) != 0) { /* 6 mac len */
      printf("[%s] hi_wifi_set_macaddr fail\r\n",__func__);
      while(1);
    }
  }else{
    printf("[%s] hi_efuse_read fail\r\n",__func__);
    while(1);
  }
  free(p);
}