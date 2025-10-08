#include "bean_context.h"
#include "bean_storage.h"
#include "esp_log.h"
#include "portmacro.h"
#include "sys/dirent.h"
#include <ctype.h>
#include <sys/unistd.h>

static const char *TAG            = "BEAN_STORAGE_DATA_LOG";
static char log_file_name[32]     = "LOG_0000.CSV";
static FILE *log_file             = NULL;
static char current_log_path[256] = { 0 };

int get_next_log_number(void)
{
    DIR *dir = opendir(STORAGE_BASE_PATH);
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Failed to open directory");
        return 0; // Start with 1 if directory can't be opened
    }

    struct dirent *entry;
    int highest_num = 0;

    while ((entry = readdir(dir)) != NULL)
    {

        char lowercase_name[256];
        strcpy(lowercase_name, entry->d_name);
        for (int i = 0; lowercase_name[i]; i++)
        {
            lowercase_name[i] = tolower(lowercase_name[i]);
        }
        ESP_LOGI(TAG, "Found file: %s", lowercase_name);
        if (strstr(lowercase_name, "log_") == lowercase_name && strstr(lowercase_name, ".csv") != NULL)
        {

            int num;
            if (sscanf(lowercase_name, "log_%04d.csv", &num) == 1)
            {
                if (num > highest_num)
                {
                    highest_num = num;
                }
            }
        }
    }
    closedir(dir);

    return highest_num + 1; // Return next available number
}

esp_err_t append_to_log_file(const char *file_path, const log_data_t *data)
{
    // Check if we need to open a new file or if path changed
    if (log_file == NULL || strcmp(current_log_path, file_path) != 0)
    {
        // Close existing file if open
        if (log_file != NULL)
        {
            fclose(log_file);
            log_file = NULL;
        }

        // Open new file in append mode
        log_file = fopen(file_path, "a");
        if (log_file == NULL)
        {
            ESP_LOGE(TAG, "Failed to open log file: %s", file_path);
            return ESP_FAIL;
        }

        // Store current path
        strncpy(current_log_path, file_path, sizeof(current_log_path) - 1);
        current_log_path[sizeof(current_log_path) - 1] = '\0';

        // Write CSV header if file is new
        fseek(log_file, 0, SEEK_END);
        if (ftell(log_file) == 0)
        {
            fprintf(log_file, "timestamp,measurement_type,value\n");
        }

        ESP_LOGI(TAG, "Opened log file: %s", file_path);
    }

    // Append the data
    fprintf(log_file, "%lu,%d,%ld\n", data->timestamp, data->measurement_type, data->measurement_value);

    // Flush to ensure data is written (but keep file open)
    fflush(log_file); // flushing can be done periodically to improve speed
    fsync(fileno(log_file)); // Forces write to storage device

    return ESP_OK;
}

void vtask_data_log_handler(void *pvParameter)
{
    bean_context_t *ctx = (bean_context_t *)pvParameter;

    int log_number = get_next_log_number();
    ESP_LOGI(TAG, "Using log_number %u", log_number);
    sprintf(log_file_name, "log_%04d.csv", log_number);
    ESP_LOGI(TAG, "Using log file name: %s", log_file_name);

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, log_file_name);

    log_data_t received_data;

    while (1)
    {
        if (xQueueReceive(ctx->data_log_queue, &received_data, portMAX_DELAY) == pdTRUE)
        {
            if (append_to_log_file(full_path, &received_data) != ESP_OK)
            {
                ESP_LOGE(TAG,
                         "Failed to log data: type=%d, timestamp=%lu, value=%ld",
                         received_data.measurement_type,
                         received_data.timestamp,
                         received_data.measurement_value);
            }
        }
    }
}
