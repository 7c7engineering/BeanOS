#include "bean_storage_usb.h"

#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"

static const char *TAG = "BEAN_STORAGE_USB";

static bool usb_msc_enabled = false;
static tinyusb_msc_storage_handle_t storage_handle;

// USB MSC mount changed callback
static void storage_mount_changed_cb(tinyusb_msc_storage_handle_t handle, tinyusb_msc_event_t *event, void *arg)
{
    (void)handle;
    (void)arg;

    switch (event->id)
    {
    case TINYUSB_MSC_EVENT_MOUNT_START:
        ESP_LOGI(TAG, "Storage mount transition started");
        break;
    case TINYUSB_MSC_EVENT_MOUNT_COMPLETE:
        ESP_LOGI(TAG, "Storage mount transition completed");
        break;
    case TINYUSB_MSC_EVENT_MOUNT_FAILED:
        ESP_LOGW(TAG, "Storage mount transition failed");
        break;
    case TINYUSB_MSC_EVENT_FORMAT_REQUIRED:
        ESP_LOGW(TAG, "Storage requires FAT formatting");
        break;
    case TINYUSB_MSC_EVENT_FORMAT_FAILED:
        ESP_LOGW(TAG, "Storage format failed");
        break;
    default:
        break;
    }
}

esp_err_t bean_storage_usb_init(wl_handle_t s_wl_handle)
{

    // Initialize USB MSC with the external flash
    ESP_LOGI(TAG, "Initializing USB MSC");

    const tinyusb_msc_storage_config_t config_spi = {
        .medium.wl_handle = s_wl_handle,
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
        .fat_fs.base_path = STORAGE_BASE_PATH,
        .fat_fs.config.max_files = 5,
        .fat_fs.config.format_if_mount_failed = true,
        .fat_fs.config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };

    esp_err_t ret = tinyusb_msc_new_storage_spiflash(&config_spi, &storage_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize MSC storage: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = tinyusb_msc_set_storage_callback(storage_mount_changed_cb, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register MSC callback: %s", esp_err_to_name(ret));
        return ret;
    }

    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB MSC initialization complete");
    usb_msc_enabled = true;

    // Unmount application access of the external flash
    storage_expose_usb();

    return ESP_OK;
}

/**
 * @brief Expose the storage device to USB host for mass storage access
 * 
 * This function makes the storage device available to the connected USB host
 * (computer) as a mass storage device. Once exposed, the host can read/write
 * files directly to the storage, but the application loses access until
 * storage_hide_usb() is called.
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL if USB MSC not initialized
 * @return Other ESP error codes from underlying storage operations
 */
esp_err_t storage_expose_usb(void)
{
    if (!usb_msc_enabled)
    {
        ESP_LOGE(TAG, "USB MSC not initialized");
        return ESP_FAIL;
    }

    if (storage_handle == NULL)
    {
        ESP_LOGE(TAG, "USB storage handle is invalid");
        return ESP_FAIL;
    }

    tinyusb_msc_mount_point_t mount_point;
    esp_err_t ret = tinyusb_msc_get_storage_mount_point(storage_handle, &mount_point);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to query storage mount point: %s", esp_err_to_name(ret));
        return ret;
    }

    if (mount_point == TINYUSB_MSC_STORAGE_MOUNT_USB)
    {
        ESP_LOGW(TAG, "Storage already exposed over USB");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Exposing storage over USB...");
    ret = tinyusb_msc_set_storage_mount_point(storage_handle, TINYUSB_MSC_STORAGE_MOUNT_USB);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount storage for USB exposure: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Storage exposed over USB");
    return ESP_OK;
}

/**
 * @brief Hide the storage device from USB host and restore application access
 * 
 * This function removes the storage device from USB host access and remounts
 * it for application use. The storage will no longer appear as a drive on the
 * host computer, but the application can resume file operations.
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL if USB MSC not initialized
 * @return Other ESP error codes from underlying storage operations
 */
esp_err_t storage_hide_usb(void)
{
    if (!usb_msc_enabled)
    {
        ESP_LOGE(TAG, "USB MSC not initialized");
        return ESP_FAIL;
    }

    if (storage_handle == NULL)
    {
        ESP_LOGE(TAG, "USB storage handle is invalid");
        return ESP_FAIL;
    }

    tinyusb_msc_mount_point_t mount_point;
    esp_err_t ret = tinyusb_msc_get_storage_mount_point(storage_handle, &mount_point);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to query storage mount point: %s", esp_err_to_name(ret));
        return ret;
    }

    if (mount_point == TINYUSB_MSC_STORAGE_MOUNT_APP)
    {
        ESP_LOGW(TAG, "Storage not currently exposed over USB");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Hiding storage from USB...");
    ret = tinyusb_msc_set_storage_mount_point(storage_handle, TINYUSB_MSC_STORAGE_MOUNT_APP);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount storage after USB hide: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Storage hidden from USB and remounted for application use");
    return ESP_OK;
}

bool storage_is_exposed_usb(void)
{
    if (!usb_msc_enabled || storage_handle == NULL)
    {
        return false;
    }

    tinyusb_msc_mount_point_t mount_point;
    if (tinyusb_msc_get_storage_mount_point(storage_handle, &mount_point) != ESP_OK)
    {
        return false;
    }

    return mount_point == TINYUSB_MSC_STORAGE_MOUNT_USB;
}
