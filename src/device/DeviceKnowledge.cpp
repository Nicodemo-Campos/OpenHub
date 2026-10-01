#include "DeviceKnowledge.hpp"

#include <utility>

namespace openhub {
namespace {

QString searchableName(const DeviceInfo& device)
{
    return (device.name + QLatin1Char(' ') + device.reportedNames.join(QLatin1Char(' '))).toLower();
}

Capability implemented(QString name, QString note)
{
    return {std::move(name), QStringLiteral("Implemented"), std::move(note)};
}

Capability detected(QString name, QString note)
{
    return {std::move(name), QStringLiteral("Detected"), std::move(note)};
}

Capability planned(QString name, QString note)
{
    return {std::move(name), QStringLiteral("Planned"), std::move(note)};
}

Capability research(QString name, QString note)
{
    return {std::move(name), QStringLiteral("Research"), std::move(note)};
}

Capability hidAccessCapability(const DeviceInfo& device)
{
    if (device.hidrawNodes.isEmpty()) {
        return {QStringLiteral("HID access"),
                QStringLiteral("Unavailable"),
                QStringLiteral("No hidraw endpoint was associated with this device.")};
    }

    if (!device.readable) {
        return {QStringLiteral("HID access"),
                QStringLiteral("Permission needed"),
                QStringLiteral("%1 endpoint(s) detected, but the current user cannot read them. Install the OpenHub udev rule before protocol probing.")
                    .arg(device.hidrawNodes.size())};
    }

    if (!device.writable) {
        return {QStringLiteral("HID access"),
                QStringLiteral("Read only"),
                QStringLiteral("%1 endpoint(s) are readable, but not writable by the current user.")
                    .arg(device.hidrawNodes.size())};
    }

    return {QStringLiteral("HID access"),
            QStringLiteral("Ready"),
            QStringLiteral("%1 endpoint(s) are readable and writable by the current user.")
                .arg(device.hidrawNodes.size())};
}

QString wirelessNote(const DeviceInfo& device)
{
    if (device.wirelessCapabilities.isEmpty()) {
        return QStringLiteral("No wireless capability is currently identified.");
    }
    return QStringLiteral("Model capability: %1. Current connection is %2.")
        .arg(device.wirelessCapabilities.join(QStringLiteral(" + ")), device.currentConnection);
}

Capability connectionCapability(const DeviceInfo& device)
{
    return implemented(
        QStringLiteral("Connection model"),
        QStringLiteral("Current connection: %1. Device role: %2. %3")
            .arg(device.currentConnection, device.role, wirelessNote(device)));
}

Capability relationCapability(const DeviceInfo& device)
{
    if (device.relatedDevices.isEmpty()) {
        return detected(
            QStringLiteral("Related interfaces"),
            QStringLiteral("No companion receiver/direct interface was linked during this scan."));
    }

    return implemented(
        QStringLiteral("Related interfaces"),
        device.relatedDevices.join(QStringLiteral(" | ")));
}

} // namespace

SupportProfile DeviceKnowledge::analyze(const DeviceInfo& device)
{
    const QString name = searchableName(device);

    if (name.contains(QStringLiteral("g502"))) {
        return {
            SupportLevel::KnownFamily,
            QStringLiteral("Known family"),
            QStringLiteral("Logitech G502 family"),
            QStringLiteral("OpenHub recognizes the G502 family and now separates the active USB path from the mouse's LIGHTSPEED capability. v0.1.1 still sends no device commands."),
            {
                implemented(QStringLiteral("Device discovery"), QStringLiteral("VID/PID, USB/HID identity and kernel-reported names.")),
                connectionCapability(device),
                relationCapability(device),
                hidAccessCapability(device),
                planned(QStringLiteral("DPI"), QStringLiteral("Targeted for the first HID++ control backend.")),
                planned(QStringLiteral("Polling rate"), QStringLiteral("Targeted for the first HID++ control backend.")),
                planned(QStringLiteral("Buttons & profiles"), QStringLiteral("Requires safe HID++ capability probing and write support.")),
                planned(QStringLiteral("Lighting"), QStringLiteral("Will only be exposed when the device reports a supported lighting feature."))
            }
        };
    }

    if (name.contains(QStringLiteral("g915"))) {
        return {
            SupportLevel::KnownFamily,
            QStringLiteral("Known family"),
            QStringLiteral("Logitech G915 X family"),
            QStringLiteral(
                "OpenHub recognizes the G915 X family. v0.3.1.1 hardware-validates the wired 0x8081 direct-frame path after the earlier 0x8071 Static experiment proved unsuitable for this keyboard."),
            {
                implemented(QStringLiteral("Device discovery"), QStringLiteral("VID/PID, USB/HID identity and kernel-reported names.")),
                connectionCapability(device),
                relationCapability(device),
                hidAccessCapability(device),
                implemented(QStringLiteral("Battery"), QStringLiteral("Unified Battery telemetry is read when the device exposes 0x1004.")),
                implemented(QStringLiteral("RGB effects discovery"), QStringLiteral("0x8071 clusters and device-reported effect metadata are enumerated at runtime.")),
                implemented(QStringLiteral("Transient direct RGB test"), QStringLiteral("v0.3.1.1 hardware-validated a volatile solid 0x8081 frame after re-validating the exact wired feature signature and 126-address map.")),
                implemented(QStringLiteral("Per-key address discovery"), QStringLiteral("0x8081 bitmap banks are decoded without claiming software lighting control.")),
                research(QStringLiteral("Per-key lighting writes"), QStringLiteral("The whole-board 0x8081 SET/commit path is hardware-validated; selective-key editing is the next validation step before a full editor.")),
                planned(QStringLiteral("Brightness & profiles"), QStringLiteral("0x8040 / 0x8101 support will be added behind feature-specific validation gates."))
            }
        };
    }

    if (name.contains(QStringLiteral("a50 x"))
        || name.contains(QStringLiteral("a50x"))
        || (name.contains(QStringLiteral("astro")) && name.contains(QStringLiteral("a50")))) {
        return {
            SupportLevel::KnownFamily,
            QStringLiteral("Known family"),
            QStringLiteral("ASTRO A50 X family"),
            QStringLiteral("OpenHub recognizes the A50 X USB/base-station interface and keeps its wireless capability separate from the current USB connection. Advanced controls remain research-only."),
            {
                implemented(QStringLiteral("Device discovery"), QStringLiteral("USB/HID identity and endpoints.")),
                connectionCapability(device),
                hidAccessCapability(device),
                research(QStringLiteral("Battery"), QStringLiteral("Protocol support must be verified on real hardware.")),
                research(QStringLiteral("EQ"), QStringLiteral("No write commands are sent in v0.1.1.")),
                research(QStringLiteral("Sidetone"), QStringLiteral("No write commands are sent in v0.1.1.")),
                research(QStringLiteral("ChatMix"), QStringLiteral("No write commands are sent in v0.1.1.")),
                research(QStringLiteral("Microphone controls"), QStringLiteral("No write commands are sent in v0.1.1."))
            }
        };
    }

    if (device.isLogitechFamily) {
        return {
            SupportLevel::LogitechDetected,
            QStringLiteral("Logitech detected"),
            QStringLiteral("Logitech / ASTRO device"),
            QStringLiteral("The vendor/device is recognizable, but OpenHub does not yet have a model-specific control profile for it."),
            {
                implemented(QStringLiteral("Device discovery"), QStringLiteral("The device can be inspected without opening its hidraw endpoints.")),
                connectionCapability(device),
                hidAccessCapability(device),
                planned(QStringLiteral("Capability probing"), QStringLiteral("Future versions will query supported HID++ features safely.")),
                planned(QStringLiteral("Controls"), QStringLiteral("Controls will appear only after a capability is positively identified."))
            }
        };
    }

    return {
        SupportLevel::GenericHid,
        QStringLiteral("HID detected"),
        QStringLiteral("Generic HID device"),
        QStringLiteral("OpenHub can see this HID device, but it is outside the currently managed Logitech/ASTRO family."),
        {
            implemented(QStringLiteral("Device discovery"), QStringLiteral("Read-only Linux sysfs enumeration.")),
            connectionCapability(device),
            hidAccessCapability(device),
            research(QStringLiteral("Device support"), QStringLiteral("No protocol assumptions are made for unknown hardware."))
        }
    };
}

} // namespace openhub
