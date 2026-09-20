# DOIO KB16 Rev1 — QMK/VIA Per-Layer RGB Indicator Implementation Plan

## Objective

Add configurable **per-layer RGB indication** to the DOIO KB16 Rev1, similar in user experience to the layer-indicator color controls available on keyboards such as the NK87.

The implementation should use the KB16's existing **QMK RGB Matrix** support rather than porting the older Wilba/NK87 RGB backlight implementation.

The desired behavior is:

- Layer 0: use the normal RGB Matrix effect configured through VIA/QMK.
- Layer 1: override the matrix with a user-configurable Layer 1 color.
- Layer 2: override the matrix with a user-configurable Layer 2 color.
- Layer 3: override the matrix with a user-configurable Layer 3 color.
- Brightness should preferably continue to follow the normal RGB Matrix brightness setting.
- Layer colors should persist across reboots.
- Layer colors should be configurable from VIA using color-picker controls.

The feature should be implemented as far down at the **keyboard level** as practical so that normal QMK keymaps, including keymaps generated through QMK Configurator, can continue to use the feature without requiring custom `keymap.c` indicator code.

---

## Target Hardware

Keyboard:

```text
DOIO KB16 Rev1
```

Relevant characteristics already established:

```text
MCU: ATmega32U4
Lighting: QMK RGB Matrix
LEDs: 16 addressable RGB LEDs
Layout: 4 × 4 macro pad
Layers in current VIA keymap: 4
VIA support: already present
Encoder map support: already present
```

The existing RGB Matrix implementation means no new LED driver should be required.

---

## Design Goals

The implementation should satisfy the following.

1. Use native QMK RGB Matrix APIs.
2. Do not port the old Wilba RGB Backlight subsystem used by older NK87 firmware.
3. Implement the actual layer-lighting behavior at the keyboard level where possible.
4. Allow normal QMK Configurator-generated keymaps to continue functioning.
5. Add VIA controls for selecting colors for Layers 1–3.
6. Persist those colors in EEPROM.
7. Preserve the existing RGB Matrix effect on Layer 0.
8. Preserve the user's existing RGB Matrix brightness setting when displaying layer colors.
9. Avoid breaking existing VIA functionality, encoder mapping, RGB effects, or dynamic keymaps.
10. Keep the implementation small enough to fit comfortably on the ATmega32U4.

---

## Non-Goals

Do not initially attempt to:

- add custom layer-color controls to the QMK Configurator web interface;
- reproduce the complete NK87/Wilba lighting implementation;
- create separate lighting effects per layer;
- implement per-key layer color assignments;
- expose RGB brightness independently for every layer;
- dynamically support an arbitrary number of layers;
- modify QMK's generic RGB Matrix subsystem.

For the first implementation, support the KB16's four normal layers:

```text
Layer 0 = normal RGB Matrix
Layer 1 = custom indicator color
Layer 2 = custom indicator color
Layer 3 = custom indicator color
```

---

## Repositories to Inspect

The implementation will likely involve two related codebases.

### QMK

Inspect the current upstream QMK definition for:

```text
keyboards/doio/kb16/
keyboards/doio/kb16/rev1/
```

Determine the current exact structure before modifying anything.

Likely relevant files include some subset of:

```text
keyboards/doio/kb16/keyboard.json
keyboards/doio/kb16/rev1/config.h
keyboards/doio/kb16/rev1/rev1.c
keyboards/doio/kb16/rev1/rev1.h
keyboards/doio/kb16/keymaps/default/keymap.c
```

Do not assume these exact files exist in the current tree; inspect first.

### VIA QMK Userspace

Inspect the current KB16 VIA definition/keymap under:

```text
the-via/qmk_userspace_via
```

Likely paths include:

```text
keyboards/doio/kb16/keymaps/via/
```

Also locate the corresponding VIA device definition JSON used by the current VIA application.

Determine whether KB16 uses:

- a V2 definition;
- a V3 definition;
- generated definitions;
- custom menus already;
- standard `qmk_rgb_matrix` menus.

---

## Architecture

The implementation should be divided into three logical pieces:

```text
                +------------------+
                |       VIA        |
                |                  |
                | Layer 1 Color    |
                | Layer 2 Color    |
                | Layer 3 Color    |
                +--------+---------+
                         |
                  VIA custom command
                         |
                         v
              +----------------------+
              | KB16 firmware config |
              |                      |
              | EEPROM-backed HSV    |
              | layer colors         |
              +----------+-----------+
                         |
                         v
              +----------------------+
              | QMK RGB Matrix       |
              | indicator callback   |
              +----------+-----------+
                         |
             +-----------+-----------+
             |                       |
          Layer 0                 Layer > 0
             |                       |
       Normal RGB effect      Override all LEDs
                             with layer color
```

---

## RGB Matrix Behavior

Use the QMK RGB Matrix indicator callback rather than replacing the underlying RGB effect.

Prefer one of:

```c
rgb_matrix_indicators_kb()
```

or:

```c
rgb_matrix_indicators_advanced_kb()
```

depending on the current QMK API and the best fit for this keyboard.

The keyboard-level function should preserve the corresponding user callback.

Conceptually:

```c
bool rgb_matrix_indicators_kb(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);

    if (layer > 0 && layer <= 3) {
        // Resolve configured HSV color.
        // Use current global RGB Matrix brightness for V.
        // Convert HSV -> RGB.
        // Override all 16 LEDs.
    }

    return rgb_matrix_indicators_user();
}
```

If using the advanced form:

```c
bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max)
```

only update LEDs between `led_min` and `led_max`.

This may be preferable for QMK's RGB Matrix processing model.

---

## Brightness Behavior

Store only:

```text
Hue
Saturation
```

for each layer color.

Do not initially store a separate `V` value.

When rendering the layer indicator, use:

```c
rgb_matrix_get_val()
```

for brightness.

Conceptually:

```c
HSV hsv = {
    .h = configured_hue,
    .s = configured_saturation,
    .v = rgb_matrix_get_val(),
};
```

Then:

```c
RGB rgb = hsv_to_rgb(hsv);
```

This ensures that VIA's normal RGB Matrix brightness setting continues to control the apparent brightness of the layer indicators.

---

## Layer Resolution

Use:

```c
get_highest_layer(layer_state | default_layer_state)
```

rather than inspecting `layer_state` alone.

This correctly accounts for the default layer as well as activated layers.

Initial behavior:

```text
Resolved layer 0:
    do not override RGB Matrix

Resolved layer 1:
    display configured Layer 1 color

Resolved layer 2:
    display configured Layer 2 color

Resolved layer 3:
    display configured Layer 3 color

Any higher layer:
    preferably fall back to normal RGB behavior unless explicitly supported
```

Do not assume that enum names such as `_FN`, `_FN1`, or `_FN2` are available at keyboard level. The keyboard implementation should generally operate on numerical layer indexes.

---

## Configuration Structure

Use a compact configuration structure.

For example:

```c
typedef struct {
    uint8_t h;
    uint8_t s;
} kb16_layer_color_t;

typedef struct {
    kb16_layer_color_t layer[3];
} kb16_layer_indicator_config_t;
```

Total payload:

```text
3 layers × 2 bytes = 6 bytes
```

Avoid compiler-dependent packed bitfields.

---

## Default Colors

Choose obvious defaults so the feature is usable before VIA customization.

For example:

```text
Layer 1: Blue
Layer 2: Green
Layer 3: Red
```

Exact HSV defaults are not especially important as long as they are visibly distinct.

Example:

```c
Layer 1: H=170 S=255
Layer 2: H=85  S=255
Layer 3: H=0   S=255
```

Verify QMK's actual HSV scale before committing.

---

## EEPROM Storage

Use VIA's supported custom configuration storage mechanism rather than manually selecting arbitrary EEPROM addresses.

Reserve enough custom VIA EEPROM space for the configuration.

Expected requirement:

```text
6 bytes
```

possibly plus a future version byte if useful.

Investigate the current QMK/VIA APIs for:

```c
VIA_EEPROM_CUSTOM_CONFIG_SIZE
via_read_custom_config()
via_update_custom_config()
via_eeprom_is_valid()
```

Use the current API names/signatures from the checked-out QMK version rather than blindly copying old examples.

