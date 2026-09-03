/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "esp_event_loop.h"
 #include "esp_err.h"

// #include "lwip/err.h"
// #include "lwip/sys.h"
#include "lwip/sockets.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "LCKFB-ESP32"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
//#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       5    //最大连接数
#define TCP_PORT                9527                // 监听客户端端口
#define WIFI_CONNECTED_BIT  BIT0

static const char *TAG = "wifi softAP";

//socket
static int server_socket = 0;                       // 服务器socket
static struct sockaddr_in server_addr;              // server地址
static struct sockaddr_in client_addr;              // client地址
static unsigned int socklen = sizeof(client_addr);  // 地址长度
static int connect_socket = 0;                      // 连接socket
bool g_rxtx_need_restart = false;                   // 异常后，重新连接标记

EventGroupHandle_t tcp_event_group;// wifi建立成功信号量

//wifi 事件
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data)
{
    switch (event_id){
    case WIFI_EVENT_AP_STACONNECTED:  //AP模式-有STA连接成功
        // 作为ap，有sta连接
//      ESP_LOGI(TAG, "station:" MACSTR " join,AID=%d\n",MAC2STR(event->event_info.sta_connected.mac),event->event_info.sta_connected.aid);
        //设置事件位
        xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
        break;
    case WIFI_EVENT_AP_STADISCONNECTED://AP模式-有STA断线
//      ESP_LOGI(TAG, "station:" MACSTR "leave,AID=%d\n",MAC2STR(event->event_info.sta_disconnected.mac),event->event_info.sta_disconnected.aid);
        //重新建立server
        g_rxtx_need_restart = true;
        xEventGroupClearBits(tcp_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
}

void wifi_init_softap(void)
{
    tcp_event_group = xEventGroupCreate();
    
    //tcpip_adapter_init();   
    

    //初始化TCP/IP底层栈
    ESP_ERROR_CHECK(esp_netif_init());
    //创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //创建默认WIFI AP。在任何初始化错误的情况下，此API中止。
    esp_netif_create_default_wifi_ap();
    
    

    //WiFi栈配置参数传递给esp_wifi_init调用。
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    //为WIFI任务分配资源
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //将事件处理程序的实例注册到默认循环。
    //这个函数的功能与esp_event_handler_instance_register_with相同，
    //只是它将处理程序注册到默认的事件循环。
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,  //WIFI账号
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),//WIFI账号长度
            //.channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,//WIFI密码
            .max_connection = EXAMPLE_MAX_STA_CONN,//最大客户端接入数
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    //设置WIFI工作模式为AP模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    //设置AP模式的配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    //开启WIFI
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s ",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);

}
// 获取socket错误代码
int get_socket_error_code(int socket)
{
    int result;         
    u32_t optlen = sizeof(int);
    int err = getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen);
    if (err == -1){
        //WSAGetLastError();
        ESP_LOGE(TAG, "socket error code:%d", err);
        ESP_LOGE(TAG, "socket error code:%s", strerror(err));
        return -1;
    }
    return result;
}
// 获取socket错误原因
int show_socket_error_reason(const char *str, int socket)
{
    int err = get_socket_error_code(socket);
    if (err != 0){
        ESP_LOGW(TAG, "%s socket error reason %d %s", str, err, strerror(err));
    }
    return err;
}
// 关闭socket
void close_socket()
{
    close(connect_socket);
    close(server_socket);
}
// 接收数据任务
void recv_data(void *pvParameters)
{
    int len = 0;
    char databuff[1024];
    while (1){
        memset(databuff, 0x00, sizeof(databuff));//清空缓存
        len = recv(connect_socket, databuff, sizeof(databuff), 0);//读取接收数据
        g_rxtx_need_restart = false;
        if (len > 0){
            ESP_LOGI(TAG, "recvData: %s", databuff);//打印接收到的数组
            send(connect_socket, databuff, strlen(databuff), 0);//接收数据回发
            //sendto(connect_socket, databuff , sizeof(databuff), 0, (struct sockaddr *) &remote_addr,sizeof(remote_addr));
        }else{
        show_socket_error_reason("recv_data", connect_socket);//打印错误信息
            g_rxtx_need_restart = true;//服务器故障，标记重连
            vTaskDelete(NULL);
        }
    }
    close_socket();
    g_rxtx_need_restart = true;//标记重连
    vTaskDelete(NULL);
}

