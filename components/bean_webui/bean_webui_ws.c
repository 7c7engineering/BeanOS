#include <string.h>
#include "bean_webui_ws.h"
#include "esp_log.h"

// Must cover httpd max_open_sockets (6) set in bean_webui_http.c
#define WEBUI_WS_SCAN_FDS 8

static const char *TAG = "BEAN_WEBUI_WS";

static httpd_handle_t server_handle = NULL;

typedef struct
{
    int fd;
    size_t len;
    char data[];
} ws_async_send_t;

// In IDF 6 the /ws uri handler is NOT invoked on handshake (only for incoming
// data frames), so clients cannot be tracked by registration. Instead httpd
// itself is the source of truth: scan its client list for handshaked sockets.
static size_t ws_client_fds(int *fds, size_t max)
{
    int all[WEBUI_WS_SCAN_FDS];
    size_t count = WEBUI_WS_SCAN_FDS;
    if (server_handle == NULL || httpd_get_client_list(server_handle, &count, all) != ESP_OK)
        return 0;

    size_t n = 0;
    for (size_t i = 0; i < count && n < max; i++)
    {
        if (httpd_ws_get_fd_info(server_handle, all[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
            fds[n++] = all[i];
    }
    return n;
}

bool bean_webui_ws_has_clients(void)
{
    int fds[WEBUI_WS_SCAN_FDS];
    return ws_client_fds(fds, WEBUI_WS_SCAN_FDS) > 0;
}

// Runs in the httpd task via httpd_queue_work, the safe context for socket writes
static void ws_async_send(void *arg)
{
    ws_async_send_t *msg = (ws_async_send_t *)arg;
    if (httpd_ws_get_fd_info(server_handle, msg->fd) == HTTPD_WS_CLIENT_WEBSOCKET)
    {
        httpd_ws_frame_t frame = { .type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)msg->data, .len = msg->len };
        esp_err_t err          = httpd_ws_send_frame_async(server_handle, msg->fd, &frame);
        if (err != ESP_OK)
        {
            // httpd reaps the dead session itself; the client just vanishes from the scan
            ESP_LOGW(TAG, "Send to fd %d failed (%s)", msg->fd, esp_err_to_name(err));
        }
    }
    free(msg);
}

void bean_webui_ws_broadcast(const char *json, size_t len)
{
    int fds[WEBUI_WS_SCAN_FDS];
    size_t n = ws_client_fds(fds, WEBUI_WS_SCAN_FDS);

    for (size_t i = 0; i < n; i++)
    {
        ws_async_send_t *msg = malloc(sizeof(ws_async_send_t) + len);
        if (msg == NULL)
            return;
        msg->fd  = fds[i];
        msg->len = len;
        memcpy(msg->data, json, len);
        esp_err_t err = httpd_queue_work(server_handle, ws_async_send, msg);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "httpd_queue_work failed (%s), frame dropped", esp_err_to_name(err));
            free(msg);
        }
    }
}

// Only called for incoming data frames; telemetry is push-only and commands go
// over REST, so drain and ignore (control frames are handled by httpd itself)
static esp_err_t ws_handler(httpd_req_t *req)
{
    httpd_ws_frame_t frame = { 0 };
    esp_err_t ret          = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK)
        return ret;
    if (frame.len > 0)
    {
        uint8_t *buf = malloc(frame.len);
        if (buf == NULL)
            return ESP_ERR_NO_MEM;
        frame.payload = buf;
        ret           = httpd_ws_recv_frame(req, &frame, frame.len);
        free(buf);
    }
    return ret;
}

esp_err_t bean_webui_ws_register(httpd_handle_t server)
{
    server_handle = server;

    const httpd_uri_t ws_uri = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .is_websocket = true,
    };
    return httpd_register_uri_handler(server, &ws_uri);
}
