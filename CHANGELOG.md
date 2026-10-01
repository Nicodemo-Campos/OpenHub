# Changelog

## 0.3.1

- Added the first explicit transient RGB write test for the hardware-validated wired G915 X signature.
- The backend requires device index 0x01, RGB Effects 0x8071 v4, Per-Key Lighting v2 0x8081 v0, Profile Management 0x8101, cluster 0 at Primary, and a device-advertised Static effect.
- Static is located dynamically by effect ID 0x0001; no effect-index assumption is hardcoded.
- The test switches 0x8101 to host mode, claims 0x8071 software control, and sends a volatile Primary Static record with persist=0.
- Added a five-second auto-release back to 0x8071 firmware control and 0x8101 firmware profile mode.
- Added manual release and best-effort dialog-close release paths.
- Added an obvious magenta default plus editable RGB values for physical validation.
- Added transient test/release activity to the copyable control report.
- No 0x8081 per-key SET, range/batch write, or frame commit is enabled yet.
- Corrected G915 X continuity notes to reflect the actual hardware report: 0x8081 feature version is v0, despite the feature name “Per-Key Lighting v2”.

## 0.3.0

- Started the Logitech G915 X milestone with a non-mutating lighting inspector.
- Added read-only HID++ 0x8071 RGB Effects discovery.
- Resolves the 0x8071 feature index at runtime and enumerates cluster count, cluster location, persistency flags, effect IDs, capability bits, and effect-period metadata.
- Added read-only HID++ 0x8081 Per-Key Lighting v2 address discovery.
- Reads the three per-key bitmap banks and decodes the addressable zone-ID universe.
- Added compact per-key address ranges and raw bitmap banks to the copyable control report.
- Added a dedicated G915 X lighting-discovery UI panel.
- Existing Unified Battery 0x1004 telemetry continues to work through the generic live-state backend.
- Updated G915 X support status in Device Knowledge.
- No G915 X software-control claim, RGB effect SET, per-key SET, or frame commit is issued in v0.3.0.
- Added docs/CONTINUITY.md with hardware IDs, validated milestones, safety boundaries, protocol facts, code landmarks, and next-step guidance.

## 0.2.7.1

- Hardware-validation hotfix for G502 Color LED Effects.
- Confirmed the tested G502 LIGHTSPEED reports both Primary and Logo zones, but only the Primary profile record produced an observed physical lighting change.
- Persistent lighting writes are now restricted to zone 0 when its reported location is Primary (0x0001).
- Reported non-Primary zones remain visible with their device-enumerated effects but are read-only.
- Backend re-validates the zone location before writing; the UI cannot bypass the restriction.
- Lighting success text now distinguishes profile-memory verification from live/physical LED verification when 0x8070 does not expose readable effect settings.
- Control reports label each lighting zone as either Primary hardware-validated or read-only with an unvalidated physical mapping.
- The previous Logo test record is not silently rewritten or reverted by this hotfix.

## 0.2.7

- Added HID++ 0x8070 Color LED Effects discovery.
- Reads zone count, location, persistency flags, supported effect IDs/capabilities, and live zone state when readable.
- Added device-enumerated lighting controls; the UI does not invent unsupported effects.
- Added persistent Off, Static, Color cycle, and Breathing effects for validated G502 profile format 0x03.
- Added RGB controls for Static/Breathing and period/intensity controls for Cycle/Breathing.
- Persistent lighting writes replace exactly one 11-byte normal lighting record in the active profile sector.
- Full-sector CRC/read-back, profile reload, DPI-stage restoration, final sector verification, and optional live 0x8070 verification are required.
- Added verified rollback to the original complete sector after lighting write/reload/live-verification failures.
- Added Color LED zone details to the copyable control report.
- Complex/unknown effects, alternate lighting records, custom animations, macros, profile directory, and firmware remain write-disabled.

## 0.2.6

