#include <stdio.h>
#include "binary_downloader.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "epd_highlevel.h"
#include "epdiy.h"

#include "JPEGDEC.h"



#include "firasans_20.h"

EpdiyHighlevelState hl;
JPEGDEC jpeg;

// Configurable download URL
#define DOWNLOAD_URL ""

// WiFi credentials - update these for your network
#define WIFI_SSID ""
#define WIFI_PASS ""

// WiFi connection timeout (30 seconds)
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RETRY_DELAY_MS 1000
#define MAX_WIFI_RETRY_ATTEMPTS 5

static const char* TAG = "MAIN";

#define DEMO_BOARD epd_board_v7_raw

// Event group for WiFi events
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

static int wifi_retry_num = 0;

// JPEG drawing callback
// Display supports 4-bit grayscale (16 levels: 0-15)
// Framebuffer format: 2 pixels per byte, 4 bits per pixel
int JPEGDraw(JPEGDRAW *pDraw)
{
    int x, y;
    uint8_t *s, *d;
    uint8_t *pBuffer = epd_hl_get_framebuffer(&hl);
    int iPitch = epd_width() / 2;  // 2 pixels per byte for 4-bit grayscale
    
    ESP_LOGD(TAG, "Drawing JPEG block: x=%d, y=%d, w=%d, h=%d", 
             pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
    
    for (y = 0; y < pDraw->iHeight; y++) {
        // Calculate destination pointer in framebuffer
        d = &pBuffer[((pDraw->y + y) * iPitch) + (pDraw->x / 2)];
        
        // Source pointer to current line in JPEG data
        s = (uint8_t *)pDraw->pPixels;
        s += (y * pDraw->iWidth);
        
        // Process pixels in pairs (2 pixels per byte for 4-bit)
        for (x = 0; x < pDraw->iWidth; x += 2) {
            // Pack two 8-bit grayscale pixels into one byte (4-bit each)
            // Take upper 4 bits of each pixel for 4-bit grayscale
            uint8_t pixel1 = s[0] & 0xF0;        // Upper 4 bits of first pixel
            uint8_t pixel2 = (s[1] >> 4) & 0x0F; // Upper 4 bits of second pixel
            *d++ = pixel1 | pixel2;
            s += 2;
        }
    }
    return 1; // Continue drawing
}

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < MAX_WIFI_RETRY_ATTEMPTS) {
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(TAG, "Retry connecting to WiFi... (attempt %d/%d)", wifi_retry_num, MAX_WIFI_RETRY_ATTEMPTS);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", MAX_WIFI_RETRY_ATTEMPTS);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void display_status_message(const char* message) {
    int temperature = 25;
    uint8_t* fb = epd_hl_get_framebuffer(&hl);
    EpdFontProperties font_props = epd_font_properties_default();
    font_props.flags = EPD_DRAW_ALIGN_CENTER;

    epd_poweron();

    int width = 500;
    int height = 100;

    int x = epd_width() / 2;
    int y = epd_height() / 2 - height / 2;
    EpdRect border = {
        .x = x - width / 2,
        .y = y - height / 2,
        .width = width,
        .height = height,
    };

    epd_fill_rect(border, 0xFF, fb);
    epd_draw_rect(border, 0x0, fb);

    y += 30;
    enum EpdDrawError text_err = epd_write_string(
        &FiraSans_20, message, &x, &y, fb, &font_props
    );
    
    if (text_err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Failed to write text to EPD: %d", text_err);
    }

    
    enum EpdDrawError update_err = epd_hl_update_area(&hl, MODE_DU, epd_ambient_temperature(), border);
    if (update_err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Failed to update EPD screen: %d", update_err);
    }
    
    epd_poweroff();
}

void idf_update() {
    display_status_message("Initializing system...");
}

void idf_setup() {
    epd_init(&DEMO_BOARD, &ED052TC4, EPD_LUT_64K);
    epd_set_vcom(1560);
    hl = epd_hl_init(EPD_BUILTIN_WAVEFORM);
    
    epd_poweron();
    epd_clear();
    epd_poweroff();
}

extern "C" {
    void app_main(void);
}

// Progress callback function
void download_progress_callback(int percentage) {
    ESP_LOGI(TAG, "Download progress: %d%%", percentage);
    
    // Update display with progress
    char progress_msg[64];
    snprintf(progress_msg, sizeof(progress_msg), "Downloading: %d%%", percentage);
}

