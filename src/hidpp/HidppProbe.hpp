#pragma once

#include "../device/DeviceInfo.hpp"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace openhub {

struct HidppFeatureInfo {
    quint16 id{0};
    quint8 index{0};
    quint8 type{0};
    int version{-1};
    QString name;
};

struct HidppProbeResult {
    bool success{false};
    QString endpoint;
    quint8 deviceIndex{0};
    int protocolMajor{-1};
    int protocolMinor{-1};
    QString error;
    QStringList candidateEndpoints;
    QStringList trace;
    QVector<HidppFeatureInfo> features;
};

class HidppProbe {
public:
    [[nodiscard]] static bool isEligible(const DeviceInfo& device);
    [[nodiscard]] static HidppProbeResult probe(const DeviceInfo& device);
    [[nodiscard]] static QString featureName(quint16 featureId);
    [[nodiscard]] static QString formatReport(const DeviceInfo& device,
                                              const HidppProbeResult& result);
};

} // namespace openhub