The implementation should:

1. initialize sensible defaults;
2. load custom EEPROM configuration when valid;
3. write defaults if the VIA EEPROM area is newly initialized;
4. save changes when VIA sends the appropriate save command.

---

## VIA Custom Protocol

Use VIA V3 custom UI functionality.

Reserve a keyboard-specific custom channel, preferably:

```text
channel 0
```

unless current VIA documentation specifies otherwise.

Create one logical custom value for layer indicator colors.

Suggested mapping:

```text
Custom channel: 0

Value ID:
1 = Layer indicator color

Argument:
0 = Layer 1
1 = Layer 2
2 = Layer 3

Payload:
Hue
Saturation
```

Conceptually:

```text
[channel]
[value ID]
[layer index]
[hue]
[saturation]
```

Support at minimum:

```text
id_custom_get_value
id_custom_set_value
id_custom_save
```

Unknown commands or values must be passed through using VIA's appropriate `id_unhandled` behavior.

---

## VIA Firmware Handler

Implement the current equivalent of:

```c
via_custom_value_command_kb()
```

Do not assume the older function signature is still current.

Conceptual flow:

```c
switch (command_id) {

    case GET:
        if custom channel/value match:
            return H/S for requested layer
        break;

    case SET:
        if custom channel/value match:
            update H/S for requested layer
        break;

    case SAVE:
        write configuration to VIA custom EEPROM
        break;

    default:
        mark command unhandled
}
```

Perform bounds checking:

```text
valid custom layer indexes = 0..2
```

Never trust the VIA payload blindly.

---

## VIA UI

Add a custom menu resembling:

```text
Lighting
 ├── Standard RGB Matrix controls
 └── Layer Indicators
      ├── Layer 1 Color
      ├── Layer 2 Color
      └── Layer 3 Color
```

Each layer should use VIA's native:

```text
color
```

control.

Retain normal RGB Matrix controls such as:

```text
Effect
Brightness
Speed
Color
```

using the existing VIA `qmk_rgb_matrix` support if already present.

The desired user experience is:

```text
VIA
→ Lighting
→ Layer Indicators
→ Layer 2 Color
→ choose color
```

No firmware rebuild should be required when changing a layer color.

---

## Relationship With QMK Configurator

A major design requirement is that **QMK Configurator does not need to know about the layer colors**.

The Configurator should continue to handle:

```text
key assignments
layer assignments
macros/keycodes that it already supports
```

The layer-indicator feature should exist at the keyboard firmware level.

Desired workflow:

```text
QMK Configurator
     |
     | generates ordinary KB16 keymap
     v
KB16 keyboard firmware
     |
     +-- RGB Matrix support
     |
     +-- layer-indicator implementation
     |
     +-- VIA custom configuration when built with VIA
```

The QMK Configurator itself does not need a color picker.

Important: investigate how QMK Configurator compiles keyboard definitions versus keymap-specific code. Ensure the layer-indicator implementation is in a location included in Configurator builds.

---

## VIA-Only Dependencies

Do not accidentally make ordinary non-VIA QMK builds fail because VIA symbols are unavailable.

Separate:

```text
RGB indicator behavior
```

from:

```text
VIA configuration protocol
```

appropriately.

Possible structure:

```c
#ifdef VIA_ENABLE
    // VIA EEPROM/configuration/custom command code
#endif
```

The RGB indicator implementation should either:

- use default colors when VIA is unavailable, or
- only compile configurable indicators when explicitly enabled.

Prefer a design where regular QMK firmware can still compile.

---

## Suggested File Organization

Exact locations should follow current QMK conventions after inspecting the tree.

A possible structure is:

```text
keyboards/doio/kb16/rev1/
    rev1.c
    rev1.h
    config.h
```

with responsibilities approximately:

```text
rev1.c
    RGB Matrix layer indication
    default layer colors
    configuration accessors

config.h
    feature-related constants if needed
```

VIA-specific code may instead belong in the VIA keymap if upstream QMK does not permit VIA-specific behavior in the keyboard implementation.

If that separation is necessary, keep the **RGB layer indicator engine** at keyboard level and expose an API for the VIA keymap to modify the stored colors.

