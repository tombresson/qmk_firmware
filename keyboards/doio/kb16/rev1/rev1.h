// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool kb16_rev1_layer_indicator_get(uint8_t layer, uint8_t *hue, uint8_t *sat);
bool kb16_rev1_layer_indicator_set(uint8_t layer, uint8_t hue, uint8_t sat);
