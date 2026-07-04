#include <ctype.h>
#include <string.h>
#include "bean_webui_http.h"
#include "bean_webui_ws.h"
#include "bean_battery.h"
#include "bean_beep.h"
#include "bean_bits.h"
#include "bean_led.h"
#include "bean_storage.h"
#include "bean_storage_logger.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#define WEBUI_HTTPD_STACK    8192
#define WEBUI_DL_CHUNK       2048
#define WEBUI_MAX_LOG_NAME   32

static const char *TAG = "BEAN_WEBUI_HTTP";

static httpd_handle_t server = NULL;
static bean_context_t *bean_ctx;

extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_html_gz_end[] asm("_binary_index_html_gz_end");

// The UI can also run from file:// during development and call this API
// cross-origin, so every /api response must carry the CORS header (see api.md)
static void set_cors(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
}

static esp_err_t h_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, (const char *)index_html_gz_start, index_html_gz_end - index_html_gz_start);
}

// CORS preflight for POST/DELETE requests from a locally-opened UI
static esp_err_t h_options(httpd_req_t *req)
{
    set_cors(req);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t send_status_json(httpd_req_t *req)
{
    set_cors(req);
    EventBits_t bits = xEventGroupGetBits(bean_ctx->system_event_group);
    bool armed       = (bits & BEAN_SYSTEM_ARMED) != 0;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    cJSON_AddBoolToObject(root, "armed", armed);
    // Placeholder until the flight-state machine exists; the key is stable
    cJSON_AddStringToObject(root, "flight_state", armed ? "armed" : "pre_launch");
    cJSON_AddNumberToObject(root, "battery_mv", bean_battery_get_voltage_mv());
    cJSON_AddBoolToObject(root, "usb_powered", (bits & BEAN_SYSTEM_USB_POWERED) != 0);
    cJSON_AddBoolToObject(root, "charging", (bits & BEAN_SYSTEM_BATTERY_CHARGING) != 0);
    cJSON_AddNumberToObject(root, "uptime_ms", (double)(esp_timer_get_time() / 1000));
    cJSON_AddStringToObject(root, "fw_version", esp_app_get_description()->version);
    cJSON_AddStringToObject(root, "idf_version", esp_get_idf_version());
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

static esp_err_t h_status(httpd_req_t *req)
{
    return send_status_json(req);
}

// Arm/disarm must be observable on the device itself, not only in the UI:
// LED2 shows the arm state, the buzzer confirms, and the event log records it.
static void arm_feedback(bool armed_now)
{
    bean_led_set_color(LED_L2, armed_now ? (led_color_rgb_t){ 255, 255, 0 } : (led_color_rgb_t){ 0, 0, 0 });
    bean_beep_sound(armed_now ? NOTE_A5 : NOTE_E4, 150);

    event_data_t evt = { .event_id   = armed_now ? BEAN_EVENT_ARMED : BEAN_EVENT_DISARMED,
                         .timestamp  = esp_log_timestamp(),
                         .event_data = strdup(armed_now ? "armed via web UI" : "disarmed via web UI") };
    if (xQueueSend(bean_ctx->event_queue, &evt, pdMS_TO_TICKS(100)) != pdPASS)
    {
        free(evt.event_data);
        ESP_LOGW(TAG, "Failed to enqueue arm event for the event log");
    }
}

static esp_err_t h_arm(httpd_req_t *req)
{
    xEventGroupSetBits(bean_ctx->system_event_group, BEAN_SYSTEM_ARMED);
    ESP_LOGI(TAG, "Armed via web UI");
    arm_feedback(true);
    return send_status_json(req);
}

static esp_err_t h_disarm(httpd_req_t *req)
{
    xEventGroupClearBits(bean_ctx->system_event_group, BEAN_SYSTEM_ARMED);
    ESP_LOGI(TAG, "Disarmed via web UI");
    arm_feedback(false);
    return send_status_json(req);
}

static void logs_list_cb(const char *name, size_t size, void *arg)
{
    cJSON *files = (cJSON *)arg;
    if (strncmp(name, "log_", 4) != 0)
        return;
    const char *ext = strrchr(name, '.');
    if (ext == NULL || strcasecmp(ext, ".csv") != 0)
        return;
    cJSON *file = cJSON_CreateObject();
    if (file == NULL)
        return;
    cJSON_AddStringToObject(file, "name", name);
    cJSON_AddNumberToObject(file, "size", size);
    cJSON_AddBoolToObject(file, "active", storage_logger_file_in_use(name));
    cJSON_AddItemToArray(files, file);
}

static esp_err_t h_logs_list(httpd_req_t *req)
{
    set_cors(req);
    cJSON *root  = cJSON_CreateObject();
    cJSON *files = cJSON_AddArrayToObject(root, "files");
    if (files == NULL)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }

    if (storage_iterate_files(logs_list_cb, files) != ESP_OK)
    {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "storage unavailable");
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

static esp_err_t h_config_get(httpd_req_t *req)
{
    set_cors(req);
    const cJSON *config = config_store_get();
    char *json          = config ? cJSON_PrintUnformatted(config) : NULL;
    if (json == NULL)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no config");
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

#define WEBUI_CONFIG_BODY_MAX 4096

// Deep-merges the posted JSON into the config store (unknown keys are ignored,
// type mismatches keep the current value) and persists it to conf.json.
static esp_err_t h_config_post(httpd_req_t *req)
{
    set_cors(req);
    if (req->content_len == 0 || req->content_len > WEBUI_CONFIG_BODY_MAX)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body missing or too large");

    char *body = malloc(req->content_len + 1);
    if (body == NULL)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");

    size_t received = 0;
    while (received < req->content_len)
    {
        int ret = httpd_req_recv(req, body + received, req->content_len - received);
        if (ret <= 0)
        {
            free(body);
            return ESP_FAIL;
        }
        received += ret;
    }
    body[received] = '\0';

    cJSON *incoming = cJSON_Parse(body);
    free(body);
    if (incoming == NULL)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");

    config_merge_result_t result = bean_context_initialize_config(incoming);
    cJSON_Delete(incoming); // merge copies values, never keeps references

    if (result.config_changed && storage_save_config() != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to persist config");
    ESP_LOGI(TAG,
             "Config updated via web UI: %d updated, %d ignored, %d type mismatches",
             result.items_updated,
             result.items_ignored,
             result.type_mismatches);

    char resp[96];
    snprintf(resp,
             sizeof(resp),
             "{\"updated\":%d,\"ignored\":%d,\"type_mismatches\":%d}",
             result.items_updated,
             result.items_ignored,
             result.type_mismatches);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static void reboot_timer_cb(void *arg)
{
    esp_restart();
}

// Applies saved WiFi settings in the field without a power cycle
static esp_err_t h_reboot(httpd_req_t *req)
{
    set_cors(req);
    ESP_LOGI(TAG, "Reboot requested via web UI");
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, "{\"rebooting\":true}", HTTPD_RESP_USE_STRLEN);

    // Give the response time to flush before restarting
    const esp_timer_create_args_t timer_args = { .callback = reboot_timer_cb, .name = "webui_reboot" };
    esp_timer_handle_t timer;
    if (esp_timer_create(&timer_args, &timer) == ESP_OK)
        esp_timer_start_once(timer, 500 * 1000);
    return ret;
}

// Rejects anything but a plain filename: no paths, no traversal, no hidden files
static esp_err_t get_validated_name(httpd_req_t *req, char *name, size_t name_size)
{
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
        return ESP_FAIL;
    if (httpd_query_key_value(query, "name", name, name_size) != ESP_OK)
        return ESP_FAIL;
    size_t len = strlen(name);
    if (len == 0 || len >= WEBUI_MAX_LOG_NAME || name[0] == '.')
        return ESP_FAIL;
    for (size_t i = 0; i < len; i++)
    {
        char c = name[i];
        if (!isalnum((unsigned char)c) && c != '.' && c != '_' && c != '-')
            return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t h_log_download(httpd_req_t *req)
{
    set_cors(req);
    char name[WEBUI_MAX_LOG_NAME];
    if (get_validated_name(req, name, sizeof(name)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name");

    char path[sizeof(STORAGE_BASE_PATH) + WEBUI_MAX_LOG_NAME];
    snprintf(path, sizeof(path), "%s/%s", STORAGE_BASE_PATH, name);
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no such log");

    char disposition[WEBUI_MAX_LOG_NAME + 32];
    snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", name);
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);

    char *buf = malloc(WEBUI_DL_CHUNK);
    if (buf == NULL)
    {
        fclose(f);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }

    esp_err_t ret = ESP_OK;
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, WEBUI_DL_CHUNK, f)) > 0)
    {
        ret = httpd_resp_send_chunk(req, buf, read_bytes);
        if (ret != ESP_OK) // client aborted
            break;
    }
    fclose(f);
    free(buf);
    if (ret != ESP_OK)
        return ret;
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t h_log_delete(httpd_req_t *req)
{
    set_cors(req);
    char name[WEBUI_MAX_LOG_NAME];
    if (get_validated_name(req, name, sizeof(name)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name");

    if (storage_logger_file_in_use(name))
    {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"log file is active\"}", HTTPD_RESP_USE_STRLEN);
    }

    if (storage_delete_file(name) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no such log");

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"deleted\":true}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t bean_webui_http_init(bean_context_t *ctx)
{
    bean_ctx = ctx;

    httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
    config.stack_size       = WEBUI_HTTPD_STACK;
    config.max_open_sockets = 6;
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;
    config.uri_match_fn     = httpd_uri_match_wildcard; // exact URIs still match; needed for OPTIONS /api/*

    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "httpd start failed");

    const httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = h_index },
        { .uri = "/api/status", .method = HTTP_GET, .handler = h_status },
        { .uri = "/api/arm", .method = HTTP_POST, .handler = h_arm },
        { .uri = "/api/disarm", .method = HTTP_POST, .handler = h_disarm },
        { .uri = "/api/logs", .method = HTTP_GET, .handler = h_logs_list },
        { .uri = "/api/logs/download", .method = HTTP_GET, .handler = h_log_download },
        { .uri = "/api/logs", .method = HTTP_DELETE, .handler = h_log_delete },
        { .uri = "/api/config", .method = HTTP_GET, .handler = h_config_get },
        { .uri = "/api/config", .method = HTTP_POST, .handler = h_config_post },
        { .uri = "/api/reboot", .method = HTTP_POST, .handler = h_reboot },
        { .uri = "/api/*", .method = HTTP_OPTIONS, .handler = h_options },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &uris[i]), TAG, "uri register failed");

    ESP_RETURN_ON_ERROR(bean_webui_ws_register(server), TAG, "ws register failed");

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return ESP_OK;
}
