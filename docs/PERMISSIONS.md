# HID permissions

OpenHub v0.1.1 still performs read-only discovery, but the next protocol-probing milestone needs access to the relevant `/dev/hidraw*` endpoints.

A default Linux installation often exposes the devices through sysfs while denying a normal user direct hidraw access. In OpenHub this appears as:

    HID access: permission needed

## Recommended rule

OpenHub ships:

    packaging/udev/70-openhub-logitech.rules

The rule is intentionally narrow:

    SUBSYSTEM=="hidraw", KERNEL=="hidraw*", ATTRS{idVendor}=="046d", TAG+="uaccess"

It targets Logitech's USB vendor ID and uses the desktop-session `uaccess` mechanism. It does **not** make every HID device world-writable.

Install it with:

    sudo ./tools/install-udev-rules.sh

Then reconnect the affected devices. Logging out and back in can also be necessary on some desktop/session setups.

Afterward, run OpenHub and press **Rescan devices**. A usable endpoint should report:

    HID access: read + write

## Do not use broad hidraw permissions

OpenHub does not recommend rules such as:

    KERNEL=="hidraw*", MODE="0666"

or manually running:

    chmod 666 /dev/hidraw*

Those approaches grant every local process broad write access to input/HID hardware and are unnecessary for OpenHub.

## If access is still blocked

Useful checks are:

    getfacl /dev/hidrawX
    udevadm info -a -n /dev/hidrawX
    loginctl session-status

Replace `hidrawX` with an endpoint shown by OpenHub.

Do not run the OpenHub GUI as root as a workaround. Permission handling belongs in udev/session policy, not in the application process.