// 建立tcp server
esp_err_t create_tcp_server(bool isCreatServer)
{
    //首次建立server
    if (isCreatServer){
        ESP_LOGI(TAG, "server socket....,port=%d", TCP_PORT);
        server_socket = socket(AF_INET, SOCK_STREAM, 0);//新建socket
        if (server_socket < 0){
            show_socket_error_reason("create_server", server_socket);
            close(server_socket);//新建失败后，关闭新建的socket，等待下次新建
            return ESP_FAIL;
        }
        //配置新建server socket参数
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TCP_PORT);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        //bind:地址的绑定
        if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
            show_socket_error_reason("bind_server", server_socket);
            close(server_socket);//bind失败后，关闭新建的socket，等待下次新建
            return ESP_FAIL;
        }
    }
    //listen，下次时，直接监听
    if (listen(server_socket, 5) < 0){
        show_socket_error_reason("listen_server", server_socket);
        close(server_socket);//listen失败后，关闭新建的socket，等待下次新建
        return ESP_FAIL;
    }
    //accept，搜寻全连接队列
    connect_socket = accept(server_socket, (struct sockaddr *)&client_addr, &socklen);
    if (connect_socket < 0){
        show_socket_error_reason("accept_server", connect_socket);
        close(server_socket);//accept失败后，关闭新建的socket，等待下次新建
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "tcp connection established!");
    return ESP_OK;
}
// 建立TCP连接并从TCP接收数据
static void tcp_connect(void *pvParameters)
{
    while (1){
        g_rxtx_need_restart = false;
        // 等待WIFI连接信号量，死等
        //阻塞等待一个或多个位在先前创建的事件组中被设置。
        xEventGroupWaitBits(tcp_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
        
        //启动TCP连接
        ESP_LOGI(TAG, "start tcp connected");
        TaskHandle_t tx_rx_task = NULL;
        vTaskDelay(3000 / portTICK_PERIOD_MS);// 延时3S准备建立server

        ESP_LOGI(TAG, "create tcp server");
        int socket_ret = create_tcp_server(true);// 建立server
        if (socket_ret == ESP_FAIL){// 建立失败
            ESP_LOGI(TAG, "create tcp socket error,stop...");
            continue;
        }else{// 建立成功
            ESP_LOGI(TAG, "create tcp socket succeed...");
                //建立tcp接收数据任务
            if (pdPASS != xTaskCreate(&recv_data, "recv_data", 4096, NULL, 4, &tx_rx_task)){
                ESP_LOGI(TAG, "Recv task create fail!");
            }else{
                ESP_LOGI(TAG, "Recv task create succeed!");
            }
        }
        while (1){
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            if (g_rxtx_need_restart){// 重新建立server，流程和上面一样
                ESP_LOGI(TAG, "tcp server error,some client leave,restart...");
                // 重新建立server
                if (ESP_FAIL != create_tcp_server(false)){
                    if (pdPASS != xTaskCreate(&recv_data, "recv_data", 4096, NULL, 4, &tx_rx_task)){
                        ESP_LOGE(TAG, "tcp server Recv task create fail!");
                    }else{
                        ESP_LOGI(TAG, "tcp server Recv task create succeed!");
                        g_rxtx_need_restart = false;//重新建立完成，清除标记
                    }
                }
            }
        }
    }
    vTaskDelete(NULL);
}
void app_main(void)
{
    //初始化FLASH
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //建立一个AP
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    //新建一个tcp连接任务
    xTaskCreate(&tcp_connect, "tcp_connect", 4096, NULL, 5, NULL);
   

  while(1)
  {
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}
