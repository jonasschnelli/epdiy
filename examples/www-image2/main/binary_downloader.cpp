#include "esp_crt_bundle.h"/**
 * @file binary_downloader.cpp
 * @brief Implementation of BinaryDownloader class
 */
#include "binary_downloader.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

#include <inttypes.h>

#include <algorithm>

static const char* TAG = "BinaryDownloader";

// Structure to hold download data during HTTP client operation
typedef struct {
    uint8_t* buffer;
    size_t buffer_capacity;
    size_t data_size;
    int content_length;
    uint32_t last_progress_report;
    DownloadResult* result;
    bool failed;
    std::function <void(int)> progress_cb;
} download_context_t;

BinaryDownloader::BinaryDownloader() {
    // Initialize any resources needed (none currently)
}

BinaryDownloader::~BinaryDownloader() {
    // Clean up any resources (none currently)
}

DownloadResult BinaryDownloader::download(const std::string& url, std::function<void(int)> progress_cb, int timeout_ms) {
    DownloadResult result;
    download_context_t context = {};
    context.buffer = nullptr;
    context.buffer_capacity = 0;
    context.data_size = 0;
    context.content_length = -1;
    context.last_progress_report = 0;
    context.result = &result;
    context.failed = false;
    context.progress_cb = progress_cb;

    // Initialize an HTTP client configuration
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = timeout_ms;
    config.event_handler = httpEventHandler;
    config.user_data = &context;
    config.buffer_size = 4096;  // Optimize buffer size for performance
    config.buffer_size_tx = 1024;
    config.crt_bundle_attach = esp_crt_bundle_attach; // Use global CA bundle
    config.keep_alive_enable = true;
    config.disable_auto_redirect = false;
    config.max_redirection_count = 5;
    

    // Initialize the HTTP client
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        result.success = false;
        result.error_msg = "Failed to initialize HTTP client";
        return result;
    }

    // Set important headers
    esp_http_client_set_header(client, "Accept", "*/*");

    // Perform the request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        result.status_code = esp_http_client_get_status_code(client);
        
        // Check if status code indicates success
        if (result.status_code >= 200 && result.status_code < 300) {
            result.success = !context.failed;
            result.size = context.data_size;
            result.buffer = context.buffer;
        } else {
            // Status code indicates failure
            result.success = false;
            result.error_msg = "HTTP error: " + std::to_string(result.status_code);
            
            // Free buffer if allocated
            if (context.buffer != nullptr) {
                heap_caps_free(context.buffer);
                context.buffer = nullptr;
            }
        }
    } else {
        // HTTP client error occurred
        result.success = false;
        result.error_msg = "HTTP client error: " + std::string(esp_err_to_name(err));
        
        // Free buffer if allocated
        if (context.buffer != nullptr) {
            heap_caps_free(context.buffer);
            context.buffer = nullptr;
        }
    }

    // Clean up the client
    esp_http_client_cleanup(client);
    
    // Log download completion
    if (result.success) {
        ESP_LOGI(TAG, "Download completed successfully: %zu bytes", result.size);
    } else {
        ESP_LOGW(TAG, "Download failed: %s", result.error_msg.c_str());
    }
    
    return result;
}

