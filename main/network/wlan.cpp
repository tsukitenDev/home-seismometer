#include "wlan.hpp"

#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_smartconfig.h"
#include "mdns.h"
#include "esp_system.h"

#include "esp_mac.h"


#include <time.h>
#include <sys/time.h>
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "../board_def.hpp"

#include "device_info.hpp"


#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define ESPTOUCH_DONE_BIT  BIT2
#define WIFI_DISCONNECTED_BIT BIT3
#define WIFI_STA_CONNECTED_BIT BIT4


#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY



static const char *TAG = "wifi station";


#define TIME_ZONE "JST-9"
#define NTP_SERVER  "ntp.jst.mfeed.ad.jp"



// tx_power = actual_power * 4 dBm
// tx_power range: [8-84]
// actual_power range: [2-20] dBm
#if BOARD_EQIS1 | BOARD_LSM6DSO_XIAO
static const int8_t tx_power = 20;
#else
static const int8_t tx_power = 66;
#endif



/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static bool is_configured = false;
static bool is_connect_failed = false;

static bool is_connected = false;
static bool is_sta_connecting = false;

static bool disconnecting = false;

static int s_retry_num = 0;


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data)
{
    if (event_base != WIFI_EVENT) return;
    if (event_id == WIFI_EVENT_STA_START) {
        s_retry_num = 0;
        //is_sta_connecting = true;
        esp_wifi_connect();
        return;
    }
    if (event_id == WIFI_EVENT_STA_CONNECTED) {
        is_sta_connecting = false;
        xEventGroupSetBits(s_wifi_event_group, WIFI_STA_CONNECTED_BIT);
        return;
    }
    if (event_id == WIFI_EVENT_STA_DISCONNECTED && !disconnecting) {
        // 想定外の切断
        is_connected = false;
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            is_sta_connecting = true;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            is_sta_connecting = false;
            is_connect_failed = true;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG,"connect to the AP failed");
        }
    }else if (event_id == WIFI_EVENT_STA_DISCONNECTED && disconnecting) {
        is_sta_connecting = false;
        is_connected = false;

        ESP_LOGI(TAG, "disconnected");

        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
    }
}

void sntp_sync_time();

static void ip_event_handler(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        is_connected = true;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
        esp_netif_t *netif = event->esp_netif;
        esp_netif_dns_info_t dns_info;
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
        ESP_LOGI(TAG, "Main DNS server : " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        // IPアドレス取得後に時刻同期
        sntp_sync_time();
    }
}


bool wifi_is_connected(){
    /*
    std::string ssid = "";
    wifi_ap_record_t ap_record;
        esp_err_t res = esp_wifi_sta_get_ap_info(&ap_record);
        if(res == ESP_OK) {
            ssid = std::string(reinterpret_cast<char*>(ap_record.ssid));
            if(ssid == "") return false;
            else return true;
        }else {
            return false;
        }
        return false;
    */
   return is_connected;
}


void wifi_init_sta(void)
{
    // NETIF初期化
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif =  esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // イベントハンドラ登録
    // インスタンスは不要かも
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL,
                                                        &instance_got_ip));


    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t currentConfig;
    esp_wifi_get_config(WIFI_IF_STA, &currentConfig);
    if(currentConfig.sta.ssid[0] != '\0') is_configured = true;
    ESP_LOGI(TAG, "stored SSID %s  password ******  channel %d",
                  currentConfig.sta.ssid,
                  //currentConfig.sta.password,
                  currentConfig.sta.channel);

    // Wi-Fi スタート
    // 前回の接続情報がNVSに保持されていれば再接続を試みる
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_LOGI(TAG, "set tx power to %d dBm", tx_power / 4);
    ESP_ERROR_CHECK( esp_wifi_set_max_tx_power(tx_power) );
    vTaskDelay(3000 / portTICK_PERIOD_MS);
}

