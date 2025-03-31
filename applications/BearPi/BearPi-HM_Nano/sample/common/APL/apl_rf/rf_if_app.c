#include "apl_rf/rf_if_hw.h"
#include "apl_rf/rf_if_app.h"
#include "apl_utils/apl_utils.h"
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

#include "wifi_device.h"
#include "lwip/netifapi.h"
#include "lwip/api_shell.h"
#include "lwip/sockets.h"

#include "apl_rf/zxbee/zxbee.h"
#include "drv_led/fml_led.h"
#include "sensor.h"
#include <hi_at.h>

#define DBG_TAG           "rf_if_app"
// #define DBG_LVL           DBG_LOG
// #define DBG_LVL           DBG_INFO
#define DBG_LVL           DBG_WARNING
//#define DBG_LVL           DBG_NODBG      // 所有信息都不显示
#include <mydbg.h>          // must after of DBG_LVL, DBG_TAG or other options

//定义
#define MAX_RECONNECTION_CNT        ((uint16_t)0xffff)      //断开重连尝试次数
#define MAX_AUTHE_RETRY_CNT         3                       //认证
#define MAX_HEART_RETRY_CNT         3                       //心跳

//外部引用变量
#ifndef USE_RTOS2_API
extern UINT32 rf_send_mutex;                                /* 无线发送锁 */
#else
extern osMutexId_t rf_send_mutex;                           /* 无线发送锁 */
#endif
extern UINT8 g_LedEvent;                                    /* led事件 */
extern int g_wifi_rssi;

//全局变量
static int sock_fd = INVALID_SOCKET;
extern int8_t g_rf_Link_status;                             //wifi连接状态：0-未连接，1-连接wifi，2-连接智云 3-智云认证成功
extern int8_t g_led_Link_status;                            //wifi连接状态，记录最大状态
static char recvBuf[TCP_CLIENT_RECV_SIZE];                  //接收数据缓存
extern t_rf_info rf_info;                                   //通讯模块信息

static uint16_t reconnection_cnt=0;                         //重连次数
static uint16_t rf_if_heart_try;                            //心跳包尝试次数

static int close_socket(void)
{
  int ret;
  ret = closesocket(sock_fd);
  if(ret!=0) {
    LOG_E("closesocket fail!");
    return -1;
  }
  sock_fd = INVALID_SOCKET;
  return 0;
}

//返回：
//正数：发送数据长度
//负数：错误码
static int tcp_client_sendData(char *data, unsigned int len)
{
    int ret;
    if (sock_fd < 0){
        LOG_E("send error: socket null.");
        return 0;
    }
    
    /* 发送数据到sock连接 */
    ret = send(sock_fd, data, strlen(data), 0);
    if (ret < 0){
        /* 发送失败，关闭这个连接 */
        close_socket();
        LOG_E(" send error,close the socket.");
    }
    else if (ret == 0){
        /* 打印send函数返回值为0的警告信息 */
        LOG_E("Send warning,send function return 0.");
    }
    return ret;
}

