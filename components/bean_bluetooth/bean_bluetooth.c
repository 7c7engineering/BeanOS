#include "esp_err.h"

#include "bean_bluetooth.h"
#include "bean_context.h"
#include "common.h"
#include "gap.h"
#include "gatt_svc.h"

/* Library function declarations */
void ble_store_config_init(void);

/* Private functions */
/*
 *  Stack event callback functions
 *      - on_stack_reset is called when host resets BLE stack due to errors
 *      - on_stack_sync is called when host has synced with controller
 */
static void on_stack_reset(int reason)
{
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void)
{
    /* On stack sync, do advertising initialization */
    adv_init();
}

static void nimble_host_config_init(void)
{
    /* Set host callbacks */
    ble_hs_cfg.reset_cb          = on_stack_reset;
    ble_hs_cfg.sync_cb           = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;

    /* Store host configuration */
    ble_store_config_init();
}

static void nimble_host_task(void *param)
{
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}

esp_err_t bean_bluetooth_init(bean_context_t *ctx)
{
    /* Local variables */
    int rc = 0;
    esp_err_t ret;

    /*
     * NVS flash initialization
     * Dependency of BLE stack to store configurations
     */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return ret;
    }

    /* NimBLE stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ", ret);
        return ret;
    }

    /* GAP service initialization */
    rc = gap_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return ESP_FAIL;
    }

    /* GATT server initialization */
    rc = gatt_svc_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", rc);
        return ESP_FAIL;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();

    /* Start NimBLE host task thread and return */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);

    return ESP_OK;
}