bool wifi_connect_to_ap(std::string ssid, std::string password){
    EventBits_t bits;
    // 切断
    if(wifi_is_connected()){
        ESP_LOGI("wifi station", "disconnect the currently connected AP");
        disconnecting = true;
        esp_err_t res_disconnect = esp_wifi_disconnect();
        if(res_disconnect == ESP_ERR_WIFI_NOT_INIT || res_disconnect == ESP_ERR_WIFI_NOT_STARTED){
            // wifi_init_sta();
        }
        // 切断待ち
        bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_DISCONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
        disconnecting = false;
        is_connect_failed = false;
    }


    xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT | WIFI_FAIL_BIT);
   
    ESP_LOGI("wifi station", "connecting to AP... SSID %s  password ******", ssid.c_str());
    
    // SSID、パスワード設定
    wifi_config_t wifi_config = {
        .sta = {
            //.ssid = EXAMPLE_ESP_WIFI_SSID,
            //.password = EXAMPLE_ESP_WIFI_PASS,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password) - 1);

    wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    //ESP_ERROR_CHECK(esp_wifi_start() );
    s_retry_num = 0;
    is_configured = true;
    is_sta_connecting = true;
    is_connect_failed = false;
    esp_wifi_connect();

    // 接続待ち
    bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ssid.c_str(), password.c_str());
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
            ssid.c_str(), password.c_str());
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    return false;

}





std::string authmode_toString(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        return("WIFI_AUTH_OPEN");
    case WIFI_AUTH_OWE:
        return("WIFI_AUTH_OWE");
    case WIFI_AUTH_WEP:
        return("WIFI_AUTH_WEP");
    case WIFI_AUTH_WPA_PSK:
        return("WIFI_AUTH_WPA_PSK");
    case WIFI_AUTH_WPA2_PSK:
        return("WIFI_AUTH_WPA2_PSK");
    case WIFI_AUTH_WPA_WPA2_PSK:
        return("WIFI_AUTH_WPA_WPA2_PSK");
    case WIFI_AUTH_ENTERPRISE:
        return("WIFI_AUTH_ENTERPRISE");
    case WIFI_AUTH_WPA3_PSK:
        return("WIFI_AUTH_WPA3_PSK");
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return("WIFI_AUTH_WPA2_WPA3_PSK");
    case WIFI_AUTH_WPA3_ENT_192:
        return("WIFI_AUTH_WPA3_ENT_192");
    default:
        return("WIFI_AUTH_UNKNOWN");
    }
}

std::string authmode_toString_short(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        return("OPEN");
    case WIFI_AUTH_OWE:
        return("OWE");
    case WIFI_AUTH_WEP:
        return("WEP");
    case WIFI_AUTH_WPA_PSK:
        return("WPA PSK");
    case WIFI_AUTH_WPA2_PSK:
        return("WPA2 PSK");
    case WIFI_AUTH_WPA_WPA2_PSK:
        return("WPA WPA2 PSK");
    case WIFI_AUTH_ENTERPRISE:
        return("ENTERPRISE");
    case WIFI_AUTH_WPA3_PSK:
        return("WPA3 PSK");
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return("WPA2 WPA3 PSK");
    case WIFI_AUTH_WPA3_ENT_192:
        return("WPA3 ENT 192");
    default:
        return("UNKNOWN");
    }
}


std::string cipher_toString(int cipher)
{
    switch (cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        return("WIFI_CIPHER_TYPE_NONE");
    case WIFI_CIPHER_TYPE_WEP40:
        return("WIFI_CIPHER_TYPE_WEP40");
    case WIFI_CIPHER_TYPE_WEP104:
        return("WIFI_CIPHER_TYPE_WEP104");
    case WIFI_CIPHER_TYPE_TKIP:
        return("WIFI_CIPHER_TYPE_TKIP");
    case WIFI_CIPHER_TYPE_CCMP:
        return("WIFI_CIPHER_TYPE_CCMP");
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        return("WIFI_CIPHER_TYPE_TKIP_CCMP");
    case WIFI_CIPHER_TYPE_AES_CMAC128:
        return("WIFI_CIPHER_TYPE_AES_CMAC128");
    case WIFI_CIPHER_TYPE_SMS4:
        return("WIFI_CIPHER_TYPE_SMS4");
    case WIFI_CIPHER_TYPE_GCMP:
        return("WIFI_CIPHER_TYPE_GCMP");
    case WIFI_CIPHER_TYPE_GCMP256:
        return("WIFI_CIPHER_TYPE_GCMP256");
    default:
        return("WIFI_CIPHER_TYPE_UNKNOWN");
    }
}


