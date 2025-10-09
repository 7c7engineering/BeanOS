#include "bean_context.h"
#include "bean_storage.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "sys/dirent.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/unistd.h>
#include "esp_timer.h"

static const char *TAG     = "BEAN_STORAGE_LOGGER";
static int log_number      = 0;
static FILE *data_log_file = NULL;
// todo add event_log_file here

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

        char lowercase_name[100];
        strcpy(lowercase_name, entry->d_name);
        for (int i = 0; lowercase_name[i]; i++)
        {
            lowercase_name[i] = tolower(lowercase_name[i]);
        }
        ESP_LOGI(TAG, "Found file: %s", lowercase_name);
        if (strstr(lowercase_name, "log_") == lowercase_name && strstr(lowercase_name, ".csv") != NULL)
        {

            int num;
            if (sscanf(lowercase_name, "log_%*c%03d", &num) == 1)
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

esp_err_t append_log_data_to_log_file(const log_data_t *data)
{
    if (data_log_file == NULL)
    {
        return ESP_FAIL;
    }

    // Append the data
    fprintf(data_log_file, "%lu,%d,%ld\n", data->timestamp, data->measurement_type, data->measurement_value);

    // Flush to ensure data is written (but keep file open)
    fflush(data_log_file); // flushing can be done periodically to improve speed
    fsync(fileno(data_log_file)); // Forces write to storage device

    return ESP_OK;
}

void write_performance_test(size_t bytes_per_write, int num_writes, int num_persist_writes)
{
    if (data_log_file == NULL)
    {
        ESP_LOGE(TAG, "Failed to start performance test - file not open");
        return;
    }

    // Create test data buffer
    char *test_buffer = malloc(bytes_per_write);
    if (test_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate test buffer of %zu bytes", bytes_per_write);
        return;
    }

    // Fill buffer with test data
    for (size_t i = 0; i < bytes_per_write; i++)
    {
        test_buffer[i] = '0' + (i % 10);
    }
    // Ensure last character is newline for readability
    if (bytes_per_write > 0)
    {
        test_buffer[bytes_per_write - 1] = '\n';
    }

    ESP_LOGI(TAG, "Starting performance test: %d writes of %zu bytes each", num_writes, bytes_per_write);

    // Get start time
    int64_t start_time = esp_timer_get_time();

    for (int i = 0; i < num_writes; i++)
    {
        fwrite(test_buffer, 1, bytes_per_write, data_log_file);

        if (i % num_persist_writes == 0)
        {
            fflush(data_log_file);
            fsync(fileno(data_log_file));
            ESP_LOGI(TAG, "Progress: %d/%d writes completed", i, num_writes);
        }
    }
    fflush(data_log_file);
    fsync(fileno(data_log_file));

    // Calculate results
    int64_t end_time      = esp_timer_get_time();
    int64_t total_time_us = end_time - start_time;
    size_t total_bytes    = bytes_per_write * num_writes;

    ESP_LOGI(TAG, "Performance test completed: %u bytes per line - write every %u lines", bytes_per_write, num_persist_writes);
    ESP_LOGI(TAG, "Total bytes written: %zu", total_bytes);
    ESP_LOGI(TAG, "Total time: %lld us (%.2f ms)", total_time_us, total_time_us / 1000.0);
    ESP_LOGI(TAG, "Average time per write: %lld us", total_time_us / num_writes);
    ESP_LOGI(TAG, "Throughput: %.2f KB/s", (total_bytes * 1000000.0) / (total_time_us * 1024.0));

    free(test_buffer);
}

esp_err_t bean_storage_logger_init()
{

    log_number = get_next_log_number();
    ESP_LOGI(TAG, "Using log_number %u", log_number);

    return ESP_OK;
}

void vtask_data_log_handler(void *pvParameter)
{
    bean_context_t *ctx = (bean_context_t *)pvParameter;
    log_data_t received_data;
    bool initialized = false;

    while (1)
    {
        if (xQueueReceive(ctx->data_log_queue, &received_data, portMAX_DELAY) == pdTRUE)
        {
            if (!ctx->is_not_usb_msc)
                continue;

            if (!initialized)
            {
                char file_name[13], full_path[23];
                // Initialize data log file
                sprintf(file_name, "log_d%03d.csv", log_number);
                snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, file_name);
                data_log_file = fopen(full_path, "a");

                write_performance_test(128, 1024, 128);
                initialized = true;
            }
            if (append_log_data_to_log_file(&received_data) != ESP_OK)
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