//返回：
//正数：发送数据长度
//负数：错误码
static int udp_client_sendData(char *data, unsigned int len)
{
    int ret;
    struct sockaddr_in server_addr;
    if (sock_fd < 0){
        LOG_E("send error: socket null.");
        return 0;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(rf_info.zhiyun.port);
    server_addr.sin_addr.s_addr = inet_addr(rf_info.zhiyun.ip);

    ret = sendto(sock_fd, data, strlen(data), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if( ret < 0) {
        /* 发送失败，关闭这个连接 */
        // close_socket();
        LOG_E("send error");
    } else if (ret == 0){
        /* 打印send函数返回值为0的警告信息 */
        LOG_E("Send warning,send function return 0.");
    }

    return ret;
}

//设置wifi的连接状态标志位
void rf_set_link_status(uint8_t new)
{
#ifndef USE_RTOS2_API
    LOS_TaskLock();
    g_rf_Link_status=new;
    if(new>=g_led_Link_status)
      g_led_Link_status=new;
    LOS_TaskUnlock();
#else
    osKernelLock();
    g_rf_Link_status=new;
    if(new>=g_led_Link_status)
      g_led_Link_status=new;
    osKernelUnlock();
#endif
}

/* 通讯接口发送数据到网关/服务器；返回值：-1--发送失败；>=0：发送出的字节数 */
static int16_t rf_if_send(uint8_t *pdata, uint16_t length, uint16_t timeout_ms) {
    int ret=0;
    char send[300];
    int send_len=0;
    struct timeval timeout;

#ifndef USE_RTOS2_API
    if(length==0 || sock_fd < 0 || rf_send_mutex==INVALID_MUTEX) return -1;
    if(g_rf_Link_status < 2 || LOS_MuxPend(rf_send_mutex, LOS_MS2Tick(1000)) != LOS_OK) return -2;
#else
    if(length==0 || sock_fd < 0 || rf_send_mutex==NULL) return -1;
    if(g_rf_Link_status < 2 || osMutexAcquire(rf_send_mutex, LOS_MS2Tick(1000)) != osOK) return -2;
#endif

    //设置socket超时时间
    timeout = msec2timeval(timeout_ms);
    if(setsockopt(sock_fd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout)) < 0){
        LOG_E("socket:%d set SendTimeout fail",sock_fd);
        ret = -3;
        goto _ret;
    }

    /* 智云数据打包 */
    //认证指令
    if(strstr((char *)pdata, "\"method\":\"authenticate\"")) {
        int32_t _len = snprintf(send, sizeof(send),
            "{\"method\":\"authenticate\",\"uid\":\"%s\",\"key\":\"%s\", \"addr\":\"%s\", \"version\":\"0.1.0\", \"autodb\":true}", 
            rf_info.zhiyun.zhiyun_id, rf_info.zhiyun.zhiyun_key, rf_info.wifi.mac);
        if((u_int32_t)_len>=sizeof(send)){
          LOG_E("[%s] send buf overflow",__func__);
          ret = -4;
          goto _ret;
        }
        send_len = _len;
    }
    //心跳包
    else if(strstr((char *)pdata, "\"method\":\"echo\"" )){
        static uint32_t echo_seq = 0;
        int32_t _len = snprintf(send, sizeof(send),
               "{\"method\":\"echo\",\"timestamp\":%u,\"seq\":%u}", (UINT32)LOS_TickCountGet(), echo_seq++);
        if((u_int32_t)_len>=sizeof(send)){
          LOG_E("[%s] send buf overflow",__func__);
          ret = -4;
          goto _ret;
        }
        send_len = _len;
    }
    //非认证指令，需要打包数据
    else {
      if(rf_info.base.link_type == 0){
        int32_t _len = snprintf(send, sizeof(send),"{\"method\":\"sensor\", \"data\":\"%s\"}", pdata);
        if((u_int32_t)_len>=sizeof(send)){
          LOG_E("[%s] send buf overflow",__func__);
          ret = -4;
          goto _ret;
        }
        send_len = _len;
      }else if(rf_info.base.link_type == 1){
        // 00:11:22:33:44:55={A0=XX,A1=32} 这种格式发送到网关的udp  7003 端口
        int32_t _len = snprintf(send, sizeof(send),"%s=%s", &rf_info.wifi.mac[5] ,pdata);
        if((u_int32_t)_len>=sizeof(send)){
          LOG_E("[%s] send buf overflow",__func__);
          ret = -4;
          goto _ret;
        }
        send_len = _len;
      }
    }
    send[send_len] = 0;
      if(rf_info.base.link_type == 0){
        /* 发送数据到服务器 */
        if(tcp_client_sendData(send, strlen(send))>0){
            log_wraw("zhiyun <-- %s\r\n", send);
        }else{
            LOG_E("[%s] sendData fail",__func__);
            ret = -5;
            goto _ret;
        }
        ret = length;
      }else if(rf_info.base.link_type == 1){
        /* 发送数据到服务器 */
        if(udp_client_sendData(send, strlen(send))>0){
            log_wraw("gateway <-- %s\r\n", send);
            //at_print_raw_cmd_ex(0,"gateway",(const char *)send,strlen((const char *)send));
        }else{
            LOG_E("[%s] sendData fail",__func__);
            ret = -5;
            goto _ret;
        }
        ret = length;
      }

_ret:
#ifndef USE_RTOS2_API
    LOS_MuxPost(rf_send_mutex);
#else
    osMutexRelease(rf_send_mutex);
#endif
    return ret;
}

// 连接上智云认证成功后使用
int16_t rf_send(uint8_t *pdata, uint16_t length, uint16_t timeout_ms)
{
  if(g_rf_Link_status < 3) return -1;
  int16_t res=0;
  res = rf_if_send(pdata, length, timeout_ms);
  LOG_D("length:%d,res:%d",length,res);
  if(res==length){
    //数据灯切换
#ifndef USE_RTOS2_API
    LOS_TaskLock();
    g_LedEvent=1;
    LOS_TaskUnlock();
#else
    osKernelLock();
    g_LedEvent=1;
    osKernelUnlock();
#endif
  }
  return res;
}

// wifi断开智云
INT8 Wifi_DisConnect_zhiyun_and_gateway(void)
{
    if(sock_fd<0){
        LOG_E("sock_fd error");
        return -1;
    }
    if(close_socket()!=0) return -2;
    return 0;
}

/*********************************************************************************************
* 连接智云相关
*********************************************************************************************/
// wifi连接智云
// 返回
// -1: 连接状态错误
// -2: gethostbyname错误
// -3: 创建socket失败
// -4: 连接智云失败
INT8 Wifi_Connecting_zhiyun(void)
{
    #undef LEVEL
    #undef STEPS
    #define LEVEL   2
    #define STEPS   3
    int steps=0;
    //服务器的地址信息
    struct hostent *host = NULL;
    struct sockaddr_in server_addr;

    if(g_rf_Link_status!=1){
        LOG_E("g_rf_Link_status status error");
        return -1;
    }

    /* 创建一个socket */
    //通过函数入口参数url获得host地址（如果是域名，会做域名解析）
    host = gethostbyname(ZHIYUN_DEFAULT_IP);
    if (host == NULL){
        LOG_E("gethostbyname error");
        return -2;
    }
    //创建一个socket，类型是SOCKET_STREAM，TCP类型
    if (sock_fd < 0){
        if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
            LOG_E("Socket error");
            return -3;
        }
    }
    LOG_I("socket Create success,id:%d",sock_fd);

    steps++;
    LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);

    //设置socket保持连接
    int optval = 1;
    if(setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval, sizeof(optval)) < 0) {
        LOG_E("Failed to set SO_KEEPALIVE on fd %d", sock_fd);
    }

    steps++;
    LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);

    /* 初始化预连接的服务端地址 */
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ZHIYUN_DEFAULT_PORT);
    memcpy(&server_addr.sin_addr.s_addr,host->h_addr_list[0],sizeof(server_addr.sin_addr.s_addr));

    /* 连接到服务端 */
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1){
        LOG_E("zhiyun Connect fail!");
        close_socket();
        return -4;
    }else{
        LOG_I("zhiyun Connect success");

        steps++;
        LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);
        return 0;
    }
    return 0;
}