std::vector<ap_record_t> wifi_scan(){

    ESP_LOGI("TAG", "Scan requested");
    const uint16_t DEFAULT_SCAN_LIST_SIZE = 16;
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ESP_ERROR_CHECK(esp_wifi_start());
    // スキャン完了までブロック
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if(err == ESP_ERR_WIFI_STATE && is_sta_connecting) {
        // 接続中なら接続完了を待つか接続解除
        ESP_LOGW(TAG, "Wait until connection is established");
        xEventGroupWaitBits(s_wifi_event_group,
            WIFI_STA_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            20000 / portTICK_PERIOD_MS);
        if(is_sta_connecting) {
            return {};
        }
        ESP_LOGI(TAG, "retry scan");
        err = esp_wifi_scan_start(NULL, true);
    }
    if(err != ESP_OK) {
        return {};
    }
    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    std::vector<ap_record_t> res;
    for (int i = 0; i < number; i++) {
        res.push_back({.ssid = std::string(reinterpret_cast<char*>(ap_info[i].ssid)),
                       .rssi = ap_info[i].rssi,
                       .authmode = authmode_toString(ap_info[i].authmode)
                      });
        ESP_LOGI(TAG, "SSID %s, RSSI %d, authmode %s", 
            ap_info[i].ssid, 
            ap_info[i].rssi, 
            authmode_toString(ap_info[i].authmode).c_str());
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            ESP_LOGI(TAG, "%s %s", cipher_toString(ap_info[i].pairwise_cipher).c_str(),
            cipher_toString(ap_info[i].group_cipher).c_str());
        }
        //ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
    }
    return res;
}








void sntp_init(const char* time_zone, const char* ntp_server){
    // タイムゾーン設定
    setenv("TZ", time_zone, 1);
    tzset();

    //ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server);
    esp_netif_sntp_init(&config);
}


void sntp_sync_time(){
    time_t now;
    struct tm timeinfo;

    int retry = 0;
    const int max_retry = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < max_retry) {
        ESP_LOGI("sntp", "Waiting for system time to be set... (%d/%d)", retry, max_retry);
    }
    time(&now);
    char strftime_buf[64];
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI("time", "%s", strftime_buf);

}



extern device_info_t device_info;


void start_mdns_service()
{
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return;
    }

    mdns_hostname_set(device_info.mdns_hostname.c_str());
    mdns_instance_name_set(device_info.mdns_instancename.c_str());
    ESP_LOGI("mdns", "start mdns service %s", device_info.mdns_hostname.c_str());
}

void wifi_init(){
  esp_log_level_set("wifi", ESP_LOG_WARN); 

  sntp_init(TIME_ZONE, NTP_SERVER);

  wifi_init_sta();
  //wifi_scan();

  // sntp_sync_time(); // IPアドレス取得後に変更

  start_mdns_service();

  return;
}

WIFI_STATE wifi_get_state(){
    WIFI_STATE res = WIFI_STATE::NOT_CONFIGURED;
    if(is_configured)     res = WIFI_STATE::NOT_CONNECTED;
    if(is_sta_connecting) res = WIFI_STATE::CONNECTING;
    if(is_connected)      res = WIFI_STATE::CONNECTED;
    if(is_connect_failed) res = WIFI_STATE::CONNECT_FAILED;
    return res;
}

ap_record_t wifi_get_current_ap() {
    ap_record_t current_ap;
    current_ap.ssid = "";
    current_ap.rssi = 0;
    current_ap.authmode = "";

    if (wifi_is_connected()) {
        wifi_ap_record_t ap_record;
        esp_err_t res = esp_wifi_sta_get_ap_info(&ap_record);
        if (res == ESP_OK) {
            current_ap.ssid = std::string(reinterpret_cast<char*>(ap_record.ssid));
            current_ap.rssi = ap_record.rssi;
            current_ap.authmode = authmode_toString_short(ap_record.authmode);
        }
    }
    return current_ap;
}

std::string wifi_get_ip_address_str() {
    esp_netif_ip_info_t ip_info_sta;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != NULL) {
        esp_err_t ip_err = esp_netif_get_ip_info(netif, &ip_info_sta);
        if (ip_err == ESP_OK) {
            char ip_str[IP4ADDR_STRLEN_MAX];
            esp_ip4addr_ntoa(&ip_info_sta.ip, ip_str, IP4ADDR_STRLEN_MAX);
            return std::string(ip_str);
        }
    }
    return "";
}
