#include "esp_err.h"

void vtask_data_log_handler(void *pvParameter);
void vtask_event_log_handler(void *pvParameter);
esp_err_t bean_storage_logger_init();
void bean_storage_get_current_data_filename(char *filename);