// Function to draw JPEG image on e-paper display
bool draw_jpeg_image(uint8_t* buffer, size_t buffer_size) {
    ESP_LOGI(TAG, "Starting JPEG decode and draw");
    display_status_message("Decoding JPEG...");
    
    // Open JPEG from RAM buffer
    if (!jpeg.openRAM(buffer, buffer_size, JPEGDraw)) {
        ESP_LOGE(TAG, "Failed to open JPEG from RAM. Error: %d", (int)jpeg.getLastError());
        return false;
    }
    
    // Get JPEG info
    int width = jpeg.getWidth();
    int height = jpeg.getHeight();
    ESP_LOGI(TAG, "JPEG dimensions: %dx%d", width, height);
    ESP_LOGI(TAG, "Display dimensions: %dx%d", epd_width(), epd_height());
    
    // Check if image fits on display
    if (width > epd_width() || height > epd_height()) {
        ESP_LOGW(TAG, "Image size (%dx%d) exceeds display size (%dx%d)", 
                 width, height, epd_width(), epd_height());
        // You could implement scaling here if needed
    }
    
    // Set pixel type to 8-bit grayscale for conversion to 4-bit
    jpeg.setPixelType(EIGHT_BIT_GRAYSCALE);
    
    // Clear the framebuffer before drawing
    uint8_t* fb = epd_hl_get_framebuffer(&hl);
    memset(fb, 0xFF, epd_width() * epd_height() / 2); // Clear to white (4-bit)
    
    // Decode and draw the JPEG
    if (!jpeg.decode(0, 0, 0)) {  // x, y, flags (0 = no flags)
        ESP_LOGE(TAG, "JPEG decode failed with error: %d", (int)jpeg.getLastError());
        jpeg.close();
        return false;
    }
    
    jpeg.close();
    ESP_LOGI(TAG, "JPEG decode completed successfully");
    
    epd_poweron();
    
    EpdRect update_area = {
        .x = 0,
        .y = 0,
        .width = epd_width(),
        .height = epd_height(),
    };
    
    enum EpdDrawError update_err = epd_hl_update_area(&hl, MODE_GC16, epd_ambient_temperature(), update_area);
    if (update_err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Failed to update display: %d", update_err);
        epd_poweroff();
        return false;
    }
    
    epd_poweroff();
    ESP_LOGI(TAG, "Image displayed successfully");
    return true;
}

esp_err_t wifi_init() {
    esp_err_t ret;
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize network interface
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create default event loop
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create WiFi event group
    wifi_event_group = xEventGroupCreate();
    if (!wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Create default WiFi station
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register event handlers
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            NULL,
                                            NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler,
                                            NULL,
                                            NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure WiFi
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi initialization completed, attempting to connect to SSID: %s", WIFI_SSID);
    return ESP_OK;
}

bool wait_for_wifi_connection() {
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    display_status_message("Connecting to WiFi...");
    
    // Wait for either connection success or failure
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi successfully");
        display_status_message("WiFi Connected!");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Show success message for 2 seconds
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        display_status_message("WiFi Connection Failed!");
        return false;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout");
        display_status_message("WiFi Timeout!");
        return false;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Binary Downloader Demo");
    
    // Initialize EPD
    idf_setup();
    
    idf_update();

    // Initialize WiFi
    if (wifi_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi");
        display_status_message("WiFi Init Failed!");
        return;
    }
    
    // Wait for WiFi connection
    if (!wait_for_wifi_connection()) {
        ESP_LOGE(TAG, "Cannot proceed without WiFi connection");
        display_status_message("No WiFi - Restarting...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
        return;
    }
    
    // Create BinaryDownloader instance
    BinaryDownloader downloader;
    
    ESP_LOGI(TAG, "Starting download from: %s", DOWNLOAD_URL);
    display_status_message("Starting download...");
    
    // Download the file
    DownloadResult result = downloader.download(
        DOWNLOAD_URL, 
        download_progress_callback,
        30000  // 30 second timeout
    );
    
    if (result.success) {
        ESP_LOGI(TAG, "Download successful!");
        ESP_LOGI(TAG, "Downloaded %" PRIu32 " bytes", (uint32_t)result.size);
        ESP_LOGI(TAG, "HTTP Status Code: %d", result.status_code);
        
        display_status_message("Download Complete!");
        
        // Detect image format
        FileFormat format = BinaryDownloader::DetectImageFormatFromBuffer(result.buffer, result.size);
        ESP_LOGI(TAG, "Detected image format: %s", BinaryDownloader::GetFormatName(format));
        
        // Check if it's a JPEG and draw it
        if (format == FILE_FORMAT_JPEG) {
            ESP_LOGI(TAG, "Processing JPEG image...");
            
            if (draw_jpeg_image(result.buffer, result.size)) {
                ESP_LOGI(TAG, "JPEG image displayed successfully!");
            } else {
                ESP_LOGE(TAG, "Failed to display JPEG image");
                display_status_message("Image display failed!");
            }
        } else {
            ESP_LOGW(TAG, "Downloaded file is not a JPEG image");
            display_status_message("Not a JPEG image!");
        }
        
    } else {
        ESP_LOGE(TAG, "Download failed!");
        ESP_LOGE(TAG, "HTTP Status Code: %d", result.status_code);
        if (!result.error_msg.empty()) {
            ESP_LOGE(TAG, "Error: %s", result.error_msg.c_str());
        }
        
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Download Failed: %d", result.status_code);
        display_status_message(error_msg);
    }
    
    ESP_LOGI(TAG, "Demo completed");
    
    // Keep the image displayed
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds before looping
    }
}