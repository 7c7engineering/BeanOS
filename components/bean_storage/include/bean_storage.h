#pragma once
#include "bean_context.h"
#include "esp_err.h"

#define BEAN_STORAGE_FLUSH_INTERVAL_MS 1000
#define STORAGE_BASE_PATH              "/extflash"

esp_err_t bean_storage_init(bean_context_t *ctx);
esp_err_t storage_write_file(char *filename, const char *data);
esp_err_t storage_list_files();
esp_err_t storage_read_file(char *filename);
esp_err_t storage_append_file(char *filename, const char *data);
esp_err_t storage_delete_file(char *filename);
esp_err_t storage_enable_usb_msc(void);
esp_err_t storage_get_file_size(char *filename, size_t *file_size);
esp_err_t storage_read_file_bytes(char *filename, uint8_t *buffer, size_t bytes_to_read, size_t offset);