esp_err_t BinaryDownloader::httpEventHandler(esp_http_client_event_t* evt) {
    download_context_t* context = static_cast<download_context_t*>(evt->user_data);
    
    esp_task_wdt_reset();
    switch (evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
            // For debugging
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER: %s: %s", evt->header_key, evt->header_value);
            break;
            
        case HTTP_EVENT_ON_DATA:
            if (!context->failed && evt->data_len > 0) {
                // Check if we need to allocate or expand the buffer
                if (context->buffer == nullptr) {
                    // First data chunk - make initial estimate of file size
                    context->content_length = esp_http_client_get_content_length(evt->client);
                    
                    // Log the expected file size
                    if (context->content_length > 0) {
                        ESP_LOGI(TAG, "Starting download, content length: %d bytes", context->content_length);
                    } else {
                        ESP_LOGI(TAG, "Starting download, content length unknown");
                    }
                    
                    // If content length is unknown, allocate an initial buffer
                    size_t initial_size = (context->content_length > 0) ? 
                        context->content_length : ((evt->data_len * 4 > 16384) ? evt->data_len * 4 : 16384);
                    
                    // Allocate in PSRAM
                    context->buffer = static_cast<uint8_t*>(heap_caps_malloc(
                        initial_size, MALLOC_CAP_SPIRAM));
                    
                    if (context->buffer == nullptr) {
                        ESP_LOGE(TAG, "Failed to allocate initial buffer of size %zu", initial_size);
                        context->failed = true;
                        return ESP_FAIL;
                    }
                    
                    context->buffer_capacity = initial_size;
                } else if (context->data_size + evt->data_len > context->buffer_capacity) {
                    // Need to expand the buffer
                    size_t new_capacity = context->buffer_capacity * 2;
                    // Ensure we have enough space for the new data
                    if (new_capacity < context->data_size + evt->data_len) {
                        new_capacity = context->data_size + evt->data_len + 8192; // Add some extra
                    }
                    
                    ESP_LOGI(TAG, "Expanding buffer from %zu to %zu bytes", 
                        context->buffer_capacity, new_capacity);
                    
                    uint8_t* new_buffer = static_cast<uint8_t*>(heap_caps_realloc(
                        context->buffer, new_capacity, MALLOC_CAP_SPIRAM));
                    
                    if (new_buffer == nullptr) {
                        ESP_LOGE(TAG, "Failed to reallocate buffer to size %zu", new_capacity);
                        heap_caps_free(context->buffer);
                        context->buffer = nullptr;
                        context->failed = true;
                        return ESP_FAIL;
                    }
                    
                    context->buffer = new_buffer;
                    context->buffer_capacity = new_capacity;
                }
                
                // Copy new data to buffer
                memcpy(context->buffer + context->data_size, evt->data, evt->data_len);
                context->data_size += evt->data_len;
                
                // Report progress every ~10% or at least every 512KB
                if (context->content_length > 0) {
                    uint32_t progress_percent = context->data_size * 100 / context->content_length;
                    if (progress_percent > context->last_progress_report + 10 || 
                        context->data_size - context->last_progress_report * context->content_length / 100 >= 524288) {
                        context->progress_cb(progress_percent);
                        ESP_LOGI(TAG, "Download progress: %u%% (%zu/%d bytes)", 
                            progress_percent, context->data_size, context->content_length);
                        context->last_progress_report = progress_percent / 10 * 10; // Round to nearest 10%
                    }
                } else {
                    // If content length is unknown, report every 512KB
                    if (context->data_size / 524288 > context->last_progress_report) {
                        ESP_LOGI(TAG, "Download progress: %zu KB received", context->data_size / 1024);
                        context->last_progress_report = context->data_size / 524288;
                    }
                }
            }
            break;
            
        case HTTP_EVENT_ON_FINISH:
            // Nothing special to do on finish
            break;
            
        case HTTP_EVENT_DISCONNECTED:
            // In newer ESP-IDF versions, we should handle disconnect events differently
            // as the error_handle member might not exist
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            
            // Mark as failed if it was a sudden disconnect during transfer
            if (context->buffer != nullptr && context->data_size > 0 && !context->failed) {
                context->failed = true;
                
                // Set error message in result
                if (context->result) {
                    context->result->error_msg = "Connection terminated unexpectedly";
                }
            }
            break;
            
        default:
            break;
    }
    
    return ESP_OK;
}


/**
 * Returns a string representation of the file format.
 * 
 * @param format The FileFormat enum value
 * @return A constant string representing the format ("PNG", "JPEG", or "UNKNOWN")
 */
const char* BinaryDownloader::GetFormatName(FileFormat format) {
    switch (format) {
        case FILE_FORMAT_PNG:
            return "PNG";
        case FILE_FORMAT_JPEG:
            return "JPEG";
        case FILE_FORMAT_UNKNOWN:
        default:
            return "UNKNOWN";
    }
}


/**
 * Determines if a buffer contains a PNG, JPEG, or unknown file format.
 * 
 * @param buffer Pointer to the beginning of the file data
 * @param bufferSize Size of the buffer in bytes
 * @return The detected file format (PNG, JPEG, or UNKNOWN)
 */
FileFormat BinaryDownloader::DetectImageFormatFromBuffer(const uint8_t *buffer, size_t bufferSize) {
    // Check if buffer is too small to contain any valid signature
    if (buffer == NULL || bufferSize < 8) {
        return FILE_FORMAT_UNKNOWN;
    }

    // Check for PNG signature
    // PNG files start with an 8-byte signature: 89 50 4E 47 0D 0A 1A 0A
    if (bufferSize >= 8 && 
        buffer[0] == 0x89 && 
        buffer[1] == 0x50 && // P
        buffer[2] == 0x4E && // N
        buffer[3] == 0x47 && // G
        buffer[4] == 0x0D && // CR
        buffer[5] == 0x0A && // LF
        buffer[6] == 0x1A && 
        buffer[7] == 0x0A) {
        return FILE_FORMAT_PNG;
    }

    // Check for JPEG signature
    // JPEG files start with FF D8 FF
    if (bufferSize >= 3 &&
        buffer[0] == 0xFF && 
        buffer[1] == 0xD8 && 
        buffer[2] == 0xFF) {
        return FILE_FORMAT_JPEG;
    }

    // No known signatures detected
    return FILE_FORMAT_UNKNOWN;
}
