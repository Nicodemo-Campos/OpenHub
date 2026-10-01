# HID++ probing, reads, and validated controls in OpenHub

OpenHub follows **discovery before control**.

## v0.2.7: profile-aware controls, safe remapping and Color LED Effects

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


## v0.2.6 persistent button remapping

v0.2.6 adds a narrow writer on top of the v0.2.5 four-byte assignment decoder.

The public backend API accepts a typed request rather than arbitrary bytes:

- `NoAction`;
- `MouseButton` with exactly one documented mouse-output bit;
- `BuiltInFunction` from the explicit v0.2.6 allow-list.

The backend performs the encoding.

### Encodings written

No action:

    80 00 FF FF

Single mouse output:

    80 01 HH LL

where `HH LL` is one allowed mouse-button bit mask.

Built-in function:

    90 FF 00 00

where `FF` is one allowed function code (tilt, DPI navigation/default/shift, profile navigation, G-Shift, battery status, or scroll up/down).

Profile-select, mode-switch and host-button functions are intentionally not in the first write allow-list because their extra semantics are not needed for the initial remapper.

### Existing-record protection

Even when the requested target action is safe, OpenHub refuses to overwrite an existing record if it is:

- a macro execute/stop record;
- keyboard HID output;
- consumer HID output;
- unknown/invalid;
- a built-in function outside the narrow allow-list;
- a built-in function carrying non-zero reserved/data bytes.

Base Button 1 and Base Button 2 are also protected in the UI/backend for v0.2.6.

### Profile/layout gate

Read-only parsing continues to support the known 0x8100 profile-format family.

Persistent **button** writes are narrower in v0.2.6 and require profile format `0x03`, the layout validated on the G502 LIGHTSPEED hardware used for this milestone.

### Write and rollback

The writer resolves the active CRC-valid sector, clones it, replaces one record at:

- `32 + (buttonIndex - 1) * 4` for Base;
- `96 + (buttonIndex - 1) * 4` for G-Shift;

then recomputes CRC and writes the full sector.

OpenHub requires:

1. exact full-sector read-back;
2. successful reload of the same active profile;
3. restoration of the previous current-DPI index when it remains valid;
4. another CRC-valid full-sector read after reload;
5. exact persistence of the new four-byte mapping.

Failure after the flash write triggers a complete original-sector rollback attempt. Rollback itself is read back and compared with the saved original before it is considered successful.


## v0.2.7 Color LED Effects (0x8070)

OpenHub now enumerates feature 0x8070 before exposing lighting controls.

The read path uses:

- function 0x00 — device/zone count and capability flags;
- function 0x10 — per-zone location and effect count;
- function 0x20 — per-zone effect-index metadata, including the real effect ID;
- function 0xE0 — current zone effect/settings when readable.

OpenHub treats effect **index** and effect **ID** as separate values. The UI is built from the device-reported effect IDs and does not assume that an effect index equals its semantic effect ID.

### Profile records

For the validated G502/G900-style profile format 0x03, each normal lighting record is 11 bytes:

- zone 0 at byte 208;
- zone 1 at byte 219.

The first byte is the profile effect ID. The narrow v0.2.7 writer encodes:

- 0x00 Off;
- 0x01 Static RGB;
- 0x03 Color cycle (period + intensity);
- 0x0A Breathing (RGB + period + default waveform + intensity).

An encoded intensity byte of 0 represents 100%, matching established HID++ profile handling.

### Write boundary

Before changing a lighting record OpenHub re-reads 0x8070 and requires the requested effect ID to appear in that exact zone's effect list.

Persistent writes additionally require:

- on-board mode enabled;
- CRC-valid active user profile;
- profile format 0x03;
- zone index 0;
- reported zone location `Primary` (`0x0001`).

The writer changes only the first 11-byte normal-lighting record (Primary, byte 208) plus CRC in a full-sector clone. The second reported record at byte 219 and all alternate/custom lighting data are preserved byte-for-byte.

After writing, OpenHub requires full-sector equality, reloads the same profile, restores the previous current DPI stage when valid, and reads the sector again. When 0x8070 current-effect reads are available, effect-specific live parameters are verified too.

A failed post-write check triggers an original-sector rollback attempt with read-back verification.


## v0.2.7.1 lighting validation hotfix

Real G502 LIGHTSPEED testing reported two 0x8070 zones:

- zone 0 / location Primary;
- zone 1 / location Logo.

Both zones reported the same effect IDs, but the device also reported no readable current-effect settings through 0x8070. A persistent write to the Primary profile record produced the expected visible hardware change. A persistent write to the second Logo record passed CRC/full-sector verification but produced no observed physical LED change.

Therefore v0.2.7.1 separates **protocol-advertised capability** from **hardware-validated write mapping**:

- Primary (zone 0, location 0x0001) remains write-enabled;
- all other reported zones stay visible/readable-at-the-metadata-level but are write-disabled;
- the backend enforces the same restriction even if called outside the UI;
- when live effect settings are unavailable, a successful write is described as profile-memory verified, not physical-LED verified.

