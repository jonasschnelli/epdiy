/**
 * @file binary_downloader.h
 * @brief Header file for BinaryDownloader class
 */
#pragma once

#include <functional>
#include <string>
#include <cstdint>
#include <memory>

#include "esp_http_client.h"
#include "esp_heap_caps.h"

/**
 * @struct DownloadResult
 * @brief Holds the result of a download operation
 */
struct DownloadResult {
    uint8_t* buffer;         ///< Pointer to downloaded data (allocated in PSRAM)
    size_t size;             ///< Size of downloaded data in bytes
    int status_code;         ///< HTTP status code
    bool success;            ///< Flag indicating if download was successful
    std::string error_msg;   ///< Error message if download failed

    /**
     * @brief Constructor to initialize with default values
     */
    DownloadResult() : 
        buffer(nullptr), 
        size(0), 
        status_code(0),
        success(false) {}

    /**
     * @brief Destructor to free buffer memory
     */
    ~DownloadResult() {
        if (buffer != nullptr) {
            heap_caps_free(buffer);
            buffer = nullptr;
        }
    }

    /**
     * @brief Move constructor
     */
    DownloadResult(DownloadResult&& other) noexcept :
        buffer(other.buffer),
        size(other.size),
        status_code(other.status_code),
        success(other.success),
        error_msg(std::move(other.error_msg)) {
        other.buffer = nullptr;  // Prevent double-free
    }

    /**
     * @brief Move assignment operator
     */
    DownloadResult& operator=(DownloadResult&& other) noexcept {
        if (this != &other) {
            if (buffer != nullptr) {
                heap_caps_free(buffer);
            }
            buffer = other.buffer;
            size = other.size;
            status_code = other.status_code;
            success = other.success;
            error_msg = std::move(other.error_msg);
            other.buffer = nullptr;  // Prevent double-free
        }
        return *this;
    }

    // Delete copy constructor and assignment to prevent accidental copies
    DownloadResult(const DownloadResult&) = delete;
    DownloadResult& operator=(const DownloadResult&) = delete;
};

typedef enum {
    FILE_FORMAT_UNKNOWN,
    FILE_FORMAT_PNG,
    FILE_FORMAT_JPEG
} FileFormat;

/**
 * @class BinaryDownloader
 * @brief Class for downloading binary data via HTTP/HTTPS
 */
class BinaryDownloader {
public:
    /**
     * @brief Constructor
     */
    BinaryDownloader();

    /**
     * @brief Destructor
     */
    ~BinaryDownloader();

    /**
     * @brief Download content from a URL
     * @param url The URL to download from
     * @param progress_cb Progress callback function
     * @param timeout_ms HTTP request timeout in milliseconds (default: 30000)
     * @return DownloadResult containing downloaded data and metadata
     */
    DownloadResult download(const std::string& url, std::function<void(int)> progress_cb, int timeout_ms = 30000);

    static FileFormat DetectImageFormatFromBuffer(const uint8_t *buffer, size_t bufferSize);
    static const char* GetFormatName(FileFormat format);

private:
    /**
     * @brief HTTP event handler
     * @param evt HTTP client event
     * @return ESP_OK if successful
     */
    static esp_err_t httpEventHandler(esp_http_client_event_t* evt);
};