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

struct HidppLiveValue {
    QString name;
    QString current;
    QString details;
    quint16 featureId{0};
};

struct HidppDpiState {
    bool available{false};
    quint8 sensorIndex{0};
    quint16 currentDpi{0};
    quint16 defaultDpi{0};
    quint16 minimumDpi{0};
    quint16 maximumDpi{0};
    quint16 stepDpi{0};
    QVector<quint16> supportedValues;
};

struct HidppReportRateState {
    bool available{false};
    quint8 currentIntervalMs{0};
    QVector<quint8> supportedIntervalsMs;
};

struct HidppOnboardProfileState {
    bool present{false};
    bool metadataReady{false};
    bool writableLayout{false};
    bool directoryCrcValid{false};
    bool profileCrcValid{false};
    quint8 mode{0};
    quint8 memoryModel{0};
    quint8 profileFormat{0};
    quint8 macroFormat{0};
    quint8 profileCount{0};
    quint8 sectorCount{0};
    quint16 sectorSize{0};
    quint16 activeChoice{0xFFFF};
    int activeIndex{-1};
    quint16 activeSector{0xFFFF};
    bool activeEnabled{false};
    quint8 activeReportIntervalMs{0};
};

struct HidppLiveStateResult {
    bool success{false};
    QString error;
    QStringList warnings;
    QStringList trace;
    QVector<HidppLiveValue> values;
    QVector<HidppDpiState> dpiSensors;
    HidppReportRateState reportRate;
    HidppOnboardProfileState onboardProfile;
    bool configurationWriteAttempted{false};
    QStringList configurationActions;
};

struct HidppWriteResult {
    bool success{false};
    QString error;
    QString summary;
    QStringList trace;
};

class HidppProbe {
public:
    [[nodiscard]] static bool isEligible(const DeviceInfo& device);
    [[nodiscard]] static HidppProbeResult probe(const DeviceInfo& device);
    [[nodiscard]] static HidppLiveStateResult readLiveState(
        const DeviceInfo& device,
        const HidppProbeResult& probeResult);
    [[nodiscard]] static HidppWriteResult setDpi(
        const HidppProbeResult& probeResult,
        quint8 sensorIndex,
        quint16 dpi);
    [[nodiscard]] static HidppWriteResult setReportRate(
        const HidppProbeResult& probeResult,
        quint8 intervalMs);
    [[nodiscard]] static HidppWriteResult setOnboardProfileReportRate(
        const HidppProbeResult& probeResult,
        quint8 intervalMs);
    [[nodiscard]] static QString featureName(quint16 featureId);
    [[nodiscard]] static QString formatReport(
        const DeviceInfo& device,
        const HidppProbeResult& result,
        const HidppLiveStateResult* liveState = nullptr);
};

} // namespace openhub
