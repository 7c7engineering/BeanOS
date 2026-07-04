#include "bean_webui.h"
#include "bean_webui_http.h"
#include "bean_webui_sampler.h"
#include "bean_webui_wifi.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "BEAN_WEBUI";

static bool disable_on_takeoff = false;

esp_err_t bean_webui_init(bean_context_t *ctx)
{
    const cJSON *config    = config_store_get();
    const cJSON *webui_cfg = config ? cJSON_GetObjectItem(config, "bean_webui") : NULL;
    if (webui_cfg == NULL)
    {
        ESP_LOGW(TAG, "No bean_webui config found, web UI disabled");
        return ESP_OK;
    }

    const cJSON *enabled = cJSON_GetObjectItem(webui_cfg, "enabled");
    if (cJSON_IsBool(enabled) && !cJSON_IsTrue(enabled))
    {
        ESP_LOGI(TAG, "Web UI disabled by configuration");
        return ESP_OK;
    }

    const cJSON *wifi_cfg = cJSON_GetObjectItem(webui_cfg, "wifi");
    const cJSON *dot      = cJSON_GetObjectItem(wifi_cfg, "disable_on_takeoff");
    disable_on_takeoff    = cJSON_IsBool(dot) && cJSON_IsTrue(dot);

    ESP_RETURN_ON_ERROR(bean_webui_wifi_init(webui_cfg), TAG, "WiFi init failed");
    ESP_RETURN_ON_ERROR(bean_webui_http_init(ctx), TAG, "HTTP server init failed");

    const cJSON *hz = cJSON_GetObjectItem(webui_cfg, "sensor_update_hz");
    ESP_RETURN_ON_ERROR(bean_webui_sampler_start(ctx, cJSON_IsNumber(hz) ? (int)cJSON_GetNumberValue(hz) : 5),
                        TAG,
                        "Sampler start failed");

    ESP_LOGI(TAG, "Web UI up");
    return ESP_OK;
}

void bean_webui_notify_takeoff(void)
{
    if (!disable_on_takeoff)
        return;
    ESP_LOGI(TAG, "Takeoff: shutting down WiFi and sampler");
    bean_webui_sampler_stop();
    bean_webui_wifi_stop();
}