//wifi认证和心跳包发送
//正常情况下一直阻塞
INT8 Wifi_zhiyun_loop(void)
{
    #undef LEVEL
    #undef STEPS
    #define LEVEL   3
    #define STEPS   2
    int steps=0;

    uint8_t try_times = 0;

    /* 执行节点注册到智云的操作 */
    while(try_times++ < MAX_AUTHE_RETRY_CNT && g_rf_Link_status==2) {
        rf_if_send((uint8_t *)"{\"method\":\"authenticate\"}", strlen("{\"method\":\"authenticate\"}"), 100);
        LOS_Msleep(1000);
    }
    steps++;
    LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);

    /* 智云认证成功,定时发送心跳包，检测是否出现断线 */
    int heart_count=0;
    while(g_rf_Link_status == 3){
        if(++heart_count >=30){
            //30秒发送1次心跳包
            heart_count=0;
            //心跳包发送3次无回复：连接已断开，断线重连
            if(rf_if_heart_try++ >= MAX_HEART_RETRY_CNT) {
                rf_if_heart_try = 0;
                rf_set_link_status(0);

                break;
            }

            rf_send((uint8_t *)"{\"method\":\"echo\"}", strlen("{\"method\":\"echo\"}"), 100);
        }

        LOS_Msleep(1000);
    }
    hi_at_printf("+LINK:0\r\nOK\r\n");

    /* 到这个步骤说明异常 */
    steps++;
    LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);
    return 0;
}

