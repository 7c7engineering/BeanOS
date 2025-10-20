#include "bean_context.h"
#include "bean_storage.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "projdefs.h"
#include "sys/dirent.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/unistd.h>

static const char *TAG      = "BEAN_STORAGE_LOGGER";
static int log_number       = 0;
static FILE *data_log_file  = NULL;
static FILE *event_log_file = NULL;

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
    bool initialized = false, has_written = false;
    TickType_t last_sync_tick    = 0;
    uint16_t sync_tick_threshold = pdMS_TO_TICKS(BEAN_STORAGE_FLUSH_INTERVAL_MS);

    while (1)
    {
        if (xQueueReceive(ctx->data_log_queue, &received_data, sync_tick_threshold) == pdTRUE)
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
                setvbuf(data_log_file, NULL, _IOFBF, 8192 * 2); // Increase buffer size for speed
                fprintf(data_log_file, "timestamp,measurement_type,value\n"); // Write headers

                initialized = true;
            }

            if (data_log_file == NULL)
                continue;

            fprintf(data_log_file,
                    "%lu,%d,%s\n",
                    received_data.timestamp,
                    received_data.measurement_type,
                    received_data.measurement_value);
            has_written = true;
        }

        // Check if we need to sync (every 1 second and only if we've written something)
        if (has_written && data_log_file != NULL)
        {
            TickType_t current_tick = xTaskGetTickCount();
            uint16_t delta_ticks    = current_tick - last_sync_tick;
            if (delta_ticks >= sync_tick_threshold)
            {
                fflush(data_log_file);
                fsync(fileno(data_log_file));
                last_sync_tick = current_tick;
                has_written    = false; // Reset flag after sync
            }
        }
    }
}

void vtask_event_log_handler(void *pvParameter)
{
    bean_context_t *ctx = (bean_context_t *)pvParameter;
    event_data_t received_data;
    bool initialized = false, has_written = false;
    TickType_t last_sync_tick    = 0;
    uint16_t sync_tick_threshold = pdMS_TO_TICKS(BEAN_STORAGE_FLUSH_INTERVAL_MS);

    while (1)
    {
        if (xQueueReceive(ctx->event_queue, &received_data, sync_tick_threshold) == pdTRUE)
        {
            if (ctx->is_not_usb_msc)
            {
                if (!initialized)
                {
                    char file_name[13], full_path[23];
                    // Initialize data log file
                    sprintf(file_name, "log_e%03d.csv", log_number);
                    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, file_name);
                    event_log_file = fopen(full_path, "a");
                    setvbuf(event_log_file, NULL, _IOFBF, 8192 * 2); // Increase file buffer for speed
                    fprintf(event_log_file, "timestamp,event_id,event_data\n"); // Write headers

                    initialized = true;
                }

                if (event_log_file != NULL)
                {

                    fprintf(event_log_file,
                            "%lu,%d,%s\n",
                            received_data.timestamp,
                            received_data.event_id,
                            received_data.event_data ? received_data.event_data : "");
                    has_written = true;
                }
            }

            if (received_data.event_data != NULL)
                free(received_data.event_data);
        }

        // Check if we need to sync (every 1 second and only if we've written something)
        if (has_written && event_log_file != NULL)
        {
            TickType_t current_tick = xTaskGetTickCount();
            uint16_t delta_ticks    = current_tick - last_sync_tick;
            if (delta_ticks >= sync_tick_threshold)
            {
                fflush(event_log_file);
                fsync(fileno(event_log_file));
                last_sync_tick = current_tick;
                has_written    = false; // Reset flag after sync
            }
        }
    }
}