Example:

```c
void kb16_set_layer_indicator_color(uint8_t layer, HSV color);
HSV kb16_get_layer_indicator_color(uint8_t layer);
```

This would still avoid putting the actual RGB Matrix behavior in `keymap.c`.

---

## Important Architectural Question

Determine whether the custom VIA configuration should live:

### Option A — Keyboard level

Advantages:

- behavior belongs to the hardware;
- one implementation;
- cleaner integration.

Disadvantage:

- introduces VIA-specific code into normal QMK keyboard sources.

### Option B — RGB engine at keyboard level, VIA protocol at VIA-keymap level

Advantages:

- QMK keyboard support remains VIA-independent;
- cleaner upstream architecture;
- VIA-specific code stays in VIA userspace.

Likely preferable for upstream acceptance.

Recommended approach:

```text
Keyboard level:
    RGB indicator behavior
    layer-color state API
    sensible defaults

VIA keymap/userspace:
    EEPROM persistence
    VIA custom commands
    custom UI
```

However, verify whether persistent values can be cleanly passed into the keyboard-level implementation without unnecessary complexity.

---

## Flash-Space Considerations

The ATmega32U4 has limited flash.

Before implementation, record baseline firmware size.

Example:

```bash
qmk compile -kb doio/kb16/rev1 -km via
```

Record:

```text
Flash used
EEPROM usage
RAM usage
```

After implementation, compare.

The additional functionality should remain small:

```text
3 HSV values
one indicator callback
small VIA command handler
```

Avoid adding large RGB effects, tables, strings, or unnecessary abstractions.

---

## Development Sequence

### Phase 1 — Establish Baseline

Clone/update:

```text
qmk/qmk_firmware
the-via/qmk_userspace_via
```

Compile the existing KB16 Rev1 VIA firmware unchanged.

Confirm:

```text
build succeeds
VIA detects device
key remapping works
encoders work
RGB Matrix controls work
```

Save the baseline binary size.

### Phase 2 — Hard-Coded Layer Indicator

Before adding VIA configuration, prove the RGB behavior.

Implement hard-coded colors:

```text
Layer 0 = normal effect
Layer 1 = blue
Layer 2 = green
Layer 3 = red
```

Test:

```text
MO(1)
MO(2)
MO(3)
TG()
TO()
LT()
```

where practical.

Verify that the highest active layer controls the color.

Also verify that releasing a momentary layer immediately restores the correct previous layer color/effect.

### Phase 3 — Preserve Brightness

Replace fixed RGB values with HSV values whose `V` comes from:

```c
rgb_matrix_get_val()
```

Confirm that VIA's normal brightness slider adjusts both:

```text
normal Layer 0 RGB
layer indicator RGB
```

### Phase 4 — Configuration API

Add a small keyboard-side API for reading/updating layer colors.

Example:

```c
bool kb16_layer_indicator_set_color(uint8_t layer, uint8_t h, uint8_t s);
bool kb16_layer_indicator_get_color(uint8_t layer, uint8_t *h, uint8_t *s);
```

Bounds-check layer numbers.

### Phase 5 — VIA EEPROM Persistence

Reserve custom VIA EEPROM space.

Implement:

```text
defaults
load
save
factory/reset behavior
```

Confirm settings survive:

```text
USB disconnect
computer reboot
keyboard reset
bootloader/normal restart
```

Also confirm a VIA reset does not corrupt dynamic keymap data.

### Phase 6 — VIA Custom Commands

Implement the custom-value protocol.

Test manually or through VIA developer tooling before building the final UI.

Verify:

```text
GET Layer 1
SET Layer 1
GET Layer 1 returns changed value
SAVE
power cycle
GET Layer 1 returns persisted value
```

Repeat for Layers 2 and 3.

### Phase 7 — VIA UI

Add three color controls.

Verify:

```text
Layer 1 Color
Layer 2 Color
Layer 3 Color
```

all initialize from firmware correctly.

Changing a picker should update the keyboard.

Saving should persist values.

---

## Functional Test Matrix

