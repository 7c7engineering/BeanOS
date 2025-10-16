/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "gatt_svc.h"
#include "common.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "host/ble_hs.h"

static uint16_t ctrl_tx_handle, ctrl_rx_handle;
static uint16_t data_tx_handle, data_rx_handle;

// 2b771fcc-c87d-42bd-9216-000000000000
static const ble_uuid128_t SVC_UUID =
  BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x92, 0xbd, 0x42, 0x7d, 0xc8, 0xcc, 0x1f, 0x77, 0x2b);

static uint16_t ctrl_tx_chr_val_handle;
// 2b771fcc-c87d-42bd-9216-000000000001
static const ble_uuid128_t CTRL_TX_UUID =
  BLE_UUID128_INIT(0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x92, 0xbd, 0x42, 0x7d, 0xc8, 0xcc, 0x1f, 0x77, 0x2b);
// 2b771fcc-c87d-42bd-9216-000000000002
static const ble_uuid128_t CTRL_RX_UUID =
  BLE_UUID128_INIT(0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x92, 0xbd, 0x42, 0x7d, 0xc8, 0xcc, 0x1f, 0x77, 0x2b);

// Minimal placeholder access_cb: returns empty value on READ, discards data on WRITE.
static int noop_rw_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
    case BLE_GATT_ACCESS_OP_READ_DSC:
        // Return empty value (OK). If you want a fixed value, append it:
        // const char *val = "OK";
        // if (os_mbuf_append(ctxt->om, val, 2) != 0) return BLE_ATT_ERR_INSUFFICIENT_RES;
        return 0;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
    case BLE_GATT_ACCESS_OP_WRITE_DSC:
    {
        // Discard incoming bytes (don’t free ctxt->om; NimBLE owns it).
        // If you want to inspect them:
        // int len = OS_MBUF_PKTLEN(ctxt->om);
        // uint8_t buf[32]; int n = MIN(len, (int)sizeof buf);
        // os_mbuf_copydata(ctxt->om, 0, n, buf);
        return 0;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static int om_to_flat(struct os_mbuf *om, uint8_t **out_buf, uint16_t *out_len)
{
    uint32_t len32 = OS_MBUF_PKTLEN(om);
    if (len32 > 0xFFFF) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    uint16_t len = (uint16_t)len32;
    uint8_t *buf = (uint8_t *)malloc(len ? len : 1); // avoid malloc(0)
    if (!buf) return BLE_ATT_ERR_INSUFFICIENT_RES;

    int rc = os_mbuf_copydata(om, 0, len, buf);
    if (rc != 0) { free(buf); return BLE_ATT_ERR_UNLIKELY; }

    *out_buf = buf;
    *out_len = len;
    return 0;
}

static int ctrl_rx_cb(uint16_t conn, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t  *buf = NULL;
    uint16_t  len = 0;

    int rc = om_to_flat(ctxt->om, &buf, &len);
    if (rc != 0) {
        ESP_LOGW(TAG, "CTRL_RX: failed to copy data rc=%d", rc);
        return rc; // Return ATT error so the client knows write failed (for Write With Response).
    }

    // Interpret as a UTF-8 command line (trim CR/LF). If you have a binary framing, parse it here instead.
    // Ensure null-termination for logging/processing convenience:
    char *cmd = (char *)malloc(len + 1);
    if (!cmd) { free(buf); return BLE_ATT_ERR_INSUFFICIENT_RES; }
    memcpy(cmd, buf, len);
    cmd[len] = '\0';

    // Optional: trim trailing newline(s)
    while (len > 0 && (cmd[len-1] == '\n' || cmd[len-1] == '\r')) { cmd[--len] = '\0'; }

    ESP_LOGI(TAG, "CTRL_RX: got command: '%s' (len=%u)", cmd, (unsigned)len);

    // TODO: dispatch to your command handler here
    // e.g., handle_command(cmd, len);

    // Example response: echo back the command and status over CTRL_TX
    // (You can return binary, JSON, etc.)
    {
        // Compose "OK <cmd>\n"
        const char *prefix = "OK ";
        const char *suffix = "\n";
        size_t out_len = strlen(prefix) + len + strlen(suffix);
        char *out = (char *)malloc(out_len);
        if (out) {
            memcpy(out, prefix, strlen(prefix));
            memcpy(out + strlen(prefix), cmd, len);
            memcpy(out + strlen(prefix) + len, suffix, strlen(suffix));

            struct os_mbuf *om = ble_hs_mbuf_from_flat(out, (uint16_t)out_len);
            if (om) {
                int nrc = ble_gatts_notify_custom(conn, ctrl_tx_chr_val_handle, om);
                if (nrc != 0) {
                    ESP_LOGW(TAG, "notify CTRL_TX failed rc=%d", nrc);
                    os_mbuf_free_chain(om); // Usually freed by NimBLE if success, but guard on failure
                }
            } else {
                ESP_LOGW(TAG, "alloc mbuf for notify failed");
            }
            free(out);
        }
    }

    free(cmd);
    free(buf);
    return 0; // success
}

static const struct ble_gatt_svc_def gatt_svcs[] = {

    { .type            = BLE_GATT_SVC_TYPE_PRIMARY,
      .uuid            = &SVC_UUID.u,
      .characteristics = (struct ble_gatt_chr_def[]){ { .uuid       = &CTRL_TX_UUID.u,
                                                        .access_cb  = noop_rw_cb,
                                                        .flags      = BLE_GATT_CHR_F_NOTIFY,
                                                        .val_handle = &ctrl_tx_chr_val_handle },
                                                      {
                                                        .uuid      = &CTRL_RX_UUID.u,
                                                        .access_cb = ctrl_rx_cb,
                                                        .flags     = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE,
                                                      },
                                                      {
                                                        0, /* No more characteristics in this service. */
                                                      } } },
    { 0 }
};
/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op)
    {

    /* Service register event */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(
          TAG, "registered service %s with handle=%d", ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
        break;

    /* Characteristic register event */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle,
                 ctxt->chr.val_handle);
        break;

    /* Descriptor register event */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG,
                 "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    /* Unknown event */
    default:
        assert(0);
        break;
    }
}

/*
 *  GATT server subscribe event callback
 */

void gatt_svr_subscribe_cb(struct ble_gap_event *event)
{
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
        ESP_LOGI(TAG,
                 "subscribe event; conn_handle=%d attr_handle=%ld",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle);
    }
    else
    {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d", event->subscribe.attr_handle);
    }

    if (event->subscribe.attr_handle == ctrl_tx_chr_val_handle) {
        ESP_LOGI(TAG, "!!! Received custom event");
    }
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void)
{
    /* Local variables */
    int rc = 0;

    /* 1. GATT service initialization */
    ble_svc_gatt_init();

    ESP_LOGI(TAG, "init success");

    /* 2. Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0)
    {
        return rc;
    }

    ESP_LOGI(TAG, "count config success");
    /* 3. Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0)
    {
        return rc;
    }

    return 0;
}
