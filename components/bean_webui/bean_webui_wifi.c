#include <string.h>
#include "bean_webui_wifi.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"

// In "sta" mode: failed join attempts before the fallback AP is started
#define WEBUI_STA_FALLBACK_RETRIES 5
// In "apsta" mode: reconnect attempts before giving up on STA (AP keeps running)
#define WEBUI_STA_MAX_RETRIES 10

static const char *TAG = "BEAN_WEBUI_WIFI";

typedef enum
{
    WEBUI_WIFI_MODE_AP,    // access point only
    WEBUI_WIFI_MODE_APSTA, // AP always on + join configured network
    WEBUI_WIFI_MODE_STA,   // join configured network; fallback AP if it fails
} webui_wifi_mode_t;

static webui_wifi_mode_t wifi_mode = WEBUI_WIFI_MODE_APSTA;
static wifi_config_t ap_config; // kept for the "sta" fallback path
static int sta_retries          = 0;
static bool ap_fallback_started = false;

static const char *cfg_str(const cJSON *obj, const char *key, const char *fallback)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    return cJSON_IsString(item) ? item->valuestring : fallback;
}

static int cfg_int(const cJSON *obj, const char *key, int fallback)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    return cJSON_IsNumber(item) ? (int)cJSON_GetNumberValue(item) : fallback;
}

// The configured network is unreachable: bring up the AP so the device
// stays accessible in the field
static void start_ap_fallback(void)
{
    if (ap_fallback_started)
        return;
    ap_fallback_started = true;
    ESP_LOGW(TAG, "Configured network not found, starting fallback access point \"%s\"", (char *)ap_config.ap.ssid);
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK || esp_wifi_set_config(WIFI_IF_AP, &ap_config) != ESP_OK)
        ESP_LOGE(TAG, "Failed to start fallback access point");
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                sta_retries++;
                if (wifi_mode == WEBUI_WIFI_MODE_STA && sta_retries >= WEBUI_STA_FALLBACK_RETRIES)
                    start_ap_fallback(); // keeps retrying STA below, AP now covers access
                if (sta_retries < WEBUI_STA_MAX_RETRIES)
                {
                    ESP_LOGW(TAG, "STA disconnected, reconnect attempt %d/%d", sta_retries, WEBUI_STA_MAX_RETRIES);
                    esp_wifi_connect();
                }
                else
                {
                    ESP_LOGW(TAG, "STA gave up after %d attempts, continuing AP-only", WEBUI_STA_MAX_RETRIES);
                    if (wifi_mode == WEBUI_WIFI_MODE_STA)
                        start_ap_fallback();
                }
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "Client joined the access point");
                break;
            default:
                break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        sta_retries              = 0;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static esp_err_t mdns_start(const char *hostname)
{
    ESP_RETURN_ON_ERROR(mdns_init(), TAG, "mDNS init failed");
    ESP_RETURN_ON_ERROR(mdns_hostname_set(hostname), TAG, "mDNS hostname failed");
    mdns_instance_name_set("Skybean flight logger");
    ESP_RETURN_ON_ERROR(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0), TAG, "mDNS service failed");
    ESP_LOGI(TAG, "mDNS responder started: http://%s.local", hostname);
    return ESP_OK;
}

static webui_wifi_mode_t parse_mode(const cJSON *wifi_cfg, const char *sta_ssid)
{
    const char *mode = cfg_str(wifi_cfg, "mode", "apsta");
    webui_wifi_mode_t parsed;
    if (strcmp(mode, "ap") == 0)
        parsed = WEBUI_WIFI_MODE_AP;
    else if (strcmp(mode, "sta") == 0)
        parsed = WEBUI_WIFI_MODE_STA;
    else if (strcmp(mode, "apsta") == 0)
        parsed = WEBUI_WIFI_MODE_APSTA;
    else
    {
        ESP_LOGW(TAG, "Unknown wifi mode \"%s\", using apsta", mode);
        parsed = WEBUI_WIFI_MODE_APSTA;
    }

    if (parsed != WEBUI_WIFI_MODE_AP && strlen(sta_ssid) == 0)
    {
        ESP_LOGW(TAG, "wifi mode \"%s\" needs a station SSID, falling back to AP-only", mode);
        parsed = WEBUI_WIFI_MODE_AP;
    }
    return parsed;
}

