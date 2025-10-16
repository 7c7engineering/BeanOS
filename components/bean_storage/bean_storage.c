#include "bean_storage.h"
#include "bean_context.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "bean_storage_usb.h"
#include "sys/dirent.h"
#include "bean_storage_logger.h"

#define HOST_ID      SPI2_HOST //SPI3_HOST
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO

static esp_flash_t *flash;
const char *partition_label = "storage";

static const char *TAG         = "BEAN_STORAGE";
const char *base_path          = STORAGE_BASE_PATH;
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

TaskHandle_t storage_data_logger_task_handle, storage_event_logger_task_handle;

static esp_flash_t *init_ext_flash(void)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num   = PIN_FLASH_MOSI,
        .miso_io_num   = PIN_FLASH_MISO,
        .sclk_io_num   = PIN_FLASH_CLK,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
    };

    const esp_flash_spi_device_config_t device_config = {
        .host_id   = HOST_ID,
        .cs_id     = 0,
        .cs_io_num = PIN_FLASH_CS,
        .io_mode   = SPI_FLASH_DIO,
        .freq_mhz  = 40,
    };

    ESP_LOGI(TAG, "Initializing external SPI Flash");
    ESP_LOGI(TAG, "Pin assignments:");
    ESP_LOGI(TAG,
             "MOSI: %2d   MISO: %2d   SCLK: %2d   CS: %2d",
             bus_config.mosi_io_num,
             bus_config.miso_io_num,
             bus_config.sclk_io_num,
             device_config.cs_io_num);

    // Initialize the SPI bus
    ESP_LOGI(TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);
    ESP_ERROR_CHECK(spi_bus_initialize(HOST_ID, &bus_config, SPI_DMA_CHAN));

    // Add device to the SPI bus
    esp_flash_t *ext_flash;
    ESP_ERROR_CHECK(spi_bus_add_flash_device(&ext_flash, &device_config));

    // Probe the Flash chip and initialize it
    esp_err_t err = esp_flash_init(ext_flash);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize external Flash: %s (0x%x)", esp_err_to_name(err), err);
        return NULL;
    }

    // Print out the ID and size
    uint32_t id;
    ESP_ERROR_CHECK(esp_flash_read_id(ext_flash, &id));
    ESP_LOGI(TAG, "Initialized external Flash, size=%" PRIu32 " KB, ID=0x%" PRIx32, ext_flash->size / 1024, id);

    return ext_flash;
}

static const esp_partition_t *add_partition(esp_flash_t *ext_flash, const char *partition_label)
{
    ESP_LOGI(TAG,
             "Adding external Flash as a partition, label=\"%s\", size=%" PRIu32 " KB",
             partition_label,
             ext_flash->size / 1024);
    const esp_partition_t *fat_partition;
    const size_t offset = 0;
    ESP_ERROR_CHECK(esp_partition_register_external(ext_flash,
                                                    offset,
                                                    ext_flash->size,
                                                    partition_label,
                                                    ESP_PARTITION_TYPE_DATA,
                                                    ESP_PARTITION_SUBTYPE_DATA_FAT,
                                                    &fat_partition));

    // Erase space of partition on the external flash chip
    //ESP_LOGI(TAG, "Erasing partition range, offset=%u size=%" PRIu32 " KB", offset, ext_flash->size / 1024);
    //ESP_ERROR_CHECK(esp_partition_erase_range(fat_partition, offset, ext_flash->size));
    return fat_partition;
}

static bool mount_fatfs(const char *partition_label)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    const esp_vfs_fat_mount_config_t mount_config = { .max_files              = 4,
                                                      .format_if_mount_failed = true,
                                                      .allocation_unit_size   = CONFIG_WL_SECTOR_SIZE };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, partition_label, &mount_config, &s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return false;
    }
    return true;
}

esp_err_t bean_storage_init(bean_context_t *ctx)
{
    // Set up SPI bus and initialize the external SPI Flash chip
    flash = init_ext_flash();
    if (flash == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize external Flash");
        return ESP_FAIL;
    }

    add_partition(flash, partition_label);

    if (!mount_fatfs(partition_label))
    {
        ESP_LOGE(TAG, "Failed to mount FATFS");
        return ESP_FAIL;
    }

    if (bean_storage_logger_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize storage logger");
        return ESP_FAIL;
    }

    xTaskCreate(&vtask_data_log_handler,
                "data_log_handler",
                4096,
                (void *)ctx,
                tskIDLE_PRIORITY,
                &storage_data_logger_task_handle);
    xTaskCreate(&vtask_event_log_handler,
                "event_log_handler",
                4096,
                (void *)ctx,
                tskIDLE_PRIORITY,
                &storage_event_logger_task_handle);
    return ESP_OK;
}

