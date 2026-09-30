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

} // namespace

SupportProfile DeviceKnowledge::analyze(const DeviceInfo& device)
{
    const QString name = searchableName(device);
    const QString hidPermission = device.hidrawNodes.isEmpty()
        ? QStringLiteral("No hidraw endpoint was associated with this device.")
        : QStringLiteral("%1 endpoint(s); filesystem permissions: %2%3.")
              .arg(device.hidrawNodes.size())
              .arg(device.readable ? QStringLiteral("read") : QStringLiteral("no-read"))
              .arg(device.writable ? QStringLiteral(" + write") : QStringLiteral(""));

    if (name.contains(QStringLiteral("g502"))) {
        return {
            SupportLevel::KnownFamily,
            QStringLiteral("Known family"),
            QStringLiteral("Logitech G502 family"),
            QStringLiteral("OpenHub recognizes this family. v0.1 only performs safe, read-only discovery; configuration backends are intentionally disabled."),
            {
                implemented(QStringLiteral("Device discovery"), QStringLiteral("VID/PID, transport and kernel-reported names.")),
                detected(QStringLiteral("HID access"), hidPermission),
                planned(QStringLiteral("DPI"), QStringLiteral("Targeted for the next control-backend milestone.")),
                planned(QStringLiteral("Polling rate"), QStringLiteral("Targeted for the next control-backend milestone.")),
                planned(QStringLiteral("Buttons & profiles"), QStringLiteral("Requires HID++ capability probing and write support.")),
                planned(QStringLiteral("Lighting"), QStringLiteral("Will only be exposed when the device reports a supported lighting feature."))
            }
        };
    }

    if (name.contains(QStringLiteral("g915"))) {
        return {
            SupportLevel::KnownFamily,
            QStringLiteral("Known family"),
            QStringLiteral("Logitech G915 family"),
            QStringLiteral("OpenHub recognizes this keyboard family. v0.1 identifies it without sending commands to the keyboard."),
            {
                implemented(QStringLiteral("Device discovery"), QStringLiteral("VID/PID, transport and kernel-reported names.")),
                detected(QStringLiteral("HID access"), hidPermission),
                planned(QStringLiteral("Per-key lighting"), QStringLiteral("Planned through a dedicated HID++ lighting backend.")),
                planned(QStringLiteral("Brightness & effects"), QStringLiteral("Will be capability-gated rather than model-list gated.")),
                planned(QStringLiteral("Profiles"), QStringLiteral("Profile storage and automatic switching are future milestones.")),
                planned(QStringLiteral("Battery"), QStringLiteral("Battery reporting will be enabled only when safely queryable."))
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
            QStringLiteral("OpenHub recognizes the A50 X family, but its advanced control protocol still needs to be documented before write support is enabled."),
            {
                implemented(QStringLiteral("Device discovery"), QStringLiteral("USB/HID identity and endpoints.")),
                detected(QStringLiteral("HID access"), hidPermission),
                research(QStringLiteral("Battery"), QStringLiteral("Protocol support must be verified on real hardware.")),
                research(QStringLiteral("EQ"), QStringLiteral("No write commands are sent in v0.1.")),
                research(QStringLiteral("Sidetone"), QStringLiteral("No write commands are sent in v0.1.")),
                research(QStringLiteral("ChatMix"), QStringLiteral("No write commands are sent in v0.1.")),
                research(QStringLiteral("Microphone controls"), QStringLiteral("No write commands are sent in v0.1."))
            }
        };
    }

    if (device.isLogitechFamily) {
        return {
            SupportLevel::LogitechDetected,
            QStringLiteral("Logitech detected"),
            QStringLiteral("Logitech / ASTRO device"),
            QStringLiteral("The vendor or kernel-reported identity is recognized, but OpenHub does not yet have a model-specific control profile for it."),
            {
                implemented(QStringLiteral("Device discovery"), QStringLiteral("The device can be inspected without opening its hidraw endpoints.")),
                detected(QStringLiteral("HID access"), hidPermission),
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
            detected(QStringLiteral("HID access"), hidPermission),
            research(QStringLiteral("Device support"), QStringLiteral("No protocol assumptions are made for unknown hardware."))
        }
    };
}

} // namespace openhub
