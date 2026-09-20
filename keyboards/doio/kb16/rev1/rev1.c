// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "rev1.h"

#define LAYER_INDICATOR_MAGIC 0x01
#define LAYER_INDICATOR_COUNT 3

enum layer_indicator_eeprom_offsets {
    layer_indicator_magic_offset = 0,
    layer_indicator_colors_offset,
};

typedef struct {
    uint8_t hue;
    uint8_t sat;
} layer_indicator_color_t;

static layer_indicator_color_t layer_indicator_colors[LAYER_INDICATOR_COUNT] = {
    {170, 255},
    {85, 255},
    {0, 255},
};

bool kb16_rev1_layer_indicator_get(uint8_t layer, uint8_t *hue, uint8_t *sat) {
    if (layer < 1 || layer > LAYER_INDICATOR_COUNT || hue == NULL || sat == NULL) {
        return false;
    }

    *hue = layer_indicator_colors[layer - 1].hue;
    *sat = layer_indicator_colors[layer - 1].sat;
    return true;
}

bool kb16_rev1_layer_indicator_set(uint8_t layer, uint8_t hue, uint8_t sat) {
    if (layer < 1 || layer > LAYER_INDICATOR_COUNT) {
        return false;
    }

    layer_indicator_colors[layer - 1] = (layer_indicator_color_t){hue, sat};
    return true;
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    if (!rgb_matrix_indicators_advanced_user(led_min, led_max)) {
        return false;
    }

    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    if (layer >= 1 && layer <= LAYER_INDICATOR_COUNT) {
        hsv_t hsv = {
            .h = layer_indicator_colors[layer - 1].hue,
            .s = layer_indicator_colors[layer - 1].sat,
            .v = rgb_matrix_get_val(),
        };
        rgb_t rgb = hsv_to_rgb(hsv);

        for (uint8_t i = led_min; i < led_max; i++) {
            rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        }
    }

    return true;
}

#ifdef VIA_ENABLE
#    include "via.h"

void via_init_kb(void) {
    uint8_t config[VIA_EEPROM_CUSTOM_CONFIG_SIZE];

    if (via_eeprom_is_valid() && via_read_custom_config(config, 0, sizeof(config)) == sizeof(config) && config[layer_indicator_magic_offset] == LAYER_INDICATOR_MAGIC) {
        for (uint8_t layer = 1; layer <= LAYER_INDICATOR_COUNT; layer++) {
            kb16_rev1_layer_indicator_set(layer, config[layer_indicator_colors_offset + (layer - 1) * 2], config[layer_indicator_colors_offset + (layer - 1) * 2 + 1]);
        }
    }
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    if (length < 2 || data[1] != id_custom_channel) {
        if (length > 0) {
            data[0] = id_unhandled;
        }
        return;
    }

    switch (data[0]) {
        case id_custom_set_value:
            if (length < 5 || !kb16_rev1_layer_indicator_set(data[2], data[3], data[4])) {
                data[0] = id_unhandled;
            }
            break;
        case id_custom_get_value:
            if (length < 5 || !kb16_rev1_layer_indicator_get(data[2], &data[3], &data[4])) {
                data[0] = id_unhandled;
            }
            break;
        case id_custom_save:
            if (length < 2) {
                data[0] = id_unhandled;
                break;
            }

            uint8_t config[VIA_EEPROM_CUSTOM_CONFIG_SIZE] = {LAYER_INDICATOR_MAGIC};
            for (uint8_t layer = 1; layer <= LAYER_INDICATOR_COUNT; layer++) {
                kb16_rev1_layer_indicator_get(layer, &config[layer_indicator_colors_offset + (layer - 1) * 2], &config[layer_indicator_colors_offset + (layer - 1) * 2 + 1]);
            }
            via_update_custom_config(config, 0, sizeof(config));
            break;
        default:
            data[0] = id_unhandled;
            break;
    }
}
#endif
