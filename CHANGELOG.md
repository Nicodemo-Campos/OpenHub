# Changelog

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
