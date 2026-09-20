# DOIO KB16 maintenance notes

These notes capture context needed to maintain the KB16 Rev1 per-layer RGB indicator implementation.

## Repository split

KB16 VIA support spans three repositories:

- QMK firmware and keyboard-level behavior: `qmk/qmk_firmware`, under `keyboards/doio/kb16/`.
- The maintained VIA keymap: `the-via/qmk_userspace_via`, at `keyboards/doio/kb16/keymaps/via/`.
- The published VIA V3 definition: `the-via/keyboards`, at `v3/doio/kb16/kb16-01.json`.

QMK intentionally ignores `keyboards/**/keymaps/via/*`. Do not add a duplicate VIA keymap to this repository just to build it. Clone `qmk_userspace_via` and build its existing keymap as an overlay:

```bash
QMK_USERSPACE=/path/to/qmk_userspace_via qmk compile -kb doio/kb16/rev1 -km via
```

The existing external VIA keymap needs no custom indicator code because the protocol and RGB behavior are implemented at keyboard/revision level.

`keyboards/doio/kb16/kb16-01.via.json` is a sideloadable copy of the VIA definition. Upstream VIA definition changes belong in `the-via/keyboards`, not normally in QMK.

## Current implementation

Rev1-specific code is in:

- `rev1/rev1.c`: RGB indicator, color API, and VIA custom command handler.
- `rev1/rev1.h`: bounds-checked H/S getter and setter.
- `rev1/config.h`: seven bytes of VIA custom EEPROM allocation.

Do not move this into shared `kb16.c` without intentionally enabling it for Rev2 as well.

Behavior:

- Layer 0 and layers above 3 leave the active RGB Matrix effect untouched.
- Layers 1, 2, and 3 override all LEDs with configurable solid colors.
- Defaults are blue (`H=170`), green (`H=85`), and red (`H=0`), all at `S=255`.
- Brightness comes from `rgb_matrix_get_val()` and is never persisted per layer.
- Layer selection uses `get_highest_layer(layer_state | default_layer_state)`.
- The advanced indicator callback must call `rgb_matrix_indicators_advanced_user()` first and only write `[led_min, led_max)`.

VIA uses custom channel `0`, with one native color value per layer:

| Value ID | Meaning | Payload |
| --- | --- | --- |
| 1 | Layer 1 color | Hue, saturation |
| 2 | Layer 2 color | Hue, saturation |
| 3 | Layer 3 color | Hue, saturation |

`SET` changes runtime state, `GET` returns H/S, and `SAVE` writes all colors. Keep packet bounds checks: a color GET or SET accesses through `data[4]` and therefore requires at least five bytes.

Persistence uses the supported VIA custom-config region (`VIA_EEPROM_CUSTOM_CONFIG_SIZE`, `via_read_custom_config()`, and `via_update_custom_config()`) rather than hard-coded EEPROM addresses. Byte 0 is a format/version marker and bytes 1–6 contain the three H/S pairs. The marker prevents erased EEPROM (`0xFF`) from becoming a valid color configuration and allows defaults after a full EEPROM erase. Although newer general keyboard data often uses the eeconfig keyboard datablock, the VIA-owned region keeps these values separate from QMK's general keyboard datablock.

Do not assume VIA's `id_eeprom_reset` command clears this region: the EEPROM backend's `nvm_via_erase()` is a no-op unless a preceding full eeconfig erase already cleared the device. KB16 also does not currently define `VIA_EEPROM_ALLOW_RESET`, so the host reset command is disabled. Revisit reset semantics if that option is enabled later.

## VIA JSON trap

A VIA V3 custom menu has three required levels:

```text
top-level menu -> submenu -> UI control
```

Putting color controls directly inside a top-level menu produces many cascading import errors even though basic JSON parsing—and some repository build paths—may succeed. The valid shape used here is:

```json
{
  "label": "Layer Indicators",
  "content": [
    {
      "label": "Colors",
      "content": [
        {
          "label": "Layer 1 Color",
          "type": "color",
          "content": ["id_layer_1_color", 0, 1]
        }
      ]
    }
  ]
}
```

Color command tuples require all three fields: a unique value key, channel ID, and value ID. The color control itself appends hue and saturation to SET packets.

Validate a changed definition by copying it to `v3/doio/kb16/kb16-01.json` in a clone of `the-via/keyboards`, then running:

```bash
npm install
npm run build
```

Also test an actual import through VIA's Design tab; repository-wide generation alone did not catch the missing-submenu mistake encountered during initial implementation.

## Useful checks

```bash
qmk compile -kb doio/kb16/rev1 -km default
QMK_USERSPACE=/path/to/qmk_userspace_via qmk compile -kb doio/kb16/rev1 -km via
qmk lint -kb doio/kb16/rev1
qmk format-c -n keyboards/doio/kb16/rev1/rev1.c keyboards/doio/kb16/rev1/rev1.h
git diff --check
```

Measured during the initial implementation:

- Original default: 24,232 / 28,672 bytes.
- Updated default: 24,388 / 28,672 bytes.
- Original VIA: 26,948 / 28,672 bytes.
- Updated VIA using the official userspace: approximately 27,302 / 28,672 bytes, leaving about 1,370 bytes.

The VIA image is relatively full, so continue checking size after changes.

## Hardware checks still required

Compilation cannot prove:

- Correct colors and brightness on physical LEDs.
- Immediate restoration of the prior Layer 0 animation.
- GET/SET/SAVE behavior against the VIA application.
- Persistence after unplugging and reconnecting.
- Default restoration after a full EEPROM erase.
- Encoder remapping and dynamic keymap integrity after the EEPROM region changed size.

Exercise `MO`, `TG`, `TO`, `LT`, `DF`, and `OSL` where practical. Verify that the highest active layer controls the indicator and that layers above 3 fall back to the normal RGB effect.