- Added the first persistent on-board button remapper for validated G502-class 0x8100 profiles.
- Added a typed remap API; callers cannot submit arbitrary raw four-byte profile records.
- Supports no-action, single mouse-button outputs, and a narrow set of documented built-in Logitech functions.
- Protects Base Button 1 and Base Button 2 in this first remapping release.
- Refuses to overwrite macro-backed, keyboard HID, consumer/media HID, unknown, invalid, or out-of-scope built-in records.
- Restricts button writes to the hardware-validated profile format 0x03 while keeping read-only decoding broader.
- Changes exactly one four-byte button record in a cloned active profile sector and recomputes CRC.
- Requires full-sector read-back equality before reloading the active profile.
- Restores the previous active DPI stage after profile reload when possible.
- Re-reads the sector after reload and requires the new record to persist.
- Added verified rollback of the complete original sector when write/read-back/reload/final verification fails.
- Added a UI remap selector with only the validated v0.2.6 action subset.
- Macros, keyboard/consumer remaps, unknown records, lighting, profile directory and firmware remain write-disabled.

## 0.2.5

- Added read-only decoding of the active 0x8100 profile button-assignment table.
- Reads the descriptor-reported button count, capped at the 16 records present in the known profile layouts.
- Detects and displays the G-Shift alternate assignment layer when advertised by the profile descriptor.
- Decodes SEND mappings for mouse buttons, keyboard HID keys/modifiers, consumer/media keys, and no-action records.
- Decodes built-in FUNCTION mappings such as DPI Shift, DPI cycling, profile cycling, G-Shift, battery status and scroll functions.
- Identifies macro execute/stop references without parsing or modifying macro sectors.
- Preserves unknown assignment types as raw four-byte records rather than guessing.
- Added a read-only button-assignment table to the HID++ controls UI.
- Added raw button records and decoded assignments to the copyable control report.
- Kept button remapping and macro writes disabled pending real-hardware slot-order validation.

## 0.2.4

- Added full readout of the five DPI slots stored in the active 0x8100 profile.
- Added current/default/DPI-shift stage identification.
- Added GetCurrentDpiIndex and SetCurrentDpiIndex support.
- Added an on-board DPI-stage editor with enable/disable state per slot.
- Added persistent active-profile DPI-stage writes.
- Validates every enabled slot against the live 0x2201 DPI range/list and step before writing.
- Requires at least one enabled stage and an enabled default stage.
- Clones the existing profile sector and changes only the default-stage byte, five DPI values, and CRC.
- Verifies the complete profile sector after writing, reloads the same profile, restores a valid active stage, and verifies live DPI.
- Attempts to restore the original profile sector if final live verification fails.
- Added on-board DPI-stage details to the copyable control report.
- Button bindings, macros, lighting, profile directory entries, and firmware remain write-disabled.

## 0.2.3.2

- Fixed support for HID++ 0x8100 profile sectors whose size is not a multiple of 16 bytes.
- The G502 LIGHTSPEED reports a 255-byte profile sector, which is valid and is also handled by mature HID++ implementations.
- Removed the incorrect 16-byte-alignment safety requirement while keeping bounded sector-size, CRC, directory, profile, and read-back validation.
- Profile writes still declare the exact sector byte count; the final HID++ long report may contain fewer than 16 meaningful bytes.
- The report-rate selector now remains interactive even when saving is blocked, so supported rates can still be inspected and a safety lock no longer looks like a broken combo box.

## 0.2.3.1

- Fixed active-profile resolution for HID++ 0x8100 devices that may report profile indexes as either zero-based or one-based.
- OpenHub now tests both index interpretations against the CRC-valid profile directory and profile sectors.
- Uses the current live report rate to disambiguate the active profile when both index interpretations are structurally valid.
- Keeps persistent report-rate writes blocked if the active profile cannot be resolved unambiguously.
- Added clearer UI diagnostics explaining exactly which profile-memory safety gate blocked report-rate editing.

## 0.2.3