This avoids treating a correctly written profile record as proof that the record drives a visible LED.


## v0.3.0 G915 X read-only lighting discovery

The first G915 X milestone intentionally performs no lighting mutation.

### RGB Effects — 0x8071

OpenHub resolves feature 0x8071 dynamically and uses function 0 with the established selector tuples:

- `FF FF 00` — device/general information, including cluster count;
- `<cluster> FF 00` — cluster metadata;
- `<cluster> <effectIndex> 00` — effect metadata.

For each cluster OpenHub records:

- cluster index;
- location;
- effect count;
- persistency flags;
- effect ID;
- effect capability bits;
- effect-period metadata.

No assumption is made that effect index equals effect ID.

### Per-Key Lighting v2 — 0x8081

OpenHub issues three read-only function-0 bitmap queries:

- `00 00`;
- `00 01`;
- `00 02`.

The two echoed request bytes are removed from each returned payload and the remaining bitmap bytes are concatenated. Zone IDs 1..254 are then decoded from the bitset.

This is capability/address discovery only. 0x8081 does not expose a true live read-back of the current per-key RGB buffer, so OpenHub does not display invented per-key colors.

### v0.3.0 write boundary

No G915 X lighting write is enabled in v0.3.0.

Specifically, OpenHub does not issue:

- 0x8071 software-control claims;
- 0x8071 firmware-effect SETs;
- 0x8081 individual-zone SETs;
- 0x8081 range/batch SETs;
- 0x8081 frame commit.

The next write milestone must be preceded by hardware validation of the discovered cluster layout and address universe on the actual G915 X.


## v0.3.1 G915 X transient Primary Static test

v0.3.0 hardware validation established the first writable signature used by v0.3.1:

- wired HID++ device index `0x01`;
- RGB Effects `0x8071` feature version 4;
- Per-Key Lighting v2 `0x8081` feature version **0**;
- Profile Management `0x8101`;
- cluster index 0 reports location Primary (`0x0001`);
- that cluster advertises Static effect ID `0x0001`.

The name “Per-Key Lighting v2” is the feature family name; the tested keyboard's HID++ feature-version field is v0.

### Runtime validation

The writer does not trust the v0.3.0 cached metadata alone. Before the test it:

1. resolves `0x8071`, `0x8081` and `0x8101` again;
2. requires the expected feature versions;
3. re-reads 0x8071 device/cluster information;
4. requires cluster 0 location Primary;
5. enumerates the cluster's effect cards and locates effect ID `0x0001`;
6. uses the returned **effect index** in the SET command.

### Claim and volatile SET

The narrow v0.3.1 sequence is:

- `0x8101` function 6 / wire `0x60`, payload `05` — switch Profile Management to host mode;
- `0x8071` function 5 / wire `0x50`, payload `01 03 04` — SET software control, mode 3, conservative NV-config flag;
- `0x8071` function 1 / wire `0x10`, long-report payload:
  - byte 0: Primary cluster index 0;
  - byte 1: dynamically discovered Static effect index;
  - bytes 2..4: R, G, B;
  - byte 5: `02` fixed-colour marker for a non-black Static color;
  - byte 12: `00` — **non-persistent / volatile**.

Black is rejected in the first test so a successful physical change is visually obvious.

### Release

The test releases with:

- `0x8071` function 5 payload `01 00 00`;
- `0x8101` function 6 payload `03`.

The UI auto-releases after about five seconds, supports manual release, and performs best-effort release when the HID++ controls dialog closes.

### Explicit v0.3.1 boundary

No `0x8081` write function is called by the transient test. In particular there is no individual-key SET, range/batch SET, or FrameEnd commit.

This milestone validates the 0x8071 control handoff independently from the more complex per-key takeover/prep path.


## v0.3.1.1 G915 X direct-frame correction

Real v0.3.1 hardware testing showed that the keyboard ACKed the 0x8101 host-mode transition, the 0x8071 software-control claim, and the 0x8071 Primary Static request, but the visible keyboard lighting went fully dark instead of showing the requested color.

The v0.3.1 Static experiment is therefore hardware-invalidated and is no longer used.

The replacement v0.3.1.1 test targets the runtime per-key buffer:

- require wired device index 0x01;
- require 0x8071 v4 and 0x8081 v0;
- re-read bitmap banks 0, 1 and 2;
- require the exact validated address universe: 0x01–0x6F, 0x99, 0x9B–0x9E, 0xB4–0xBC, 0xD2;
- perform the C356 software-control handshake;
- issue 0x8081 function 5 SetRange for each contiguous run;
- issue one 0x8081 function 7 FrameEnd/commit;
- release 0x8071 software control after the observation window.

Unlike the failed v0.3.1 experiment, this hotfix does not write an 0x8071 effect record and does not switch 0x8101 Profile Management.
