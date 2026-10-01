# Changelog

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
