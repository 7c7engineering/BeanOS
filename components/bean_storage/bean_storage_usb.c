#include "bean_storage_usb.h"

#include "device/usbd.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"

static const char *TAG = "BEAN_STORAGE_USB";

// USB MSC descriptors
#define EPNUM_MSC           1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum
{
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum
{
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,
    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

static tusb_desc_device_t descriptor_config = { .bLength            = sizeof(descriptor_config),
                                                .bDescriptorType    = TUSB_DESC_DEVICE,
                                                .bcdUSB             = 0x0200,
                                                .bDeviceClass       = TUSB_CLASS_MISC,
                                                .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
                                                .bDeviceProtocol    = MISC_PROTOCOL_IAD,
                                                .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
                                                .idVendor           = 0x303A, // Espressif VID
                                                .idProduct          = 0x4002,
                                                .bcdDevice          = 0x100,
                                                .iManufacturer      = 0x01,
                                                .iProduct           = 0x02,
                                                .iSerialNumber      = 0x03,
                                                .bNumConfigurations = 0x01 };

static uint8_t const msc_fs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

static char const *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 }, // 0: is supported language is English (0x0409)
    "Bean Device", // 1: Manufacturer
    "Bean Storage", // 2: Product
    "123456", // 3: Serials
    "Bean MSC", // 4. MSC
};

static bool usb_msc_enabled = false;

// USB MSC mount changed callback
static void storage_mount_changed_cb(tinyusb_msc_event_t *event)
{
    if (event->mount_changed_data.is_mounted)
    {
        ESP_LOGI(TAG, "Storage mounted to application");
    }
    else
    {
        ESP_LOGI(TAG, "Storage unmounted from application - exposing to USB");
        // Could automatically expose here, but this might cause issues
    }
    ESP_LOGI(TAG, "Storage mounted to application: %s", event->mount_changed_data.is_mounted ? "Yes" : "No");
}

esp_err_t bean_storage_usb_init(wl_handle_t s_wl_handle)
{

    // Initialize USB MSC with the external flash
    ESP_LOGI(TAG, "Initializing USB MSC");

    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle              = s_wl_handle,
        .callback_mount_changed = storage_mount_changed_cb,
        .mount_config.max_files = 5,
    };

    esp_err_t ret = tinyusb_msc_storage_init_spiflash(&config_spi);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize MSC storage: %s", esp_err_to_name(ret));
        return ret;
    }

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = &descriptor_config,
        .string_descriptor        = string_desc_arr,
        .string_descriptor_count  = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy             = false,
        .configuration_descriptor = msc_fs_configuration_desc,
    };

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

    if (tinyusb_msc_storage_in_use_by_usb_host())
    {
        ESP_LOGW(TAG, "Storage already exposed over USB");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Exposing storage over USB...");
    esp_err_t ret = tinyusb_msc_storage_unmount();
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

    if (!tinyusb_msc_storage_in_use_by_usb_host())
    {
        ESP_LOGW(TAG, "Storage not currently exposed over USB");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Hiding storage from USB...");
    esp_err_t ret = tinyusb_msc_storage_mount(STORAGE_BASE_PATH);
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
    if (!usb_msc_enabled)
    {
        return false;
    }
    return tinyusb_msc_storage_in_use_by_usb_host();
}
