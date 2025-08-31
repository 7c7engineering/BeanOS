#pragma once
#include "esp_err.h"

esp_err_t bean_storage_init(void);
esp_err_t storage_write_file(char* filename, const char* data);
esp_err_t storage_list_files();
esp_err_t storage_read_file(char* filename);
esp_err_t storage_append_file(char* filename, const char* data);
esp_err_t storage_delete_file(char* filename);
