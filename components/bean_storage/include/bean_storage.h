#pragma once
#include "bean_context.h"
#include "esp_err.h"

#define BEAN_STORAGE_FLUSH_INTERVAL_MS 1000
#define STORAGE_BASE_PATH              "/extflash"

typedef void (*storage_file_cb_t)(const char *name, size_t size, void *arg);

esp_err_t bean_storage_init(bean_context_t *ctx);
esp_err_t storage_write_file(char *filename, const char *data);
esp_err_t storage_list_files();
esp_err_t storage_iterate_files(storage_file_cb_t cb, void *arg);
esp_err_t storage_read_file(char *filename);
esp_err_t storage_append_file(char *filename, const char *data);
esp_err_t storage_delete_file(char *filename);
esp_err_t storage_save_config(void); // persist the live config store to conf.json
esp_err_t storage_enable_usb_msc(void);