/*********************************************************************************************
* 连接 udp 网关 
*********************************************************************************************/
// wifi 创建 UDP socket
INT8 Wifi_create_socket(void)
{
    #undef LEVEL
    #undef STEPS
    #define LEVEL   2
    #define STEPS   2
    int steps=0;
    struct sockaddr_in server_addr;

    if(g_rf_Link_status!=1){
        LOG_E("g_rf_Link_status status error");
        return -1;
    }

    /* 创建一个socket */
    //创建一个socket，类型是SOCKET_DGRAM，UDP类型
    if (sock_fd < 0){
        if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
            LOG_E("Socket error");
            return -3;
        }
    }
    LOG_I("socket Create success,id:%d",sock_fd);

    steps++;
    LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);

    //设置socket保持连接
    int optval = 1;
    if(setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval, sizeof(optval)) < 0) {
        LOG_E("Failed to set SO_KEEPALIVE on fd %d", sock_fd);
    }

    steps++;
    LOG_I("[%s] level%d--step:%d/%d",__func__,LEVEL,steps,STEPS);

    return 0;
}

int isValidIPv4WithInetPton(const char *ip) {
    struct in_addr addr;
    return inet_pton(AF_INET, ip, &addr) == 1;
}

//wifi认证和心跳包发送
//正常情况下一直阻塞
INT8 Wifi_udp_loop(void)
{
    int steps=0;

    if(!isValidIPv4WithInetPton(rf_info.zhiyun.ip)) return 0;
    rf_set_link_status(3);
    hi_at_printf("+LINK:1\r\nOK\r\n");
    #ifndef WIFI_SERIAL      /*终端节点*/
      #ifdef CONFIG_SENSOR_NODE
        sensor_fun_p.sensorLinkOn();
      #else
        sensorLinkOn();
      #endif
    #endif

    while(g_rf_Link_status == 3){
        LOS_Msleep(1000);
    }
    hi_at_printf("+LINK:0\r\nOK\r\n");
    return 0;
}

//rf 连接处理线程函数
int rf_if_Link(void)
{
    int ret=0;
    rf_set_link_status(0);

    /* 连接热点 */
    ret = Wifi_Connecting_hotspots();
    if(ret!=0){
        LOG_E("Wifi_Connecting_hotspots FAIL! errcode:%d",ret);
        goto _reconnection; 
    }
    rf_set_link_status(1);

    if(rf_info.base.link_type==0){
      /* 连接智云 */
      ret = Wifi_Connecting_zhiyun();
      if(ret!=0){
          LOG_E("Wifi_Connecting_zhiyun FAIL! errcode:%d",ret);
          goto _reconnection; 
      }
      rf_set_link_status(2);

      /* 智云通讯循环,正常情况一直循环 */
      Wifi_zhiyun_loop();
    }else if(rf_info.base.link_type==1){
      /* 创建 UDP socket */
      ret = Wifi_create_socket();
      if(ret!=0){
          LOG_E("Wifi_Connecting_gateway_udp FAIL! errcode:%d",ret);
          goto _reconnection; 
      }
      rf_set_link_status(2);

      /* 连接的 loop 循环 */
      Wifi_udp_loop();
    }

_reconnection:
    /* 断开连接重连 */
    if(Wifi_DisConnect_zhiyun_and_gateway()!=WIFI_SUCCESS )
        LOG_E("Wifi_DisConnect_zhiyun_and_gateway FAIL!");
    if(Wifi_DisConnect_hotspots()!=WIFI_SUCCESS)
        LOG_E("Wifi_DisConnect_hotspots FAIL!");
    rf_set_link_status(0);
    LOS_Msleep(1000);
    // 重来次数限制
    if(++reconnection_cnt>=MAX_RECONNECTION_CNT){
        LOG_E("Exceeded reconnection times");
        return -1;
    }
    return 0;
}

