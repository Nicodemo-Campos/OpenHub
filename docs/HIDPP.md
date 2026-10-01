# HID++ probing, reads, and validated controls in OpenHub

OpenHub follows **discovery before control**.

## v0.2.3: profile-aware report-rate control

The G502 exposes both:

- Adjustable Report Rate `0x8060`;
- On-board Profiles `0x8100`.

When on-board mode is active, direct `0x8060 SetReportRate` can be rejected even though the same feature remains readable. In that mode, report rate is part of the active profile.

OpenHub v0.2.3 therefore uses two paths:

- **Host mode:** direct `0x8060` write.
- **On-board mode:** update the report-rate byte inside the active `0x8100` profile sector.

## 0x8100 memory flow

OpenHub first reads `GetProfilesDescriptor` and currently accepts only the family of layouts already used by mature HID++ implementations:

- memory model `0x01`;
- profile format `0x01` through `0x05`;
- macro format `0x01`;
- bounded, 16-byte-aligned sector sizes.

A profile-memory write is not enabled unless the descriptor, directory and active profile can all be validated.

### Directory

Sector `0x0000` is read in 16-byte blocks through `MemoryRead`.

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

Memory addressing and data writes require HID++ long reports because their payloads exceed the 3-byte short-report parameter area.

## CRC

Profile sectors use CRC-16/CCITT with seed `0xFFFF`.

The CRC is calculated over every sector byte except the final two bytes and then stored big-endian in those final two bytes.

OpenHub validates the original CRC before writing and the new CRC after read-back.

## Live verification and rollback

After a profile sector verifies in flash, OpenHub reads the live report rate through `0x8060`.

If firmware has not reloaded the modified active profile, OpenHub re-selects the same profile choice and checks again.

If live verification still fails, OpenHub attempts to write the original sector back and re-select the same profile.

## DPI

DPI remains on the v0.2.2 active-state path for now:

- read sensor count and supported DPI range/list;
- validate the requested value;
- `SetSensorDpi`;
- verify with `GetSensorDpi`.

A later profile milestone can persist DPI slots without mixing that work into the first profile-memory write.

## Intentionally excluded in v0.2.3

No writes are made to:

- profile directory entries;
- profile DPI tables;
- buttons or macros;
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