esp_err_t storage_write_file(char *filename, const char *data)
{
    char *abs_filename = malloc(strlen(base_path) + strlen(filename) + 2);
    strcpy(abs_filename, base_path);
    strcat(abs_filename, "/");
    strcat(abs_filename, filename);
    FILE *f = fopen(abs_filename, "wb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        free(abs_filename);
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    free(abs_filename);
    return ESP_OK;
}

esp_err_t storage_append_file(char *filename, const char *data)
{
    char *abs_filename = malloc(strlen(base_path) + strlen(filename) + 2);
    strcpy(abs_filename, base_path);
    strcat(abs_filename, "/");
    strcat(abs_filename, filename);
    FILE *f = fopen(abs_filename, "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing (append))");
        free(abs_filename);
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    free(abs_filename);
    return ESP_OK;
}

esp_err_t storage_list_files()
{
    DIR *dir;
    struct dirent *pDirent;
    dir = opendir(base_path);
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Failed to open directory");
        return ESP_FAIL;
    }

    while ((pDirent = readdir(dir)) != NULL)
    {
        ESP_LOGI(TAG, "File: %s", pDirent->d_name);
    }
    closedir(dir);
    return ESP_OK;
}

esp_err_t storage_delete_file(char *filename)
{
    char *abs_filename = malloc(strlen(base_path) + strlen(filename) + 2);
    strcpy(abs_filename, base_path);
    strcat(abs_filename, "/");
    strcat(abs_filename, filename);
    if (unlink(abs_filename) != 0)
    {
        ESP_LOGE(TAG, "Failed to delete file");
        free(abs_filename);
        return ESP_FAIL;
    }
    free(abs_filename);
    return ESP_OK;
}

esp_err_t storage_read_file(char *filename)
{
    char *abs_filename = malloc(strlen(base_path) + strlen(filename) + 2);
    strcpy(abs_filename, base_path);
    strcat(abs_filename, "/");
    strcat(abs_filename, filename);
    FILE *f = fopen(abs_filename, "rb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        free(abs_filename);
        return ESP_FAIL;
    }
    char data[64];
    while (fgets(data, 64, f) != NULL)
    {
        printf("%s", data);
    }
    fclose(f);
    return ESP_OK;
}

esp_err_t storage_get_file_size(char *filename, size_t *file_size)
{
    char *abs_filename = malloc(strlen(base_path) + strlen(filename) + 2);
    strcpy(abs_filename, base_path);
    strcat(abs_filename, "/");
    strcat(abs_filename, filename);

    FILE *f = fopen(abs_filename, "rb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for size check");
        free(abs_filename);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    *file_size = ftell(f);
    fclose(f);
    free(abs_filename);
    return ESP_OK;
}

esp_err_t storage_read_file_bytes(char *filename, uint8_t *buffer, size_t bytes_to_read, size_t offset)
{
    char *abs_filename = malloc(strlen(base_path) + strlen(filename) + 2);
    strcpy(abs_filename, base_path);
    strcat(abs_filename, "/");
    strcat(abs_filename, filename);

    FILE *f = fopen(abs_filename, "rb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading bytes");
        free(abs_filename);
        return ESP_FAIL;
    }

    if (fseek(f, offset, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "Failed to seek to offset %zu", offset);
        fclose(f);
        free(abs_filename);
        return ESP_FAIL;
    }

    size_t bytes_read = fread(buffer, 1, bytes_to_read, f);
    if (bytes_read != bytes_to_read)
    {
        ESP_LOGW(TAG, "Read %zu bytes instead of requested %zu bytes", bytes_read, bytes_to_read);
    }

    fclose(f);
    free(abs_filename);
    return ESP_OK;
}

esp_err_t storage_enable_usb_msc(void)
{
    ESP_LOGI(TAG, "Enabling USB MSC");
    bean_storage_usb_init(s_wl_handle);
    return ESP_OK;
}
