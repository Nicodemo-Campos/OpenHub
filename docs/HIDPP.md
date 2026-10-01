# HID++ probing, reads, and validated controls in OpenHub

OpenHub follows **discovery before control**.

## v0.2.5: profile-aware controls and button assignment decoding

The G502 exposes both:

- Adjustable Report Rate `0x8060`;
- On-board Profiles `0x8100`.

When on-board mode is active, direct `0x8060 SetReportRate` can be rejected even though the same feature remains readable. In that mode, report rate is part of the active profile.

OpenHub uses two report-rate paths:

- **Host mode:** direct `0x8060` write.
- **On-board mode:** update the report-rate byte inside the active `0x8100` profile sector.

## 0x8100 memory flow

OpenHub first reads `GetProfilesDescriptor` and currently accepts only the family of layouts already used by mature HID++ implementations:

- memory model `0x01`;
- profile format `0x01` through `0x05`;
- macro format `0x01`;
- bounded sector sizes. HID++ profile sectors do not need to be a multiple of 16 bytes; the G502 LIGHTSPEED reports 255 bytes.

A profile-memory write is not enabled unless the descriptor, directory and active profile can all be validated.

### Directory

Sector `0x0000` is read through `MemoryRead` using 16-byte HID++ chunks. For non-multiple sector sizes, the final read overlaps the previous chunk so the exact final bytes can be recovered without reading past the sector boundary.

OpenHub verifies the CRC-CCITT stored in the final two bytes before trusting the directory.

Each active profile directory entry is used only to locate the existing profile sector. v0.2.3 does **not** change directory entries.

### Active profile sector

The active sector is read and CRC-checked.

For the known profile formats, byte 0 stores the report interval in milliseconds. The rest of the sector includes DPI slots, button bindings, lighting state, names, timeouts, and other fields.

OpenHub does not reconstruct that structure for this write. It instead:

1. keeps the exact original sector bytes;
2. changes only byte 0;
3. recomputes the CRC;
4. writes the complete sector back;
5. reads the complete sector back;
6. requires byte-for-byte equality with the intended clone.

This avoids rewriting unknown fields from assumptions.

## HID++ memory commands used

The profile path uses these `0x8100` functions:

- `0x00` GetProfilesDescriptor
- `0x20` GetOnboardMode
- `0x30` SetCurrentProfile, only to reload the already-active profile if required
- `0x40` GetCurrentProfile
- `0x50` MemoryRead
- `0x60` MemoryAddressWrite
- `0x70` MemoryWrite
- `0x80` MemoryWriteEnd

Memory addressing and data writes require HID++ long reports because their payloads exceed the 3-byte short-report parameter area. The write-start command declares the exact byte count, so a sector such as 255 bytes is valid even though the transport chunks are up to 16 bytes.

## CRC

Profile sectors use CRC-16/CCITT with seed `0xFFFF`.

The CRC is calculated over every sector byte except the final two bytes and then stored big-endian in those final two bytes.

OpenHub validates the original CRC before writing and the new CRC after read-back.

## Live verification and rollback

After a profile sector verifies in flash, OpenHub reads the live report rate through `0x8060`.

If firmware has not reloaded the modified active profile, OpenHub re-selects the same profile choice and checks again.

If live verification still fails, OpenHub attempts to write the original sector back and re-select the same profile.

## DPI and on-board DPI stages

The direct active-state DPI path remains available:

- read sensor count and supported DPI range/list;
- validate the requested value;
- `SetSensorDpi`;
- verify with `GetSensorDpi`.

v0.2.4 additionally parses the active profile's documented DPI fields:

- byte 1: default DPI stage index;
- byte 2: DPI-shift stage index;
- bytes 3–12: five little-endian 16-bit DPI values;
- a DPI value of 0 means that stage is disabled.

The current active DPI stage is read through 0x8100 function `0xB0` and changed through `0xC0`.

For persistent stage edits, OpenHub validates all non-zero values against the live 0x2201 range/list before cloning the active profile sector. Only byte 1, bytes 3–12, and the CRC are changed. Byte 2 (the DPI-shift stage assignment) is preserved.

After writing, OpenHub:

1. reads the complete sector back;
2. requires an exact match;
3. re-selects the same profile;
4. restores the previous active DPI stage if it is still enabled, otherwise the selected default stage;
5. verifies both the current DPI index and the live 0x2201 DPI value.

If that final verification fails, OpenHub attempts to restore the original profile sector.

## Intentionally excluded in v0.2.4

No writes are made to:

- profile directory entries;
- button bindings or macros;
- lighting data;
- profile names;
- power/time-out settings;
- hidden/internal features;
- firmware/DFU;
- receiver-child devices;
- ASTRO A50 X.

## References used during implementation

Protocol behavior and layouts were cross-checked against public implementations:

- libratbag HID++ 2.0 and on-board profile handling:
  https://github.com/libratbag/libratbag
- Solaar on-board profile parsing/writing:
  https://github.com/pwr-Solaar/Solaar

OpenHub contains its own implementation and does not embed those projects as runtime dependencies.


## v0.2.5 button assignment decoding

Known 0x8100 profile formats place button assignments in four-byte records.

For the layouts currently accepted by OpenHub:

- the base button table begins at profile byte 32;
- the alternate/G-Shift table begins at profile byte 96;
- each record is 4 bytes;
- the descriptor's button count determines how many records are read, capped at 16.

The descriptor mechanical-layout bits indicate whether an alternate G-Shift layer exists.

### Record behaviors currently decoded

OpenHub recognizes these high-nibble behavior classes:

- `0x0` — execute macro reference;
- `0x1` — stop macro reference;
- `0x2` — stop all macros;
- `0x8` — send HID output;
- `0x9` — built-in Logitech function.

For SEND records the second byte selects:

- `0x00` no action;
- `0x01` mouse-button bitmask;
- `0x02` keyboard modifiers + USB HID key code;
- `0x03` HID consumer/media code.

For FUNCTION records OpenHub names documented actions including tilt, DPI next/previous/cycle/default/shift, profile next/previous/cycle, G-Shift, battery status, profile select, mode switch, host button and scroll up/down.

Macro records are shown as sector/address references only. v0.2.5 does not follow, decode or write macro sectors.

Every row retains its original four raw bytes in the UI/report. Unknown behaviors are left unknown.

### Write boundary

Button assignment writes are deliberately absent in v0.2.5. The next step is to compare these profile slot numbers and decoded values with the tested G502's physical controls before exposing persistent remapping.