| Test | Expected Result |
|---|---|
| Boot on Layer 0 | Normal RGB Matrix effect |
| Activate Layer 1 | All LEDs show Layer 1 color |
| Release Layer 1 | Normal RGB effect returns |
| Activate Layer 2 | All LEDs show Layer 2 color |
| Activate Layer 3 | All LEDs show Layer 3 color |
| Layer 1 + Layer 2 active | Highest active layer wins |
| Change global brightness | Indicator brightness follows |
| Change RGB effect | Layer 0 uses new effect |
| Enter higher layer | Indicator still overrides effect |
| Change Layer 1 color in VIA | Layer 1 immediately reflects it |
| Reboot keyboard | Selected colors persist |
| Remap keys in VIA | Indicator behavior unaffected |
| Remap encoders | Encoder functionality unaffected |
| VIA reset | No EEPROM corruption |
| Non-VIA build | Compiles successfully if intended |
| QMK Configurator keymap | Compiles and retains keyboard-level indicator behavior |

---

## Layer-Key Edge Cases

Test common QMK layer mechanisms:

```text
MO(layer)
TG(layer)
TO(layer)
LT(layer, key)
DF(layer)
OSL(layer)
```

The implementation should derive lighting state from QMK's actual layer state rather than trying to detect layer keycodes.

This ensures the lighting also works when layers are changed programmatically.

---

## RGB Matrix Interaction Tests

Test with several existing RGB effects:

```text
Solid Color
Breathing
Cycle
Reactive effect
```

Expected behavior:

```text
Layer 0:
    selected effect runs normally

Layer 1–3:
    indicator color overrides effect

Return to Layer 0:
    previous effect resumes
```

The indicator implementation should not permanently modify:

```text
RGB mode
RGB hue
RGB saturation
RGB speed
```

It should only override rendered LEDs during the indicator phase.

---

## Factory Defaults / Reset Behavior

Determine how VIA's current reset behavior interacts with custom EEPROM.

Desired behavior after a complete VIA EEPROM reset:

```text
Layer 1 = default blue
Layer 2 = default green
Layer 3 = default red
```

Do not allow erased EEPROM (`0xFF`) to become unintended HSV values.

Use VIA's EEPROM validity mechanism where possible.

---

## QMK Configurator Validation

This is an important acceptance test.

Generate a KB16 Rev1 keymap through QMK Configurator.

Compile it using the normal Configurator/QMK workflow.

Confirm:

```text
keyboard boots
keymap works
RGB Matrix works
layer indicator code is present
Layer 1–3 produce indicator colors
```

If custom VIA colors obviously cannot exist in a non-VIA firmware, the firmware should use the compiled default colors.

The important point is that the **indicator mechanism itself** must not require custom `keymap.c` code.

---

## Potential Enhancement: Enable/Disable Toggle

Do not make this mandatory for v1, but structure the configuration so adding:

```text
Layer Indicator: Enabled / Disabled
```

later is straightforward.

Potential config structure:

```c
typedef struct {
    uint8_t enabled;
    kb16_layer_color_t layer[3];
} kb16_layer_indicator_config_t;
```

This would increase storage only slightly.

For the initial implementation, always-on indicators for layers 1–3 are acceptable.

---

## Potential Enhancement: Individual Indicator LEDs

The initial implementation should color all 16 LEDs.

Later, support could optionally be added for:

```text
single LED indicator
specific key indicators
layer-specific LED mask
```

Do not complicate the first implementation with this.

The all-key implementation is appropriate for a 4×4 macropad and gives very obvious layer feedback.

---

## Upstreaming Strategy

Keep upstreamability in mind even if the first version is maintained privately.

Ideally create two logically separated changes.

### QMK PR

Contains:

```text
KB16 RGB layer-indicator support
clean keyboard-level API
no VIA UI dependency where avoidable
```

### VIA PR

Contains:

```text
custom layer color controls
VIA custom command handler/config
definition JSON changes
```

Before opening a PR:

```text
run qmk format
run qmk lint
compile default keymap
compile VIA keymap
```

Also check QMK's current contribution requirements for keyboard-level feature additions.

---

## Risks

### Flash space

ATmega32U4 firmware can be space-constrained.

Mitigation:

- measure baseline and final binaries;
- keep implementation simple.

