#pragma once

#include "esphome.h"
#include <vector>
#include <string>

class PlaylistHandler : public Component {
public:
    lv_obj_t* parent;
    lv_obj_t* scroll_container;
    lv_obj_t* back_btn;
    lv_obj_t* back_label;
    std::vector<lv_obj_t*> buttons;
    std::vector<lv_obj_t*> labels;
    std::vector<std::string> playlist_names;

    void setup() {
        if (!parent) return;

        // Create scrollable container
        scroll_container = lv_obj_create(parent);
        lv_obj_set_size(scroll_container, 320, 180);
        lv_obj_set_pos(scroll_container, 0, 0);
        lv_obj_set_style_bg_color(scroll_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(scroll_container, 0, 0);
        lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_AUTO);
        
        // Back button at bottom
        back_btn = lv_btn_create(parent);
        lv_obj_set_size(back_btn, 280, 50);
        lv_obj_set_pos(back_btn, 18, 190);
        
        // Style the back button to match theme (using dynamic colors)
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_obj_set_style_border_color(back_btn, lv_color_hex(id(current_fg_color)), LV_PART_MAIN);
        lv_obj_set_style_border_width(back_btn, 1, LV_PART_MAIN);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x444444), LV_STATE_PRESSED);
        
        back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, "Back to Player");
        lv_obj_set_style_text_font(back_label, &id(main_font), LV_PART_MAIN);
        lv_obj_set_style_text_color(back_label, lv_color_hex(id(current_fg_color)), LV_PART_MAIN);
        lv_obj_center(back_label);
        
        lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
            lv_disp_load_scr(id(main_page).obj);
        }, LV_EVENT_CLICKED, nullptr);
    }

    void update_colors() {
        // Update back button colors
        if (back_btn) {
            lv_obj_set_style_border_color(back_btn, lv_color_hex(id(current_fg_color)), LV_PART_MAIN);
        }
        if (back_label) {
            lv_obj_set_style_text_color(back_label, lv_color_hex(id(current_fg_color)), LV_PART_MAIN);
        }
        
        // Update all playlist button colors
        for (auto btn : buttons) {
            lv_obj_set_style_border_color(btn, lv_color_hex(id(current_fg_color)), LV_PART_MAIN);
        }
        
        // Update all playlist label colors
        for (auto label : labels) {
            lv_obj_set_style_text_color(label, lv_color_hex(id(current_fg_color)), LV_PART_MAIN);
        }
    }

    void update_playlists(const std::string& options_str) {
        // Clear existing buttons
        for (auto btn : buttons) {
            lv_obj_del(btn);
        }
        buttons.clear();
        labels.clear();
        playlist_names.clear();

        if (!scroll_container) return;

        // Parse the options string - expecting format like: "['playlist1', 'playlist2', 'playlist3']"
        // or simple comma-separated: "playlist1, playlist2, playlist3"
        
        std::string cleaned = options_str;
        
        // Remove brackets and quotes if present
        size_t start = cleaned.find_first_not_of(" \t\n\r[");
        size_t end = cleaned.find_last_not_of(" \t\n\r]");
        if (start != std::string::npos && end != std::string::npos) {
            cleaned = cleaned.substr(start, end - start + 1);
        }
        
        // Parse comma-separated values
        size_t pos = 0;
        while (pos < cleaned.length()) {
            size_t comma_pos = cleaned.find(',', pos);
            if (comma_pos == std::string::npos) {
                comma_pos = cleaned.length();
            }
            
            std::string item = cleaned.substr(pos, comma_pos - pos);
            
            // Remove quotes and whitespace
            size_t item_start = item.find_first_not_of(" \t\n\r'\"");
            size_t item_end = item.find_last_not_of(" \t\n\r'\"");
            
            if (item_start != std::string::npos && item_end != std::string::npos) {
                item = item.substr(item_start, item_end - item_start + 1);
                if (!item.empty()) {
                    playlist_names.push_back(item);
                }
            }
            
            pos = comma_pos + 1;
        }

        ESP_LOGD("playlist", "Parsed %d playlists from: %s", playlist_names.size(), options_str.c_str());

        // Create buttons for each playlist
        int y_pos = 10;
        for (size_t i = 0; i < playlist_names.size(); i++) {
            lv_obj_t* btn = lv_btn_create(scroll_container);
            lv_obj_set_size(btn, 280, 45);
            lv_obj_set_pos(btn, 5, y_pos);
            
            // Style the button to match theme (using dynamic colors)
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), LV_PART_MAIN);
            lv_obj_set_style_border_color(btn, lv_color_hex(id(current_fg_color)), LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x444444), LV_STATE_PRESSED);
            
            lv_obj_t* label = lv_label_create(btn);
            lv_label_set_text(label, playlist_names[i].c_str());
            lv_obj_set_style_text_font(label, &id(main_font), LV_PART_MAIN);
            lv_obj_set_style_text_color(label, lv_color_hex(id(current_fg_color)), LV_PART_MAIN);
            lv_obj_center(label);
            
            // Store index for callback
            lv_obj_set_user_data(btn, (void*)i);
            
            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                PlaylistHandler* handler = (PlaylistHandler*)lv_event_get_user_data(e);
                lv_obj_t* btn = lv_event_get_target(e);
                size_t idx = (size_t)lv_obj_get_user_data(btn);
                
                if (idx < handler->playlist_names.size()) {
                    handler->play_playlist(handler->playlist_names[idx]);
                }
            }, LV_EVENT_CLICKED, this);
            
            buttons.push_back(btn);
            labels.push_back(label);
            y_pos += 55;
        }

        ESP_LOGD("playlist", "Created %d playlist buttons", buttons.size());
    }

    void play_playlist(const std::string& playlist_name) {
        ESP_LOGI("playlist", "Playing playlist: %s", playlist_name.c_str());
        
        // Store the playlist name in a global variable that the YAML automation can read
        id(selected_playlist).publish_state(playlist_name);
        
        // Go back to main page using a timeout
        this->set_timeout(100, []() {
            lv_disp_load_scr(id(main_page).obj);
        });
    }
};

// Global instance
static PlaylistHandler playlist_handler;
