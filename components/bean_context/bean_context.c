#include "bean_context.h"
#include <esp_log.h>

/* Linker symbols from EMBED_FILES */
extern const uint8_t _binary_default_json_start[]; // start of bytes
extern const uint8_t _binary_default_json_end[]; // one past last byte

static const char *TAG = "BEAN_CONTEXT";
// Gets loaded with default at first & is overwritten with the stored conf.json
static cJSON *config   = NULL;

const cJSON *config_store_get(void)
{
    return config;
}

static config_merge_result_t merge_array_config(cJSON *default_obj, const cJSON *stored_item, const char *current_path)
{
    config_merge_result_t result = { 0 };

    cJSON *default_item = cJSON_GetObjectItem(default_obj, stored_item->string);
    if (!default_item || !cJSON_IsArray(default_item))
    {
        ESP_LOGW(TAG, "Default config doesn't have array '%s' - ignoring", current_path);
        result.items_ignored++;
        return result;
    }

    int stored_size  = cJSON_GetArraySize(stored_item);
    int default_size = cJSON_GetArraySize(default_item);

    // Determine array type from stored array (assume all elements same type)
    bool is_number_array = true;
    bool is_string_array = false;
    bool is_bool_array   = false;

    if (stored_size > 0)
    {
        cJSON *first_item = cJSON_GetArrayItem(stored_item, 0);
        is_number_array   = cJSON_IsNumber(first_item);
        is_string_array   = cJSON_IsString(first_item);
        is_bool_array     = cJSON_IsBool(first_item);
    }
    // If empty array, defaults to number array as specified

    // Validate all elements are the same type
    bool type_consistent = true;
    for (int i = 0; i < stored_size; i++)
    {
        cJSON *item = cJSON_GetArrayItem(stored_item, i);
        if ((is_number_array && !cJSON_IsNumber(item)) || (is_string_array && !cJSON_IsString(item)) ||
            (is_bool_array && !cJSON_IsBool(item)))
        {
            type_consistent = false;
            break;
        }
    }

    if (!type_consistent)
    {
        ESP_LOGW(TAG, "Array '%s' has inconsistent types - keeping default", current_path);
        result.type_mismatches++;
        return result;
    }

    // Check if array content has changed
    bool array_changed = (stored_size != default_size);

    if (!array_changed && stored_size > 0)
    {
        // Same size, check if values are different
        for (int i = 0; i < stored_size; i++)
        {
            cJSON *stored_elem  = cJSON_GetArrayItem(stored_item, i);
            cJSON *default_elem = cJSON_GetArrayItem(default_item, i);

            if (is_number_array)
            {
                if (cJSON_GetNumberValue(stored_elem) != cJSON_GetNumberValue(default_elem))
                {
                    array_changed = true;
                    break;
                }
            }
            else if (is_string_array)
            {
                if (strcmp(cJSON_GetStringValue(stored_elem), cJSON_GetStringValue(default_elem)) != 0)
                {
                    array_changed = true;
                    break;
                }
            }
            else if (is_bool_array)
            {
                if (cJSON_IsTrue(stored_elem) != cJSON_IsTrue(default_elem))
                {
                    array_changed = true;
                    break;
                }
            }
        }
    }

    if (array_changed)
    {
        // Replace the entire array
        cJSON *array_copy = cJSON_Duplicate(stored_item, 1);
        if (array_copy)
        {
            cJSON_ReplaceItemInObject(default_obj, stored_item->string, array_copy);
            result.config_changed = true;
            result.items_updated++;
            ESP_LOGI(TAG,
                     "Updated %s array config '%s' (size: %d -> %d)",
                     is_number_array ? "number" : (is_string_array ? "string" : "boolean"),
                     current_path,
                     default_size,
                     stored_size);
        }
    }

    return result;
}

