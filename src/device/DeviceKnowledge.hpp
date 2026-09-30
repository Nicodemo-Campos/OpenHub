#pragma once

#include "DeviceInfo.hpp"

#include <QList>
#include <QString>

namespace openhub {

struct Capability {
    QString name;
    QString status;
    QString note;
};

enum class SupportLevel {
    KnownFamily,
    LogitechDetected,
    GenericHid
};

struct SupportProfile {
    SupportLevel level{SupportLevel::GenericHid};
    QString badge;
    QString title;
    QString summary;
    QList<Capability> capabilities;
};

class DeviceKnowledge {
public:
    [[nodiscard]] static SupportProfile analyze(const DeviceInfo& device);
};

} // namespace openhub
