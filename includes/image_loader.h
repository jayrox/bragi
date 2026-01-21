#pragma once

#include "esphome.h"
#include <HTTPClient.h>
#include <WiFiClient.h>

// Simple image data holder
static uint8_t *album_art_data = nullptr;
static size_t album_art_size = 0;
static std::string last_loaded_key = "";

class ImageLoader {
public:
    bool load_image(std::string url, std::string cache_key) {
        // Skip if same image
        if (cache_key == last_loaded_key && cache_key.length() > 0) {
            ESP_LOGD("albumart", "Same image already loaded");
            return true;
        }
        
        ESP_LOGI("albumart", "Loading image from: %s", url.c_str());
        
        // Free old image data first
        free_image();
        
        // Download image
        HTTPClient http;
        http.begin(url.c_str());
        http.setTimeout(5000);
        
        int httpCode = http.GET();
        
        if (httpCode != HTTP_CODE_OK) {
            ESP_LOGE("albumart", "HTTP failed: %d", httpCode);
            http.end();
            return false;
        }
        
        int len = http.getSize();
        
        // Sanity check - reject if too large
        if (len <= 0 || len > 50000) {
            ESP_LOGE("albumart", "Invalid size: %d", len);
            http.end();
            return false;
        }
        
        ESP_LOGI("albumart", "Image size: %d bytes, Free heap: %d", len, ESP.getFreeHeap());
        
        // Check if we have enough memory (need at least 2x the file size free)
        if (ESP.getFreeHeap() < (len * 2)) {
            ESP_LOGE("albumart", "Not enough memory! Need: %d, Have: %d", len * 2, ESP.getFreeHeap());
            http.end();
            return false;
        }
        
        // Allocate memory
        album_art_data = (uint8_t*)malloc(len);
        if (!album_art_data) {
            ESP_LOGE("albumart", "malloc failed for %d bytes", len);
            http.end();
            return false;
        }
        
        // Read image data in chunks
        WiFiClient *stream = http.getStreamPtr();
        int downloaded = 0;
        
        while (http.connected() && downloaded < len) {
            size_t available = stream->available();
            if (available) {
                int toRead = (available > (len - downloaded)) ? (len - downloaded) : available;
                int bytesRead = stream->readBytes(album_art_data + downloaded, toRead);
                downloaded += bytesRead;
            }
            yield(); // Let other tasks run
        }
        
        http.end();
        
        if (downloaded != len) {
            ESP_LOGW("albumart", "Downloaded %d bytes, expected %d", downloaded, len);
        }
        
        album_art_size = downloaded;
        last_loaded_key = cache_key;
        
        ESP_LOGI("albumart", "Image loaded successfully. Free heap now: %d", ESP.getFreeHeap());
        
        return true;
    }
    
    void free_image() {
        if (album_art_data) {
            free(album_art_data);
            album_art_data = nullptr;
            album_art_size = 0;
        }
        last_loaded_key = "";
        ESP_LOGD("albumart", "Image freed. Free heap: %d", ESP.getFreeHeap());
    }
    
    uint8_t* get_data() { return album_art_data; }
    size_t get_size() { return album_art_size; }
};

static ImageLoader image_loader;