### VIA API changes

Older examples on the web may use outdated VIA custom UI APIs.

Mitigation:

- use the current `qmk_userspace_via` implementation and current VIA documentation as authoritative references.

### QMK callback API changes

RGB Matrix callback signatures may differ between old examples and current QMK.

Mitigation:

- inspect the current QMK headers/docs and existing keyboards using the callbacks.

### VIA EEPROM collision

Incorrect EEPROM offsets could corrupt the dynamic keymap.

Mitigation:

- exclusively use VIA's custom EEPROM allocation APIs;
- do not hard-code arbitrary EEPROM addresses.

### Keyboard-level VIA dependency

Putting VIA protocol code directly in `rev1.c` may be undesirable upstream.

Mitigation:

- separate generic layer-indicator code from VIA-specific persistence/UI.

---

## Open Questions for the Implementing Agent

Before coding, resolve these from the current source tree:

1. What is the exact current KB16 Rev1 keyboard path and file structure?
2. Is RGB Matrix currently declared in `keyboard.json`, `info.json`, or legacy config files?
3. Which RGB Matrix indicator callback form is preferred by current QMK?
4. Where does the current KB16 VIA keymap live?
5. Does its VIA definition already use V3 custom menus?
6. Does the current VIA custom color control send H/S only, as expected?
7. What is the current recommended VIA custom EEPROM API?
8. Can the generic layer-color state cleanly live at keyboard level while persistence lives in the VIA keymap?
9. How much flash remains in the existing KB16 Rev1 VIA build?
10. Does QMK Configurator compile the same keyboard-level source code in a way that preserves the proposed indicator implementation?

Do not guess these. Inspect the current QMK/VIA trees.

---

## Acceptance Criteria

The task is complete when all of the following are true:

- [ ] Existing KB16 Rev1 firmware builds successfully.
- [ ] Existing VIA functionality remains operational.
- [ ] Existing encoder mappings remain operational.
- [ ] Layer 0 retains the selected normal RGB Matrix effect.
- [ ] Layer 1 displays a configurable solid color.
- [ ] Layer 2 displays a configurable solid color.
- [ ] Layer 3 displays a configurable solid color.
- [ ] Highest active layer determines the indicator color.
- [ ] RGB brightness continues to affect the indicator.
- [ ] Entering a layer does not permanently change the selected RGB effect.
- [ ] Returning to Layer 0 restores the previous RGB effect.
- [ ] Layer colors can be changed through VIA.
- [ ] Layer colors survive a power cycle.
- [ ] Custom EEPROM storage does not corrupt VIA keymaps/macros.
- [ ] A normal non-custom keymap can use the keyboard-level indicator implementation.
- [ ] A QMK Configurator-generated keymap retains layer indication.
- [ ] Firmware still fits safely within ATmega32U4 flash limits.
- [ ] Code passes applicable QMK formatting/lint checks.

---

## Recommended First Milestone

Do **not** start by implementing VIA.

First prove the underlying behavior with a minimal hard-coded keyboard-level RGB Matrix callback:

```text
Layer 0 → normal RGB
Layer 1 → blue
Layer 2 → green
Layer 3 → red
```

Once that works correctly with QMK Configurator-compatible keymaps, add the VIA configuration layer.

This keeps the problem separated into:

```text
1. QMK layer indication
2. persistent configuration
3. VIA UI
```

and makes failures substantially easier to diagnose.

---

## Desired End State

The user should ultimately be able to configure the KB16 like this:

```text
VIA
  → Lighting
      → RGB Matrix
          Effect: Cycle Left/Right
          Brightness: 70%

      → Layer Indicators
          Layer 1: Blue
          Layer 2: Orange
          Layer 3: Red
```

Then, during normal use:

```text
Layer 0
    animated VIA-selected RGB effect

Hold Layer 1
    entire KB16 becomes blue

Enter Layer 2
    entire KB16 becomes orange

Enter Layer 3
    entire KB16 becomes red

Return to Layer 0
    previous RGB animation resumes
```

The implementation should achieve this using the current QMK RGB Matrix architecture and VIA custom configuration facilities, without depending on the older NK87/Wilba RGB-backlight implementation.
