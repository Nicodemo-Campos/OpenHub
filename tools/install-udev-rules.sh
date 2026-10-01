#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run this installer as root, for example:"
    echo "  sudo ./tools/install-udev-rules.sh"
    exit 1
fi

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_RULE="${SOURCE_DIR}/packaging/udev/70-openhub-logitech.rules"
DEST_RULE="/etc/udev/rules.d/70-openhub-logitech.rules"

if [[ ! -f "${SOURCE_RULE}" ]]; then
    echo "OpenHub udev rule not found: ${SOURCE_RULE}" >&2
    exit 1
fi

install -Dm0644 "${SOURCE_RULE}" "${DEST_RULE}"
udevadm control --reload-rules

echo "Installed: ${DEST_RULE}"
echo
echo "Reconnect the Logitech/ASTRO devices (or log out and back in), then press"
echo "'Rescan devices' in OpenHub. The HID access line should become 'read + write'."
echo
echo "OpenHub does not require chmod 666 on /dev/hidraw*."
