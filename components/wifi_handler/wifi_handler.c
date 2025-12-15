#include "wifi_handler.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <string.h>

#include "mdns.h"
#include <lwip/apps/netbiosns.h>

#include "status_handler.h"

#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

#define HOSTNAME                "insect_monitor"
#define MDNS_INSTANCE           "web server"

#define DEFAULT_AP_NAME         "InsectMonitor"
#define DEFAULT_AP_PASSWORD     "InsectMonitor"
#define DEFAULT_MODE            "AP"

#define WIFI_HANDLER_TASK_SIZE  (3 * 1024)
#define WIFI_MAX_RETRIES        8


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group = NULL;

static const char *TAG = "WIFI_HANDLER";
static esp_netif_t *netif_con = NULL;
static volatile bool isConnected = false;

static void event_handler (void *arg, esp_event_base_t event_base,
                            int32_t event_id, void * event_data)
{
    static int retry = 0;
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        esp_wifi_connect();
    } else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        if (retry < WIFI_MAX_RETRIES) {
            retry++;
            ESP_LOGD(TAG, "retrying to connect to AP");
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            retry = 0;
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t* event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        retry = 0;
    }
}

static esp_err_t start_mdns(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(mdns_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(mdns_hostname_set(HOSTNAME));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mdns_instance_name_set(MDNS_INSTANCE));

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}
    };

    if (!mdns_service_exists("_http", "_tcp", NULL)) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(mdns_service_add(NULL, "_http", "_tcp", 80, serviceTxtData,
                                        sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
    }

    return ESP_OK;
}

static void parse_wifi_settings(char *ssid, char *pass, char *mode)
{
}

esp_err_t restart_wifi(void)
{
    mdns_free();
    esp_event_loop_delete_default();
    esp_netif_destroy_default_wifi(netif_con);
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    return start_wifi(WIFI_HANDLER_MODE_FROM_SETTINGS);
}

static void wh_task(void *arg)
{
    bool isSTA = false;
    wifi_handler_mode_t *wh_mode = (wifi_handler_mode_t*) arg;

    char ssid[20] = {0};
    char pass[20] = {0};
    char mode[10] = {0};

    if (*wh_mode == WIFI_HANDLER_MODE_FROM_SETTINGS) {
        parse_wifi_settings(ssid, pass, mode);
    } else if (*wh_mode == WIFI_HANDLER_MODE_DEFAULT_AP) {
        memcpy(ssid, DEFAULT_AP_NAME, sizeof(DEFAULT_AP_NAME)-1);
        memcpy(pass, DEFAULT_AP_PASSWORD, sizeof(DEFAULT_AP_PASSWORD)-1);
        memcpy(mode, DEFAULT_MODE, sizeof(DEFAULT_MODE)-1);
    }

    if (!strcmp(mode, "Client")) {
        isSTA = true;
    } else if (!strcmp(mode, "AP")) {
        isSTA = false;
    } else {
        ESP_LOGE(TAG, "Wrong mode: (%s). Loading default AP mode", mode);
        goto wh_finish;
    }

    if (!strlen((char*)ssid) && (strlen((char*)pass) < 8)) {
        ESP_LOGE(TAG, "ssid or pass is too short");
        goto wh_finish;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if(err != ESP_ERR_INVALID_STATE){
        ESP_ERROR_CHECK(err);
    }
    
    if(isSTA){
        if(s_wifi_event_group == NULL){
            s_wifi_event_group = xEventGroupCreate();
        }
        netif_con = esp_netif_create_default_wifi_sta();
    } else {
        netif_con = esp_netif_create_default_wifi_ap();

        esp_netif_ip_info_t ip_info = {0};

        esp_netif_set_ip4_addr(&ip_info.ip, 10, 0, 0, 1);
        esp_netif_set_ip4_addr(&ip_info.gw, 10, 0, 0, 1);
        esp_netif_set_ip4_addr(&ip_info.netmask, 255, 255, 255, 0);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(netif_con));
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_ip_info(netif_con, &ip_info));
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(netif_con));
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t inst_any_id;
    esp_event_handler_instance_t ins_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &inst_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &ins_got_ip));

    wifi_config_t wifi_cfg = {0};
    if(!isSTA){
        // start AP
        wifi_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_cfg.ap.channel = 1;
        wifi_cfg.ap.pmf_cfg.required = true;
        wifi_cfg.ap.max_connection = 2;
        memcpy(wifi_cfg.ap.ssid, ssid, strlen((char*)ssid));
        memcpy(wifi_cfg.ap.password, pass, strlen((char*)pass));
        wifi_cfg.ap.ssid_len = strlen((char*)ssid);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));

        ESP_LOGD(TAG, "Making AP with SSID: %s and Password: %s", wifi_cfg.ap.ssid, wifi_cfg.ap.password);
        isConnected = true;
    } else {
        // connect to a user WiFi AP
        wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
        memcpy(wifi_cfg.sta.password, pass, strlen((char*)pass));
        memcpy(wifi_cfg.sta.ssid, ssid, strlen((char*)ssid));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

        ESP_LOGI(TAG, "Trying to connect to AP %s", wifi_cfg.sta.ssid);
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    if(isSTA){
        EventBits_t event = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if(event & WIFI_CONNECTED_BIT){
            ESP_LOGI(TAG, "Connected to AP %s", wifi_cfg.sta.ssid);
            isConnected = true;
        } else if(event & WIFI_FAIL_BIT) {
            ESP_LOGE(TAG, "Failed to connect to ap");
            isConnected = false;
            status_handler_wifi_set_state(false);
        }
    }

    esp_wifi_set_max_tx_power(66);
wh_finish:
    if(!isConnected){
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_wifi_deinit();
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_netif_destroy_default_wifi(netif_con);
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGE(TAG, "WiFi init failed.");
        wifi_handler_mode_t value = WIFI_HANDLER_MODE_DEFAULT_AP;
        xTaskCreate(wh_task, "wh_task", WIFI_HANDLER_TASK_SIZE, &value, 6, NULL);
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(start_mdns());
        ESP_LOGI(TAG, "WiFi init finished.");
        status_handler_wifi_set_state(true);
    }
    vTaskDelete(NULL);
}

bool wifi_is_connected(void)
{
    return isConnected;
}

wifi_mode_t wifi_get_mode(void)
{
    wifi_mode_t ret;
    esp_wifi_get_mode(&ret);
    return ret;
}

esp_err_t start_wifi(wifi_handler_mode_t mode)
{
    wifi_mode_t value = mode;
    xTaskCreate(wh_task, "wh_task", WIFI_HANDLER_TASK_SIZE, &value, 6, NULL);
    return ESP_OK;
}
