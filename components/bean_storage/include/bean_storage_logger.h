#include <stdbool.h>
#include "esp_err.h"

void vtask_data_log_handler(void *pvParameter);
void vtask_event_log_handler(void *pvParameter);
esp_err_t bean_storage_logger_init();
bool storage_logger_file_in_use(const char *filename);