/*********************************************************************************************
* 数据接收相关
*********************************************************************************************/
// data: 智云下行的数据 len:数据长度(字符串长度)
static int zy_recvAnalysis(char* data,int len)
{
    char *phead = NULL, *ptail = NULL;
    char info_data[201]={0};
    uint16_t info_len=0;

    if(rf_info.base.link_type==0){
      log_wraw("zhiyun --> %s\r\n", data);

      /* 智云数据解析 */
      if(len == 0 || data[len-1] != '}') return -1;
      phead = strstr(data, "method");
      if(phead == NULL || *(phead+8) != '"') return -2;
      //指向method的键值
      phead = phead+9;
      /* 认证指令回复 */
      if(strncmp(phead, "authenticate_rsp", strlen("authenticate_rsp")) == 0) {
          /* 指令类型: {"method":"authenticate_rsp","status":"ok"} */
          phead = strstr(phead, "status");
          if(phead == NULL || *(phead+8) != '"') return -5;
          ptail = strchr(phead+9, '"');
          info_len = ptail - phead - 9;
          strncpy((char *)info_data, phead+9, info_len);
          info_data[info_len] = 0;

          LOG_D("info_data: %s", info_data);
          //验证数据
          if(strncmp(info_data, "ok", strlen("ok")) == 0){
              rf_set_link_status(3);
              hi_at_printf("+LINK:1\r\nOK\r\n");
              #ifndef WIFI_SERIAL      /*终端节点*/
                #ifdef CONFIG_SENSOR_NODE
                  sensor_fun_p.sensorLinkOn();
                #else
                  sensorLinkOn();
                #endif
              #endif
              LOG_I("zhiyun authentication success");
          }else if(strncmp(info_data, "error", strlen("error")) == 0){
              rf_set_link_status(0);
              LOG_E("zhiyun authentication fail");
          }else{
              rf_set_link_status(0);
              LOG_E("zhiyun authentication unknown error");
          }
      }
      /* 服务器下发指令 */
      else if(strncmp(phead, "control", strlen("control")) == 0) {
          /* 指令类型: {"method":"control","data":"{D1=?}"} */
          phead = strstr(phead, "data");
          if(phead == NULL || *(phead+6) != '"') return -1;
          ptail = strchr(phead+7, '"');
          info_len = ptail - phead - 7;
          strncpy((char *)info_data, phead+7, info_len);
          info_data[info_len] = 0;

          LOG_D("info_data: %s", info_data);

          char cmd[40];
          snprintf(cmd,40, "+RECV:%u,%d\r\n", strlen(info_data),g_wifi_rssi);
          hi_at_printf(cmd);
          hi_at_printf(info_data);
          
  #ifndef WIFI_SERIAL      /*终端节点*/
          if(info_data[0]=='{' && info_data[info_len-1]=='}') { //control类数据
              zxbee_onrecv_fun(info_data, info_len);
          }
  #endif
      } 
      /* 心跳包回复 */
      else if(strncmp(phead, "echo", strlen("echo")) == 0){
          rf_if_heart_try = 0;
      }
      /* 未知 */
      else{
          LOG_E("Reply not processed:%s",data);
      }
    }else if(rf_info.base.link_type==1){
      /* 智云数据解析 */
      if(len == 0 || data[len-1] != '}') return -1;

      log_wraw("gateway --> %s\r\n", data);
      /* 服务器下发指令 */
      // 固定格式: 00:94:51:06:40:6C={TYPE=?}
      strcpy(info_data,&data[18]);
      info_len = strlen(info_data);

      char cmd[40];
      snprintf(cmd,40, "+RECV:%u,%d\r\n", strlen(info_data),g_wifi_rssi);
      hi_at_printf(cmd);
      hi_at_printf(info_data);
      
#ifndef WIFI_SERIAL      /*终端节点*/
      if(info_data[0]=='{' && info_data[info_len-1]=='}') { //control类数据
          zxbee_onrecv_fun(info_data, info_len);
      }
#endif //WIFI_SERIAL
    }

    return 0;
}

//rf 数据接收处理线程函数
int rf_if_recv(void)
{
    if (g_rf_Link_status>=2 && sock_fd >= 0){
      if(rf_info.base.link_type == 0){
        int bytes_received = recv(sock_fd, recvBuf, TCP_CLIENT_RECV_SIZE - 1, 0);
        if (bytes_received > 0){
            int len = strlen(recvBuf);
            recvBuf[len] = '\0';
            zy_recvAnalysis(recvBuf,len);
        }
        else{
            LOG_E("recv error,%d",bytes_received);
            LOS_Msleep(200);
        }
      }else if(rf_info.base.link_type == 1){
        struct sockaddr_in client_addr;  // 存储客户端地址信息
        socklen_t addr_len = sizeof(client_addr);  // 地址长度

        // 使用 recvfrom 接收数据
        int bytes_received = recvfrom(sock_fd, recvBuf, TCP_CLIENT_RECV_SIZE - 1, 0, 
                                      (struct sockaddr *)&client_addr, &addr_len);
        if (bytes_received > 0) {
            // 确保字符串以 null 结尾
            recvBuf[bytes_received] = '\0';

            // 打印客户端的 IP 和端口（可选）
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            LOG_D("Received data from %s:%d", client_ip, ntohs(client_addr.sin_port));

            // 处理接收到的数据
            zy_recvAnalysis(recvBuf, bytes_received);
        }
        else{
            LOG_E("recv error,%d",bytes_received);
            LOS_Msleep(200);
        }
      }
    }
    else
        LOS_Msleep(100);
    return 0;
}