static void build_ap_config(const cJSON *ap_cfg)
{
    ap_config = (wifi_config_t){
        .ap = { .channel        = cfg_int(ap_cfg, "channel", 6),
                .max_connection = cfg_int(ap_cfg, "max_connections", 2),
                .authmode       = WIFI_AUTH_WPA2_PSK },
    };
    const char *ap_ssid = cfg_str(ap_cfg, "ssid", "");
    if (strlen(ap_ssid) == 0)
    {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
        snprintf((char *)ap_config.ap.ssid, sizeof(ap_config.ap.ssid), "Skybean-%02X%02X", mac[4], mac[5]);
    }
    else
    {
        strlcpy((char *)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid));
    }
    ap_config.ap.ssid_len = strlen((char *)ap_config.ap.ssid);

    const char *ap_password = cfg_str(ap_cfg, "password", "");
    if (strlen(ap_password) < 8)
    {
        ESP_LOGW(TAG, "AP password shorter than 8 characters, the access point will be OPEN");
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    else
    {
        strlcpy((char *)ap_config.ap.password, ap_password, sizeof(ap_config.ap.password));
    }
}

esp_err_t bean_webui_wifi_init(const cJSON *webui_cfg)
{
    const cJSON *wifi_cfg = cJSON_GetObjectItem(webui_cfg, "wifi");
    const cJSON *sta_cfg  = cJSON_GetObjectItem(wifi_cfg, "sta");
    const char *sta_ssid  = cfg_str(sta_cfg, "ssid", "");

    wifi_mode = parse_mode(wifi_cfg, sta_ssid);
    build_ap_config(cJSON_GetObjectItem(wifi_cfg, "ap"));

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    // Create both netifs regardless of mode so the "sta" fallback AP can start later
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_cfg), TAG, "wifi init failed");
    // conf.json is the source of truth for credentials, don't shadow them in NVS
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage failed");

    ESP_RETURN_ON_ERROR(
      esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG, "event reg failed");
    ESP_RETURN_ON_ERROR(
      esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL), TAG, "event reg failed");

    wifi_mode_t esp_mode = (wifi_mode == WEBUI_WIFI_MODE_AP)    ? WIFI_MODE_AP
                           : (wifi_mode == WEBUI_WIFI_MODE_STA) ? WIFI_MODE_STA
                                                                : WIFI_MODE_APSTA;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(esp_mode), TAG, "wifi set mode failed");

    if (wifi_mode != WEBUI_WIFI_MODE_STA)
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config), TAG, "wifi AP config failed");

    if (wifi_mode != WEBUI_WIFI_MODE_AP)
    {
        wifi_config_t sta = { 0 };
        strlcpy((char *)sta.sta.ssid, sta_ssid, sizeof(sta.sta.ssid));
        strlcpy((char *)sta.sta.password, cfg_str(sta_cfg, "password", ""), sizeof(sta.sta.password));
        // STA join failures must never take down the AP, so errors only log
        if (esp_wifi_set_config(WIFI_IF_STA, &sta) != ESP_OK)
            ESP_LOGW(TAG, "STA config invalid");
    }

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
    if (wifi_mode != WEBUI_WIFI_MODE_STA)
        ESP_LOGI(TAG, "Access point \"%s\" started, web UI at http://192.168.4.1", (char *)ap_config.ap.ssid);
    if (wifi_mode != WEBUI_WIFI_MODE_AP)
        ESP_LOGI(TAG, "Joining \"%s\" as station%s", sta_ssid,
                 wifi_mode == WEBUI_WIFI_MODE_STA ? " (AP fallback if it is not found)" : "");

    if (mdns_start(cfg_str(webui_cfg, "mdns_hostname", "skybean")) != ESP_OK)
        ESP_LOGW(TAG, "mDNS failed to start, device reachable by IP only");

    return ESP_OK;
}

esp_err_t bean_webui_wifi_stop(void)
{
    mdns_free();
    return esp_wifi_stop();
}