- Added a persistent report-rate backend for active HID++ 0x8100 on-board profiles.
- Reads and validates the profile-memory descriptor before enabling writes.
- Reads and CRC-checks the user profile directory and active profile sector.
- Clones the active profile sector and changes only the report-rate byte.
- Recomputes the Logitech HID++ CRC-CCITT before writing.
- Writes profile memory through 0x8100 address/data/end commands using long HID++ reports.
- Reads the entire sector back and requires an exact match after the write.
- Verifies the live 0x8060 report rate and re-selects the same profile when firmware needs a reload.
- Attempts to restore the original sector if the persistent write verifies in flash but not in live state.
- Added a confirmation dialog before persistent profile-memory writes.
- Keeps profile directory entries, profile DPI slots, button bindings, macros, lighting, names, power settings, and firmware untouched.

## 0.2.2.1

- Fixed report-rate handling on devices with On-board Profiles (0x8100).
- Added live detection of the current on-board/host mode.
- Prevents direct 0x8060 report-rate SET while on-board profiles are enabled, avoiding firmware INVALID_ARGUMENT errors.
- Keeps DPI control available when its direct 0x2201 SET is accepted.
- Added readable HID++ error names, including INVALID_ARGUMENT (0x02).
- The UI now explains when report rate is controlled by the active on-board profile instead of presenting a broken Apply action.

## 0.2.2

- Added the first intentional HID++ configuration writes.
- Added active DPI SET support for Adjustable DPI (0x2201).
- Added active report-rate SET support for Adjustable Report Rate (0x8060).
- Re-reads device-supported ranges/masks before every write.
- Rejects unsupported DPI/report-rate values before sending a SET.
- Performs a protocol identity check immediately before configuration.
- Verifies every successful SET with a corresponding GET.
- Added capability-driven DPI/report-rate controls to the HID++ dialog.
- Added configuration actions and SET/verification traces to the copyable control report.
- Kept profile-memory, lighting, remap, DFU, receiver-child, and A50 X writes disabled.

## 0.2.1

- Added read-only HID++ live-state queries after capability discovery.
- Added Adjustable DPI (0x2201) sensor/range/current/default reads.
- Added Adjustable Report Rate (0x8060) supported/current reads.
- Added Battery Voltage (0x1001) voltage/status reads with an explicitly approximate voltage-derived percentage.
- Added Unified Battery (0x1004) percentage/status reads.
- Added Battery Status (0x1000) read support for compatible future devices.
- Added a Live State table and copyable combined state/probe report.
- Expanded feature names for IDs observed on the G502/G915 X.
- Fixed feature-version reporting: Root now uses the version returned by Feature Set instead of the HID++ protocol major version.
- Kept all configuration/SET commands disabled.

## 0.2.0

- Added explicit, non-mutating HID++ capability probing.
- Added HID report-descriptor parsing to identify report 0x10/0x11 vendor endpoints.
- Added runtime HID++ device-index detection for directly attached Logitech devices.
- Added Root.GetProtocolVersion probing.
- Added runtime Feature Set enumeration and feature-version resolution.
- Added readable names for common Logitech HID++ feature IDs.
- Added high-level capability grouping for DPI, report rate, battery, buttons/remapping, lighting, and profiles.
- Added a copyable HID++ probe report with raw TX/RX trace.
- Kept startup discovery passive.
- Kept all configuration/SET commands disabled.
- Kept receiver-child and A50 X protocol probing disabled pending dedicated transports.

## 0.1.1

- Separated current connection state from wireless capability.
- Distinguished physical devices, LIGHTSPEED receivers, and the A50 X base-station interface.
- Added related-interface linking.
- Added effective hidraw ACL checks.
- Added a session-scoped Logitech udev rule using `TAG+="uaccess"`.

## 0.1.0

- Initial Linux USB/HID discovery.
- Device Inspector and copyable diagnostics.
- Initial G502, G915 X, and A50 X family recognition.
- No device protocol traffic.