/*********************************************************************************************
* rf信息初始化
*********************************************************************************************/
// 初始化rf连接信息
void rf_info_init(void)
{
  hi_u32 ret;

  //读取通讯模块信息
  read_sys_parameter();
  rf_info_dump();

  /* 设置wifi默认信息 */
  //MAC地址后面获取
  if(strncmp(rf_info.wifi.mac, "WIFI:", 5) != 0) {
    strcpy(rf_info.wifi.mac, "WIFI:");
  }
  if( rf_info.wifi.wifi_ssid[0] == 0xFF || 
      strlen(rf_info.wifi.wifi_ssid) >= sizeof(rf_info.wifi.wifi_ssid) || 
      strlen(rf_info.wifi.wifi_ssid)==0) {
    //rf_info.wifi.wifi_ssid[0] = 0;
    strcpy(rf_info.wifi.wifi_ssid, WIFI_DEFAULT_SSID);
  }
  if( rf_info.wifi.wifi_key[0] == 0xFF || 
      strlen(rf_info.wifi.wifi_key) >= sizeof(rf_info.wifi.wifi_key) ||
      strlen(rf_info.wifi.wifi_key)==0) {
    //rf_info.wifi.wifi_key[0] = 0;
    strcpy(rf_info.wifi.wifi_key, WIFI_DEFAULT_PASSWORD);
  }

  /* 设置智云默认信息 */
  if( rf_info.zhiyun.zhiyun_id[0] == 0xFF || 
      strlen(rf_info.zhiyun.zhiyun_id) >= sizeof(rf_info.zhiyun.zhiyun_id) ||
      strlen(rf_info.zhiyun.zhiyun_id)==0 ) {
    //rf_info.zhiyun.zhiyun_id[0] = 0;
    strcpy(rf_info.zhiyun.zhiyun_id,ZHIYUN_DEFAULT_ID);
  }
  if( rf_info.zhiyun.zhiyun_key[0] == 0xFF || 
      strlen(rf_info.zhiyun.zhiyun_key) >= sizeof(rf_info.zhiyun.zhiyun_key) ||
      strlen(rf_info.zhiyun.zhiyun_key)==0 ) {
    //rf_info.zhiyun.zhiyun_key[0] = 0;
    strcpy(rf_info.zhiyun.zhiyun_key,ZHIYUN_DEFAULT_KEY);
  }
  if( rf_info.zhiyun.ip[0] == 0xFF || 
      strlen(rf_info.zhiyun.ip) >= sizeof(rf_info.zhiyun.ip) ||
      strlen(rf_info.zhiyun.ip)==0 ) {
    strcpy(rf_info.zhiyun.ip, ZHIYUN_DEFAULT_IP);
    // rf_info.zhiyun.port = ZHIYUN_DEFAULT_PORT;
  }

  if(strlen(rf_info.zhiyun.zhiyun_id) && strlen(rf_info.zhiyun.zhiyun_key)){
    rf_info.base.link_type = 0;    // 0：连接智云 1：UDP连接网关  
    //if(rf_info.port == 0xFFFF )
    rf_info.zhiyun.port = ZHIYUN_DEFAULT_PORT;
  }else{
    rf_info.base.link_type = 1;
    //if(rf_info.port == 0xFFFF)
    rf_info.zhiyun.port = DEFAULT_UDP_PORT;
  }
  // rf_info_dump();
}

void rf_app_init(void)
{
  rf_info_init();       //初始化无线模块参数
  rf_if_init_mac();     //初始化MAC地址
  rf_if_get_mac();      //获取MAC地址
  led_Init();           //LED初始化
}