static config_merge_result_t merge_json_recursive(cJSON *default_obj, const cJSON *stored_obj, const char *path)
{
    config_merge_result_t result = { 0 };

    if (!default_obj || !stored_obj)
    {
        return result;
    }

    const cJSON *stored_item = NULL;
    cJSON_ArrayForEach(stored_item, stored_obj)
    {
        if (!stored_item->string)
        {
            continue;
        }

        char current_path[256];
        snprintf(current_path, sizeof(current_path), "%s%s%s", path ? path : "", path ? "." : "", stored_item->string);

        cJSON *default_item = cJSON_GetObjectItem(default_obj, stored_item->string);

        if (!default_item)
        {
            ESP_LOGW(TAG, "Unknown config key '%s' - ignoring", current_path);
            result.items_ignored++;
            continue;
        }

        // Handle nested objects recursively
        if (cJSON_IsObject(stored_item) && cJSON_IsObject(default_item))
        {
            config_merge_result_t nested_result = merge_json_recursive(default_item, stored_item, current_path);
            result.config_changed |= nested_result.config_changed;
            result.needs_storage_update |= nested_result.needs_storage_update;
            result.items_updated += nested_result.items_updated;
            result.items_ignored += nested_result.items_ignored;
            result.type_mismatches += nested_result.type_mismatches;
        }
        // Handle arrays
        else if (cJSON_IsArray(stored_item) && cJSON_IsArray(default_item))
        {
            config_merge_result_t array_result = merge_array_config(default_obj, stored_item, current_path);
            result.config_changed |= array_result.config_changed;
            result.needs_storage_update |= array_result.needs_storage_update;
            result.items_updated += array_result.items_updated;
            result.items_ignored += array_result.items_ignored;
            result.type_mismatches += array_result.type_mismatches;
        }
        // Handle primitive types
        else if (cJSON_IsString(stored_item) && cJSON_IsString(default_item))
        {
            if (strcmp(cJSON_GetStringValue(stored_item), cJSON_GetStringValue(default_item)) != 0)
            {
                cJSON_SetValuestring(default_item, cJSON_GetStringValue(stored_item));
                result.config_changed = true;
                result.items_updated++;
                ESP_LOGI(TAG, "Updated string config '%s'", current_path);
            }
        }
        else if (cJSON_IsNumber(stored_item) && cJSON_IsNumber(default_item))
        {
            if (cJSON_GetNumberValue(stored_item) != cJSON_GetNumberValue(default_item))
            {
                cJSON_SetNumberValue(default_item, cJSON_GetNumberValue(stored_item));
                result.config_changed = true;
                result.items_updated++;
                ESP_LOGI(TAG, "Updated number config '%s'", current_path);
            }
        }
        else if (cJSON_IsBool(stored_item) && cJSON_IsBool(default_item))
        {
            if (cJSON_IsTrue(stored_item) != cJSON_IsTrue(default_item))
            {
                cJSON *new_bool = cJSON_IsTrue(stored_item) ? cJSON_CreateTrue() : cJSON_CreateFalse();
                cJSON_ReplaceItemInObject(default_obj, stored_item->string, new_bool);
                result.config_changed = true;
                result.items_updated++;
                ESP_LOGI(TAG, "Updated boolean config '%s'", current_path);
            }
        }
        else
        {
            ESP_LOGW(TAG, "Type mismatch for config key '%s' - keeping default", current_path);
            result.type_mismatches++;
        }
    }

    return result;
}

config_merge_result_t bean_context_initialize_config(cJSON *stored_config)
{

    config_merge_result_t result = { 0 };

    if (!config || !stored_config)
    {
        ESP_LOGW(TAG, "Cannot initialize config - missing default or stored config");
        return result;
    }

    result = merge_json_recursive(config, stored_config, NULL);

    if (result.config_changed || result.type_mismatches || result.items_ignored)
    {
        result.needs_storage_update = true;
        ESP_LOGI(TAG,
                 "Configuration merge complete: %d updated, %d ignored, %d type mismatches",
                 result.items_updated,
                 result.items_ignored,
                 result.type_mismatches);
    }
    else
    {
        ESP_LOGI(TAG, "No configuration changes needed");
    }

    return result;

    // overwrite all values that where stored in config
    // check if there were changes, if so storage will need to rewrite the file
    // types must be checked before overwriting
}

esp_err_t bean_context_init(bean_context_t **ctx)
{

    *ctx = (bean_context_t *)malloc(sizeof(bean_context_t));

    // Load the default.json in to the config
    size_t len = (size_t)(_binary_default_json_end - _binary_default_json_start);
    /* Parse directly from embedded bytes (may not be NUL-terminated) */
    config = cJSON_ParseWithLength((const char *)_binary_default_json_start, len);
    if (!config)
    {
        const char *err = cJSON_GetErrorPtr();

        ESP_LOGE(TAG, "Failed to parse embedded default.json: %s", err ? err : "(unknown)");
        return false;
    }
    ESP_LOGI(TAG, "default.json parsed (%u bytes)", (unsigned)len);

    (*ctx)->is_not_usb_msc = false;

    // Set default config, will be overwritten or used by bean_storage
    (*ctx)->config.data_logging_enabled  = true;
    (*ctx)->config.event_logging_enabled = true;

    if (!*ctx)
        return ESP_ERR_NO_MEM;

    (*ctx)->system_event_group = xEventGroupCreate();
    if (!(*ctx)->system_event_group)
        return ESP_ERR_NO_MEM;

    (*ctx)->event_queue = xQueueCreate(10, sizeof(event_data_t));
    if (!(*ctx)->event_queue)
        return ESP_ERR_NO_MEM;

    (*ctx)->data_log_queue = xQueueCreate(10, sizeof(log_data_t));
    if (!(*ctx)->data_log_queue)
        return ESP_ERR_NO_MEM;

    return ESP_OK;
}
