#include "HidppProbe.hpp"

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace openhub {
namespace {

constexpr quint8 kReportShort = 0x10;
constexpr quint8 kReportLong = 0x11;
constexpr int kShortLength = 7;
constexpr int kLongLength = 20;
constexpr quint8 kSoftwareId = 0x0A;
constexpr int kResponseTimeoutMs = 250;
constexpr int kMaxEnumeratedFeatures = 96;

struct EndpointCaps {
    QString path;
    bool shortReport{false};
    bool longReport{false};
    QString reportIds;
};

struct RequestResult {
    bool ok{false};
    QByteArray response;
    int hidppError{-1};
    QString error;
};

struct ResolvedFeature {
    bool ok{false};
    quint8 index{0};
    quint8 type{0};
    quint8 version{0};
};

QString hexByte(quint8 value)
{
    return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString hexWord(quint16 value)
{
    return QStringLiteral("%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}

QString hexBytes(const QByteArray& bytes)
{
    QStringList parts;
    parts.reserve(bytes.size());
    for (const char c : bytes) {
        parts.push_back(hexByte(static_cast<quint8>(c)));
    }
    return parts.join(QLatin1Char(' '));
}

quint16 be16(const QByteArray& bytes, int offset)
{
    if (offset < 0 || offset + 1 >= bytes.size()) {
        return 0;
    }
    return static_cast<quint16>(
        (static_cast<quint8>(bytes.at(offset)) << 8)
        | static_cast<quint8>(bytes.at(offset + 1)));
}

QSet<quint8> parseReportIds(const QByteArray& descriptor)
{
    QSet<quint8> reportIds;

    qsizetype offset = 0;
    while (offset < descriptor.size()) {
        const quint8 prefix = static_cast<quint8>(descriptor.at(offset++));

        if (prefix == 0xFE) {
            if (offset + 2 > descriptor.size()) {
                break;
            }

            const quint8 payloadSize = static_cast<quint8>(descriptor.at(offset));
            offset += 2;
            offset += payloadSize;
            continue;
        }

        const int sizeCode = prefix & 0x03;
        const int payloadSize = sizeCode == 3 ? 4 : sizeCode;
        const int type = (prefix >> 2) & 0x03;
        const int tag = (prefix >> 4) & 0x0F;

        if (offset + payloadSize > descriptor.size()) {
            break;
        }

        if (type == 1 && tag == 8 && payloadSize >= 1) {
            reportIds.insert(static_cast<quint8>(descriptor.at(offset)));
        }

        offset += payloadSize;
    }

    return reportIds;
}

EndpointCaps endpointCaps(const QString& path)
{
    EndpointCaps caps;
    caps.path = path;

    const QString hidrawName = QFileInfo(path).fileName();
    const QString descriptorPath = QStringLiteral("/sys/class/hidraw/%1/device/report_descriptor")
        .arg(hidrawName);

    QFile descriptorFile(descriptorPath);
    if (!descriptorFile.open(QIODevice::ReadOnly)) {
        caps.reportIds = QStringLiteral("descriptor unavailable");
        return caps;
    }

    const QSet<quint8> ids = parseReportIds(descriptorFile.readAll());

    QList<quint8> sortedIds(ids.cbegin(), ids.cend());
    std::sort(sortedIds.begin(), sortedIds.end());

    QStringList idStrings;
    for (const quint8 id : sortedIds) {
        idStrings.push_back(QStringLiteral("0x%1").arg(hexByte(id)));
    }

    caps.shortReport = ids.contains(kReportShort);
    caps.longReport = ids.contains(kReportLong);
    caps.reportIds = idStrings.isEmpty()
        ? QStringLiteral("no report IDs")
        : idStrings.join(QStringLiteral(", "));
    return caps;
}

void drainFd(int fd, QStringList& trace)
{
    char buffer[64];
    int drained = 0;

    for (;;) {
        const ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count > 0) {
            ++drained;
            continue;
        }

        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }

    if (drained > 0) {
        trace.push_back(QStringLiteral("drained %1 pending HID report(s)").arg(drained));
    }
}

RequestResult sendRequest(int fd,
                          const EndpointCaps& caps,
                          quint8 deviceIndex,
                          quint8 featureIndex,
                          quint8 functionId,
                          const QByteArray& params,
                          QStringList& trace)
{
    RequestResult result;

    quint8 reportId = 0;
    int reportLength = 0;
    if (caps.shortReport) {
        reportId = kReportShort;
        reportLength = kShortLength;
    } else if (caps.longReport) {
        reportId = kReportLong;
        reportLength = kLongLength;
    } else {
        result.error = QStringLiteral("endpoint descriptor does not expose HID++ report 0x10 or 0x11");
        return result;
    }

    QByteArray request(reportLength, '\0');
    request[0] = static_cast<char>(reportId);
    request[1] = static_cast<char>(deviceIndex);
    request[2] = static_cast<char>(featureIndex);
    const quint8 functionAndSoftwareId = static_cast<quint8>((functionId << 4) | kSoftwareId);
    request[3] = static_cast<char>(functionAndSoftwareId);

    const int parameterCapacity = reportLength - 4;
    for (int i = 0; i < params.size() && i < parameterCapacity; ++i) {
        request[4 + i] = params.at(i);
    }

    drainFd(fd, trace);
    trace.push_back(QStringLiteral("TX %1").arg(hexBytes(request)));

    const ssize_t written = ::write(fd, request.constData(), static_cast<size_t>(request.size()));
    if (written != request.size()) {
        result.error = written < 0
            ? QStringLiteral("write failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)))
            : QStringLiteral("short write: %1/%2 bytes").arg(written).arg(request.size());
        trace.push_back(QStringLiteral("! %1").arg(result.error));
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < kResponseTimeoutMs) {
        const int remaining = std::max(1, kResponseTimeoutMs - static_cast<int>(timer.elapsed()));

        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;

        const int pollResult = ::poll(&pfd, 1, remaining);
        if (pollResult == 0) {
            break;
        }
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            result.error = QStringLiteral("poll failed: %1")
                .arg(QString::fromLocal8Bit(std::strerror(errno)));
            trace.push_back(QStringLiteral("! %1").arg(result.error));
            return result;
        }

        if (!(pfd.revents & POLLIN)) {
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                result.error = QStringLiteral("hidraw endpoint became unavailable");
                trace.push_back(QStringLiteral("! %1").arg(result.error));
                return result;
            }
            continue;
        }

        char buffer[64]{};
        const ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            result.error = QStringLiteral("read failed: %1")
                .arg(QString::fromLocal8Bit(std::strerror(errno)));
            trace.push_back(QStringLiteral("! %1").arg(result.error));
            return result;
        }

        if (count < kShortLength) {
            continue;
        }

        const QByteArray response(buffer, static_cast<int>(count));
        trace.push_back(QStringLiteral("RX %1").arg(hexBytes(response)));

        const quint8 responseReport = static_cast<quint8>(response.at(0));
        const quint8 responseDevice = static_cast<quint8>(response.at(1));
        const quint8 responseFeature = static_cast<quint8>(response.at(2));
        const quint8 responseAddress = static_cast<quint8>(response.at(3));

        if (responseReport != kReportShort && responseReport != kReportLong) {
            continue;
        }
        if (responseDevice != deviceIndex) {
            continue;
        }

        if (responseFeature == featureIndex && responseAddress == functionAndSoftwareId) {
            result.ok = true;
            result.response = response;
            return result;
        }

        if ((responseFeature == 0xFF || responseFeature == 0x8F)
            && responseAddress == featureIndex
            && count >= 6
            && static_cast<quint8>(response.at(4)) == functionAndSoftwareId) {
            result.hidppError = static_cast<quint8>(response.at(5));
            result.error = QStringLiteral("HID++ error 0x%1").arg(hexByte(result.hidppError));
            trace.push_back(QStringLiteral("! %1").arg(result.error));
            return result;
        }
    }

    result.error = QStringLiteral("timeout waiting for matching HID++ response");
    trace.push_back(QStringLiteral("! %1").arg(result.error));
    return result;
}

RequestResult rootProtocolVersion(int fd,
                                  const EndpointCaps& caps,
                                  quint8 deviceIndex,
                                  QStringList& trace)
{
    return sendRequest(fd, caps, deviceIndex, 0x00, 0x01, {}, trace);
}

ResolvedFeature rootGetFeature(int fd,
                               const EndpointCaps& caps,
                               quint8 deviceIndex,
                               quint16 featureId,
                               QStringList& trace)
{
    QByteArray params;
    params.push_back(static_cast<char>((featureId >> 8) & 0xFF));
    params.push_back(static_cast<char>(featureId & 0xFF));
    params.push_back('\0');

    const RequestResult response = sendRequest(
        fd, caps, deviceIndex, 0x00, 0x00, params, trace);

    ResolvedFeature resolved;
    if (!response.ok || response.response.size() < 7) {
        return resolved;
    }

    resolved.index = static_cast<quint8>(response.response.at(4));
    resolved.type = static_cast<quint8>(response.response.at(5));
    resolved.version = static_cast<quint8>(response.response.at(6));
    resolved.ok = resolved.index != 0;
    return resolved;
}

QString typeText(quint8 type)
{
    QStringList flags;
    if (type & 0x20) {
        flags.push_back(QStringLiteral("internal"));
    }
    if (type & 0x40) {
        flags.push_back(QStringLiteral("hidden"));
    }
    if (type & 0x80) {
        flags.push_back(QStringLiteral("obsolete"));
    }

    const quint8 remaining = type & 0x1F;
    if (remaining != 0) {
        flags.push_back(QStringLiteral("flags=0x%1").arg(hexByte(remaining)));
    }

    return flags.isEmpty()
        ? QStringLiteral("normal")
        : flags.join(QStringLiteral(", "));
}

const HidppFeatureInfo* findFeature(const HidppProbeResult& result, quint16 featureId)
{
    for (const HidppFeatureInfo& feature : result.features) {
        if (feature.id == featureId) {
            return &feature;
        }
    }
    return nullptr;
}

bool hasFeature(const QVector<HidppFeatureInfo>& features, std::initializer_list<quint16> ids)
{
    for (const HidppFeatureInfo& feature : features) {
        for (const quint16 id : ids) {
            if (feature.id == id) {
                return true;
            }
        }
    }
    return false;
}

QStringList capabilitySummary(const QVector<HidppFeatureInfo>& features)
{
    QStringList lines;

    if (hasFeature(features, {0x1000, 0x1001, 0x1004})) {
        lines.push_back(QStringLiteral("Battery telemetry"));
    }
    if (hasFeature(features, {0x2201, 0x2202})) {
        lines.push_back(QStringLiteral("Adjustable DPI"));
    }
    if (hasFeature(features, {0x8060, 0x8061})) {
        lines.push_back(QStringLiteral("Adjustable report rate"));
    }
    if (hasFeature(features, {0x1B00, 0x1B01, 0x1B02, 0x1B03, 0x1B04, 0x1B05, 0x1B10, 0x1C00})) {
        lines.push_back(QStringLiteral("Reprogrammable controls"));
    }
    if (hasFeature(features, {0x8070, 0x8071, 0x8080, 0x8081, 0x8040, 0x1981, 0x1982, 0x1983, 0x1990})) {
        lines.push_back(QStringLiteral("Lighting / brightness"));
    }
    if (hasFeature(features, {0x8100, 0x8101})) {
        lines.push_back(QStringLiteral("On-board/profile management"));
    }
    if (hasFeature(features, {0x8300, 0x8310, 0x8320, 0x0200, 0x0201, 0x0604, 0x0609})) {
        lines.push_back(QStringLiteral("Headset audio controls"));
    }

    return lines;
}

QVector<quint8> indexCandidates(const DeviceInfo& device)
{
    const QString name = (device.name + QLatin1Char(' ')
        + device.reportedNames.join(QLatin1Char(' '))).toLower();

    if (name.contains(QStringLiteral("g915"))) {
        return {0x01, 0x00};
    }
    return {0x00, 0x01};
}

QString batteryStatusName(quint8 status)
{
    switch (status) {
    case 0x00: return QStringLiteral("discharging");
    case 0x01: return QStringLiteral("recharging");
    case 0x02: return QStringLiteral("almost full");
    case 0x03: return QStringLiteral("full");
    case 0x04: return QStringLiteral("slow recharge");
    case 0x05: return QStringLiteral("invalid battery");
    case 0x06: return QStringLiteral("thermal error");
    default:
        return QStringLiteral("unknown (0x%1)").arg(hexByte(status));
    }
}

int estimateBatteryPercent(quint16 millivolts)
{
    struct Point { int mv; int percent; };
    static constexpr Point curve[] = {
        {4186, 100}, {4067, 90}, {3989, 80}, {3922, 70}, {3859, 60},
        {3811, 50}, {3778, 40}, {3751, 30}, {3717, 20}, {3671, 10},
        {3646, 5}, {3579, 2}, {3500, 0}
    };

    if (millivolts >= curve[0].mv) {
        return curve[0].percent;
    }
    if (millivolts <= curve[std::size(curve) - 1].mv) {
        return curve[std::size(curve) - 1].percent;
    }

    for (size_t i = 0; i + 1 < std::size(curve); ++i) {
        const Point high = curve[i];
        const Point low = curve[i + 1];
        if (millivolts <= high.mv && millivolts >= low.mv) {
            const double fraction = static_cast<double>(millivolts - low.mv)
                / static_cast<double>(high.mv - low.mv);
            return static_cast<int>(
                low.percent + fraction * static_cast<double>(high.percent - low.percent) + 0.5);
        }
    }

    return 0;
}

QString batteryVoltageStatus(quint8 flags)
{
    if (!(flags & 0x80)) {
        return (flags & 0x20)
            ? QStringLiteral("discharging · critical flag")
            : QStringLiteral("discharging");
    }

    const quint8 chargeStatus = flags & 0x03;
    if (chargeStatus == 0x01) {
        return QStringLiteral("full");
    }
    if (flags & 0x10) {
        return QStringLiteral("slow recharge");
    }
    if (chargeStatus == 0x02) {
        return QStringLiteral("connected · not charging");
    }
    if (chargeStatus == 0x07) {
        return QStringLiteral("charging error");
    }
    if (flags & 0x08) {
        return QStringLiteral("fast charging");
    }
    return QStringLiteral("recharging");
}

QString dpiSupportedText(const QVector<quint16>& values, quint16 step)
{
    if (values.isEmpty()) {
        return QStringLiteral("supported range not reported");
    }

    quint16 minimum = values.constFirst();
    quint16 maximum = values.constFirst();
    for (const quint16 value : values) {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }

    if (step > 0 && values.size() >= 2) {
        return QStringLiteral("%1–%2 DPI, step %3")
            .arg(minimum)
            .arg(maximum)
            .arg(step);
    }

    QStringList list;
    for (const quint16 value : values) {
        list.push_back(QString::number(value));
    }
    return QStringLiteral("values: %1 DPI").arg(list.join(QStringLiteral(", ")));
}

QString rateText(int intervalMs)
{
    if (intervalMs <= 0) {
        return QStringLiteral("unknown");
    }

    const double hz = 1000.0 / static_cast<double>(intervalMs);
    const int rounded = static_cast<int>(hz + 0.5);
    return QStringLiteral("%1 Hz (%2 ms)").arg(rounded).arg(intervalMs);
}

bool parseDpiList(const QByteArray& response,
                  QVector<quint16>& supported,
                  quint16& step,
                  quint16& minimum,
                  quint16& maximum)
{
    supported.clear();
    step = 0;
    minimum = 0;
    maximum = 0;

    if (response.size() < 7) {
        return false;
    }

    for (int offset = 5; offset + 1 < response.size(); offset += 2) {
        const quint16 value = be16(response, offset);
        if (value == 0) {
            break;
        }

        if (value > 0xE000) {
            step = static_cast<quint16>(value - 0xE000);
        } else {
            supported.push_back(value);
        }
    }

    if (supported.isEmpty()) {
        return false;
    }

    minimum = supported.constFirst();
    maximum = supported.constFirst();
    for (const quint16 value : supported) {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }

    return true;
}

bool dpiAllowed(quint16 requested,
                const QVector<quint16>& supported,
                quint16 step,
                quint16 minimum,
                quint16 maximum)
{
    if (supported.isEmpty()) {
        return false;
    }

    if (step > 0 && minimum <= maximum) {
        return requested >= minimum
            && requested <= maximum
            && ((requested - minimum) % step) == 0;
    }

    return supported.contains(requested);
}

QVector<quint8> reportIntervalsFromMask(quint8 mask)
{
    QVector<quint8> intervals;
    for (int bit = 0; bit < 8; ++bit) {
        if (mask & (1u << bit)) {
            intervals.push_back(static_cast<quint8>(bit + 1));
        }
    }
    return intervals;
}

int openVerifiedEndpoint(const HidppProbeResult& probe,
                         EndpointCaps& caps,
                         HidppWriteResult& result)
{
    if (!probe.success || probe.endpoint.isEmpty()) {
        result.error = QStringLiteral("A successful HID++ probe is required before configuration.");
        return -1;
    }

    caps = endpointCaps(probe.endpoint);
    if (!caps.shortReport && !caps.longReport) {
        result.error = QStringLiteral("The selected endpoint no longer advertises HID++ report IDs.");
        return -1;
    }

    const QByteArray nativePath = QFile::encodeName(probe.endpoint);
    const int fd = ::open(nativePath.constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        result.error = QStringLiteral("Could not open %1: %2")
            .arg(probe.endpoint, QString::fromLocal8Bit(std::strerror(errno)));
        return -1;
    }

    const RequestResult version = rootProtocolVersion(
        fd, caps, probe.deviceIndex, result.trace);
    if (!version.ok || version.response.size() < 6) {
        result.error = QStringLiteral("The HID++ endpoint did not pass the pre-write protocol check.");
        ::close(fd);
        return -1;
    }

    const int major = static_cast<quint8>(version.response.at(4));
    const int minor = static_cast<quint8>(version.response.at(5));
    if (major != probe.protocolMajor || minor != probe.protocolMinor) {
        result.error = QStringLiteral(
            "The HID++ protocol identity changed since probing (%1.%2 -> %3.%4). Refusing to write.")
            .arg(probe.protocolMajor)
            .arg(probe.protocolMinor)
            .arg(major)
            .arg(minor);
        ::close(fd);
        return -1;
    }

    return fd;
}

bool readDpiState(int fd,
                  const EndpointCaps& caps,
                  const HidppProbeResult& probe,
                  HidppLiveStateResult& state)
{
    const HidppFeatureInfo* feature = findFeature(probe, 0x2201);
    if (!feature) {
        return false;
    }

    const RequestResult count = sendRequest(
        fd, caps, probe.deviceIndex, feature->index, 0x00, {}, state.trace);
    if (!count.ok || count.response.size() < 5) {
        state.warnings.push_back(QStringLiteral("Adjustable DPI: could not read sensor count."));
        return true;
    }

    const int sensorCount = static_cast<quint8>(count.response.at(4));
    if (sensorCount <= 0) {
        state.warnings.push_back(QStringLiteral("Adjustable DPI: device reported zero sensors."));
        return true;
    }

    for (int sensor = 0; sensor < sensorCount; ++sensor) {
        QByteArray parameter(1, static_cast<char>(sensor));

        const RequestResult list = sendRequest(
            fd, caps, probe.deviceIndex, feature->index, 0x01, parameter, state.trace);
        const RequestResult current = sendRequest(
            fd, caps, probe.deviceIndex, feature->index, 0x02, parameter, state.trace);

        if (!current.ok || current.response.size() < 9) {
            state.warnings.push_back(
                QStringLiteral("Adjustable DPI sensor %1: current DPI response was incomplete.")
                    .arg(sensor));
            continue;
        }

        QVector<quint16> supported;
        quint16 step = 0;
        quint16 minimum = 0;
        quint16 maximum = 0;

        if (!list.ok || !parseDpiList(list.response, supported, step, minimum, maximum)) {
            state.warnings.push_back(
                QStringLiteral("Adjustable DPI sensor %1: supported DPI list could not be parsed.")
                    .arg(sensor));
        }

        const quint8 returnedSensor = static_cast<quint8>(current.response.at(4));
        const quint16 dpi = be16(current.response, 5);
        const quint16 defaultDpi = be16(current.response, 7);

        HidppDpiState dpiState;
        dpiState.available = true;
        dpiState.sensorIndex = returnedSensor;
        dpiState.currentDpi = dpi;
        dpiState.defaultDpi = defaultDpi;
        dpiState.minimumDpi = minimum;
        dpiState.maximumDpi = maximum;
        dpiState.stepDpi = step;
        dpiState.supportedValues = supported;
        state.dpiSensors.push_back(dpiState);

        QString details = QStringLiteral("Feature 0x2201 v%1 · sensor %2 · supported %3")
            .arg(feature->version)
            .arg(returnedSensor)
            .arg(dpiSupportedText(supported, step));
        if (defaultDpi > 0) {
            details += QStringLiteral(" · default %1 DPI").arg(defaultDpi);
        }

        state.values.push_back({
            sensorCount == 1
                ? QStringLiteral("DPI")
                : QStringLiteral("DPI sensor %1").arg(returnedSensor),
            QStringLiteral("%1 DPI").arg(dpi),
            details,
            0x2201
        });
    }

    return true;
}

bool readReportRateState(int fd,
                         const EndpointCaps& caps,
                         const HidppProbeResult& probe,
                         HidppLiveStateResult& state)
{
    const HidppFeatureInfo* feature = findFeature(probe, 0x8060);
    if (!feature) {
        return false;
    }

    const RequestResult list = sendRequest(
        fd, caps, probe.deviceIndex, feature->index, 0x00, {}, state.trace);

    QByteArray parameter(1, '\0');
    const RequestResult current = sendRequest(
        fd, caps, probe.deviceIndex, feature->index, 0x01, parameter, state.trace);

    if (!current.ok || current.response.size() < 5) {
        state.warnings.push_back(QStringLiteral("Adjustable Report Rate: current value could not be read."));
        return true;
    }

    const quint8 intervalMs = static_cast<quint8>(current.response.at(4));

    QStringList supported;
    QString maskText = QStringLiteral("unknown");
    QVector<quint8> intervals;

    if (list.ok && list.response.size() >= 5) {
        const quint8 mask = static_cast<quint8>(list.response.at(4));
        maskText = QStringLiteral("0x%1").arg(hexByte(mask));
        intervals = reportIntervalsFromMask(mask);
        for (const quint8 interval : intervals) {
            supported.push_back(rateText(interval));
        }
    } else {
        state.warnings.push_back(QStringLiteral("Adjustable Report Rate: supported-rate list could not be read."));
    }

    state.reportRate.available = true;
    state.reportRate.currentIntervalMs = intervalMs;
    state.reportRate.supportedIntervalsMs = intervals;

    state.values.push_back({
        QStringLiteral("Report rate"),
        rateText(intervalMs),
        QStringLiteral("Feature 0x8060 v%1 · supported: %2 · raw mask %3")
            .arg(feature->version)
            .arg(supported.isEmpty() ? QStringLiteral("not reported")
                                     : supported.join(QStringLiteral(", ")))
            .arg(maskText),
        0x8060
    });

    return true;
}

bool readBatteryState(int fd,
                      const EndpointCaps& caps,
                      const HidppProbeResult& probe,
                      HidppLiveStateResult& state)
{
    if (const HidppFeatureInfo* feature = findFeature(probe, 0x1004)) {
        const RequestResult response = sendRequest(
            fd, caps, probe.deviceIndex, feature->index, 0x01, {}, state.trace);

        if (!response.ok || response.response.size() < 8) {
            state.warnings.push_back(QStringLiteral("Unified Battery: state could not be read."));
            return true;
        }

        const quint8 reportedPercent = static_cast<quint8>(response.response.at(4));
        const quint8 levelCode = static_cast<quint8>(response.response.at(5));
        const quint8 status = static_cast<quint8>(response.response.at(6));

        int level = reportedPercent;
        bool approximate = false;
        if (level == 0) {
            approximate = true;
            switch (levelCode) {
            case 8: level = 90; break;
            case 4: level = 50; break;
            case 2: level = 20; break;
            case 1: level = 5; break;
            default: level = 0; break;
            }
        }

        state.values.push_back({
            QStringLiteral("Battery"),
            QStringLiteral("%1%2%").arg(approximate ? QStringLiteral("~") : QString()).arg(level),
            QStringLiteral("Unified Battery 0x1004 v%1 · %2 · level code 0x%3")
                .arg(feature->version)
                .arg(batteryStatusName(status))
                .arg(hexByte(levelCode)),
            0x1004
        });
        return true;
    }

    if (const HidppFeatureInfo* feature = findFeature(probe, 0x1001)) {
        const RequestResult response = sendRequest(
            fd, caps, probe.deviceIndex, feature->index, 0x00, {}, state.trace);

        if (!response.ok || response.response.size() < 7) {
            state.warnings.push_back(QStringLiteral("Battery Voltage: state could not be read."));
            return true;
        }

        const quint16 voltage = be16(response.response, 4);
        const quint8 flags = static_cast<quint8>(response.response.at(6));
        const int estimatedPercent = estimateBatteryPercent(voltage);

        state.values.push_back({
            QStringLiteral("Battery"),
            QStringLiteral("~%1%").arg(estimatedPercent),
            QStringLiteral("Battery Voltage 0x1001 v%1 · %2 mV · %3 · raw flags 0x%4 · percentage estimated from voltage")
                .arg(feature->version)
                .arg(voltage)
                .arg(batteryVoltageStatus(flags))
                .arg(hexByte(flags)),
            0x1001
        });
        return true;
    }

    if (const HidppFeatureInfo* feature = findFeature(probe, 0x1000)) {
        const RequestResult response = sendRequest(
            fd, caps, probe.deviceIndex, feature->index, 0x00, {}, state.trace);

        if (!response.ok || response.response.size() < 7) {
            state.warnings.push_back(QStringLiteral("Battery Status: state could not be read."));
            return true;
        }

        const quint8 percent = static_cast<quint8>(response.response.at(4));
        const quint8 nextLevel = static_cast<quint8>(response.response.at(5));
        const quint8 status = static_cast<quint8>(response.response.at(6));

        state.values.push_back({
            QStringLiteral("Battery"),
            percent == 0 ? QStringLiteral("unknown") : QStringLiteral("%1%").arg(percent),
            QStringLiteral("Battery Status 0x1000 v%1 · %2 · next level %3%")
                .arg(feature->version)
                .arg(batteryStatusName(status))
                .arg(nextLevel),
            0x1000
        });
        return true;
    }

    return false;
}

} // namespace

bool HidppProbe::isEligible(const DeviceInfo& device)
{
    if (device.vendorId != 0x046d || !device.isLogitechFamily) {
        return false;
    }
    if (!device.readable || !device.writable) {
        return false;
    }

    const QString role = device.role.toLower();
    const QString name = (device.name + QLatin1Char(' ')
        + device.reportedNames.join(QLatin1Char(' '))).toLower();

    if (role.contains(QStringLiteral("receiver"))
        || role.contains(QStringLiteral("base station"))
        || name.contains(QStringLiteral("a50"))) {
        return false;
    }

    return !device.hidrawNodes.isEmpty();
}

HidppProbeResult HidppProbe::probe(const DeviceInfo& device)
{
    HidppProbeResult result;

    if (!isEligible(device)) {
        result.error = QStringLiteral(
            "This v0.2.2 HID++ path is limited to directly attached Logitech HID++ device interfaces. "
            "Receiver-child and A50 X protocol probing remain disabled.");
        return result;
    }

    QVector<EndpointCaps> candidates;
    for (const QString& node : device.hidrawNodes) {
        EndpointCaps caps = endpointCaps(node);
        result.trace.push_back(
            QStringLiteral("%1 report IDs: %2").arg(node, caps.reportIds));

        if (caps.shortReport || caps.longReport) {
            result.candidateEndpoints.push_back(node);
            candidates.push_back(std::move(caps));
        }
    }

    if (candidates.isEmpty()) {
        result.error = QStringLiteral(
            "No hidraw endpoint advertises HID++ report ID 0x10 or 0x11 in its HID report descriptor.");
        return result;
    }

    int selectedFd = -1;
    EndpointCaps selectedCaps;

    for (const EndpointCaps& caps : candidates) {
        const QByteArray nativePath = QFile::encodeName(caps.path);
        const int fd = ::open(nativePath.constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            result.trace.push_back(
                QStringLiteral("! %1 open failed: %2")
                    .arg(caps.path, QString::fromLocal8Bit(std::strerror(errno))));
            continue;
        }

        bool matched = false;
        for (const quint8 deviceIndex : indexCandidates(device)) {
            result.trace.push_back(
                QStringLiteral("probing %1 with HID++ device index 0x%2")
                    .arg(caps.path, hexByte(deviceIndex)));

            const RequestResult version = rootProtocolVersion(fd, caps, deviceIndex, result.trace);
            if (!version.ok || version.response.size() < 6) {
                continue;
            }

            const int major = static_cast<quint8>(version.response.at(4));
            const int minor = static_cast<quint8>(version.response.at(5));

            if (major < 2) {
                result.trace.push_back(
                    QStringLiteral("! endpoint replied with protocol %1.%2, not HID++ feature protocol")
                        .arg(major)
                        .arg(minor));
                continue;
            }

            result.endpoint = caps.path;
            result.deviceIndex = deviceIndex;
            result.protocolMajor = major;
            result.protocolMinor = minor;
            selectedFd = fd;
            selectedCaps = caps;
            matched = true;
            break;
        }

        if (matched) {
            break;
        }

        ::close(fd);
    }

    if (selectedFd < 0) {
        result.error = QStringLiteral(
            "HID++ report IDs were found, but no candidate endpoint answered a non-mutating Root.GetProtocolVersion request.");
        return result;
    }

    const ResolvedFeature featureSet = rootGetFeature(
        selectedFd, selectedCaps, result.deviceIndex, 0x0001, result.trace);

    if (!featureSet.ok) {
        result.error = QStringLiteral(
            "HID++ protocol responded, but Feature Set (0x0001) could not be resolved.");
        ::close(selectedFd);
        return result;
    }

    const RequestResult countResponse = sendRequest(
        selectedFd, selectedCaps, result.deviceIndex, featureSet.index, 0x00, {}, result.trace);

    if (!countResponse.ok || countResponse.response.size() < 5) {
        result.error = QStringLiteral("Feature Set was found, but its feature count could not be read.");
        ::close(selectedFd);
        return result;
    }

    const int advertisedCount = static_cast<quint8>(countResponse.response.at(4)) + 1;
    const int featureCount = std::min(advertisedCount, kMaxEnumeratedFeatures);

    for (int i = 0; i < featureCount; ++i) {
        QByteArray params;
        params.push_back(static_cast<char>(i));

        const RequestResult featureResponse = sendRequest(
            selectedFd,
            selectedCaps,
            result.deviceIndex,
            featureSet.index,
            0x01,
            params,
            result.trace);

        if (!featureResponse.ok || featureResponse.response.size() < 7) {
            result.trace.push_back(
                QStringLiteral("! feature-set index %1 could not be read").arg(i));
            continue;
        }

        HidppFeatureInfo feature;
        feature.id = static_cast<quint16>(
            (static_cast<quint8>(featureResponse.response.at(4)) << 8)
            | static_cast<quint8>(featureResponse.response.at(5)));
        feature.index = static_cast<quint8>(i);
        feature.type = static_cast<quint8>(featureResponse.response.at(6));
        feature.version = featureResponse.response.size() >= 8
            ? static_cast<quint8>(featureResponse.response.at(7))
            : -1;
        feature.name = featureName(feature.id);

        if (feature.id != 0x0000) {
            const ResolvedFeature resolved = rootGetFeature(
                selectedFd, selectedCaps, result.deviceIndex, feature.id, result.trace);
            if (resolved.ok) {
                feature.index = resolved.index;
                feature.type = resolved.type;
                feature.version = resolved.version;
            }
        }

        result.features.push_back(std::move(feature));
    }

    if (advertisedCount > kMaxEnumeratedFeatures) {
        result.trace.push_back(
            QStringLiteral("! feature list truncated from %1 to %2 entries")
                .arg(advertisedCount)
                .arg(kMaxEnumeratedFeatures));
    }

    ::close(selectedFd);

    std::sort(result.features.begin(), result.features.end(), [](const HidppFeatureInfo& a,
                                                                 const HidppFeatureInfo& b) {
        return a.index < b.index;
    });

    result.success = true;
    return result;
}

HidppLiveStateResult HidppProbe::readLiveState(
    const DeviceInfo&,
    const HidppProbeResult& probeResult)
{
    HidppLiveStateResult state;

    if (!probeResult.success || probeResult.endpoint.isEmpty()) {
        state.error = QStringLiteral("A successful HID++ capability probe is required first.");
        return state;
    }

    const EndpointCaps caps = endpointCaps(probeResult.endpoint);
    if (!caps.shortReport && !caps.longReport) {
        state.error = QStringLiteral("The previously selected endpoint no longer advertises HID++ reports.");
        return state;
    }

    const QByteArray nativePath = QFile::encodeName(probeResult.endpoint);
    const int fd = ::open(nativePath.constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        state.error = QStringLiteral("Could not reopen %1: %2")
            .arg(probeResult.endpoint, QString::fromLocal8Bit(std::strerror(errno)));
        return state;
    }

    state.trace.push_back(
        QStringLiteral("live-state session on %1, device index 0x%2")
            .arg(probeResult.endpoint, hexByte(probeResult.deviceIndex)));

    const bool dpiAvailable = readDpiState(fd, caps, probeResult, state);
    const bool rateAvailable = readReportRateState(fd, caps, probeResult, state);
    const bool batteryAvailable = readBatteryState(fd, caps, probeResult, state);

    ::close(fd);

    if (!dpiAvailable && !rateAvailable && !batteryAvailable) {
        state.error = QStringLiteral(
            "The device was probed successfully, but v0.2.1 does not yet implement a live-state reader for any feature it exposed.");
        return state;
    }

    if (state.values.isEmpty()) {
        state.error = QStringLiteral(
            "Known live-state features were present, but none returned a complete value.");
        return state;
    }

    state.success = true;
    return state;
}

HidppWriteResult HidppProbe::setDpi(
    const HidppProbeResult& probeResult,
    quint8 sensorIndex,
    quint16 dpi)
{
    HidppWriteResult result;
    result.trace.push_back(
        QStringLiteral("SET DPI requested: sensor %1 -> %2 DPI")
            .arg(sensorIndex)
            .arg(dpi));

    const HidppFeatureInfo* feature = findFeature(probeResult, 0x2201);
    if (!feature) {
        result.error = QStringLiteral("Adjustable DPI (0x2201) was not discovered on this device.");
        return result;
    }

    EndpointCaps caps;
    const int fd = openVerifiedEndpoint(probeResult, caps, result);
    if (fd < 0) {
        return result;
    }

    const RequestResult count = sendRequest(
        fd, caps, probeResult.deviceIndex, feature->index, 0x00, {}, result.trace);
    if (!count.ok || count.response.size() < 5
        || sensorIndex >= static_cast<quint8>(count.response.at(4))) {
        result.error = QStringLiteral("The requested DPI sensor is no longer reported by the device.");
        ::close(fd);
        return result;
    }

    QByteArray sensorParameter(1, static_cast<char>(sensorIndex));
    const RequestResult list = sendRequest(
        fd, caps, probeResult.deviceIndex, feature->index, 0x01, sensorParameter, result.trace);

    QVector<quint16> supported;
    quint16 step = 0;
    quint16 minimum = 0;
    quint16 maximum = 0;
    if (!list.ok || !parseDpiList(list.response, supported, step, minimum, maximum)) {
        result.error = QStringLiteral(
            "The device did not provide a usable DPI capability list. Refusing to write.");
        ::close(fd);
        return result;
    }

    if (!dpiAllowed(dpi, supported, step, minimum, maximum)) {
        result.error = step > 0
            ? QStringLiteral("DPI %1 is outside the device-reported range/step (%2–%3, step %4).")
                  .arg(dpi).arg(minimum).arg(maximum).arg(step)
            : QStringLiteral("DPI %1 is not in the device-reported supported list.").arg(dpi);
        ::close(fd);
        return result;
    }

    QByteArray params;
    params.push_back(static_cast<char>(sensorIndex));
    params.push_back(static_cast<char>((dpi >> 8) & 0xFF));
    params.push_back(static_cast<char>(dpi & 0xFF));

    const RequestResult setResponse = sendRequest(
        fd, caps, probeResult.deviceIndex, feature->index, 0x03, params, result.trace);
    if (!setResponse.ok) {
        result.error = QStringLiteral("SET_SENSOR_DPI failed: %1").arg(setResponse.error);
        ::close(fd);
        return result;
    }

    if (feature->version > 0 && setResponse.response.size() >= 7) {
        const quint16 echoedDpi = be16(setResponse.response, 5);
        if (echoedDpi != 0 && echoedDpi != dpi) {
            result.error = QStringLiteral(
                "The device echoed %1 DPI instead of requested %2 DPI.")
                .arg(echoedDpi)
                .arg(dpi);
            ::close(fd);
            return result;
        }
    }

    const RequestResult verify = sendRequest(
        fd, caps, probeResult.deviceIndex, feature->index, 0x02, sensorParameter, result.trace);
    ::close(fd);

    if (!verify.ok || verify.response.size() < 9) {
        result.error = QStringLiteral(
            "The DPI SET request was accepted, but the verification GET failed.");
        return result;
    }

    const quint16 verifiedDpi = be16(verify.response, 5);
    if (verifiedDpi != dpi) {
        result.error = QStringLiteral(
            "DPI verification mismatch: requested %1 DPI, device reports %2 DPI.")
            .arg(dpi)
            .arg(verifiedDpi);
        return result;
    }

    result.success = true;
    result.summary = QStringLiteral("DPI verified at %1 DPI.").arg(verifiedDpi);
    return result;
}

HidppWriteResult HidppProbe::setReportRate(
    const HidppProbeResult& probeResult,
    quint8 intervalMs)
{
    HidppWriteResult result;
    result.trace.push_back(
        QStringLiteral("SET report rate requested: %1").arg(rateText(intervalMs)));

    const HidppFeatureInfo* feature = findFeature(probeResult, 0x8060);
    if (!feature) {
        result.error = QStringLiteral("Adjustable Report Rate (0x8060) was not discovered on this device.");
        return result;
    }

    EndpointCaps caps;
    const int fd = openVerifiedEndpoint(probeResult, caps, result);
    if (fd < 0) {
        return result;
    }

    const RequestResult list = sendRequest(
        fd, caps, probeResult.deviceIndex, feature->index, 0x00, {}, result.trace);
    if (!list.ok || list.response.size() < 5) {
        result.error = QStringLiteral(
            "The device did not provide its supported report-rate mask. Refusing to write.");
        ::close(fd);
        return result;
    }

    const quint8 mask = static_cast<quint8>(list.response.at(4));
    const QVector<quint8> supported = reportIntervalsFromMask(mask);
    if (!supported.contains(intervalMs)) {
        result.error = QStringLiteral(
            "%1 is not present in the device-reported report-rate mask 0x%2.")
            .arg(rateText(intervalMs), hexByte(mask));
        ::close(fd);
        return result;
    }

    QByteArray params(1, static_cast<char>(intervalMs));
    const RequestResult setResponse = sendRequest(
        fd, caps, probeResult.deviceIndex, feature->index, 0x02, params, result.trace);
    if (!setResponse.ok) {
        result.error = QStringLiteral("SET_REPORT_RATE failed: %1").arg(setResponse.error);
        ::close(fd);
        return result;
    }

    QByteArray verifyParameter(1, '\0');
    const RequestResult verify = sendRequest(
        fd, caps, probeResult.deviceIndex, feature->index, 0x01, verifyParameter, result.trace);
    ::close(fd);

    if (!verify.ok || verify.response.size() < 5) {
        result.error = QStringLiteral(
            "The report-rate SET request was accepted, but the verification GET failed.");
        return result;
    }

    const quint8 verifiedInterval = static_cast<quint8>(verify.response.at(4));
    if (verifiedInterval != intervalMs) {
        result.error = QStringLiteral(
            "Report-rate verification mismatch: requested %1, device reports %2.")
            .arg(rateText(intervalMs), rateText(verifiedInterval));
        return result;
    }

    result.success = true;
    result.summary = QStringLiteral("Report rate verified at %1.").arg(rateText(verifiedInterval));
    return result;
}

QString HidppProbe::featureName(quint16 featureId)
{
    static const QMap<quint16, QString> names = {
        {0x0000, QStringLiteral("Root")},
        {0x0001, QStringLiteral("Feature Set")},
        {0x0002, QStringLiteral("Feature Info")},
        {0x0003, QStringLiteral("Device Firmware Version")},
        {0x0004, QStringLiteral("Device Unit ID")},
        {0x0005, QStringLiteral("Device Name")},
        {0x0006, QStringLiteral("Device Groups")},
        {0x0007, QStringLiteral("Device Friendly Name")},
        {0x0008, QStringLiteral("Keep Alive")},
        {0x0011, QStringLiteral("Property Access")},
        {0x0020, QStringLiteral("Configuration Change")},
        {0x0030, QStringLiteral("Target Software")},
        {0x0080, QStringLiteral("Wireless Signal Strength")},
        {0x00C2, QStringLiteral("Signed DFU Control")},
        {0x00C3, QStringLiteral("DFU Control")},
        {0x00D0, QStringLiteral("DFU")},
        {0x1000, QStringLiteral("Battery Status")},
        {0x1001, QStringLiteral("Battery Voltage")},
        {0x1004, QStringLiteral("Unified Battery")},
        {0x1010, QStringLiteral("Charging Control")},
        {0x1300, QStringLiteral("LED Control")},
        {0x1802, QStringLiteral("Device Reset")},
        {0x1805, QStringLiteral("OOB State")},
        {0x1806, QStringLiteral("Device Properties")},
        {0x1814, QStringLiteral("Change Host")},
        {0x1815, QStringLiteral("Hosts Info")},
        {0x1981, QStringLiteral("Backlight")},
        {0x1982, QStringLiteral("Backlight 2")},
        {0x1983, QStringLiteral("Backlight 3")},
        {0x1990, QStringLiteral("Illumination")},
        {0x1B00, QStringLiteral("Reprogrammable Controls")},
        {0x1B01, QStringLiteral("Reprogrammable Controls v2")},
        {0x1B02, QStringLiteral("Reprogrammable Controls v2.2")},
        {0x1B03, QStringLiteral("Reprogrammable Controls v3")},
        {0x1B04, QStringLiteral("Reprogrammable Controls v4")},
        {0x1B05, QStringLiteral("Full Key Customization")},
        {0x1B10, QStringLiteral("Control List")},
        {0x1C00, QStringLiteral("Persistent Remappable Action")},
        {0x1D4B, QStringLiteral("Wireless Device Status")},
        {0x1E00, QStringLiteral("Enable Hidden Features")},
        {0x2001, QStringLiteral("Left/Right Swap")},
        {0x2100, QStringLiteral("Vertical Scrolling")},
        {0x2110, QStringLiteral("Smart Shift")},
        {0x2111, QStringLiteral("Smart Shift Enhanced")},
        {0x2120, QStringLiteral("Hi-Res Scrolling")},
        {0x2121, QStringLiteral("Hi-Res Wheel")},
        {0x2130, QStringLiteral("Low-Res Wheel")},
        {0x2150, QStringLiteral("Thumb Wheel")},
        {0x2200, QStringLiteral("Mouse Pointer")},
        {0x2201, QStringLiteral("Adjustable DPI")},
        {0x2202, QStringLiteral("Extended Adjustable DPI")},
        {0x2230, QStringLiteral("Angle Snapping")},
        {0x2240, QStringLiteral("Surface Tuning")},
        {0x40A0, QStringLiteral("Fn Inversion")},
        {0x40A2, QStringLiteral("New Fn Inversion")},
        {0x4220, QStringLiteral("Lock Key State")},
        {0x4520, QStringLiteral("Keyboard Layout")},
        {0x4521, QStringLiteral("Keyboard Disable Keys")},
        {0x4522, QStringLiteral("Keyboard Disable by Usage")},
        {0x4523, QStringLiteral("Keyboard Disable Controls")},
        {0x4530, QStringLiteral("Dual Platform")},
        {0x4531, QStringLiteral("Multi Platform")},
        {0x4540, QStringLiteral("Keyboard Layout 2")},
        {0x8010, QStringLiteral("G-Keys")},
        {0x8020, QStringLiteral("M-Keys")},
        {0x8030, QStringLiteral("MR Key")},
        {0x8040, QStringLiteral("Brightness Control")},
        {0x8051, QStringLiteral("Logitech Modifiers")},
        {0x8060, QStringLiteral("Adjustable Report Rate")},
        {0x8061, QStringLiteral("Extended Adjustable Report Rate")},
        {0x8070, QStringLiteral("Color LED Effects")},
        {0x8071, QStringLiteral("RGB Effects")},
        {0x8080, QStringLiteral("Per-Key Lighting")},
        {0x8081, QStringLiteral("Per-Key Lighting v2")},
        {0x8090, QStringLiteral("Mode Status")},
        {0x8100, QStringLiteral("On-board Profiles")},
        {0x8101, QStringLiteral("Profile Management")},
        {0x8110, QStringLiteral("Mouse Button Spy")},
        {0x8111, QStringLiteral("Latency Monitoring")},
        {0x8120, QStringLiteral("Gaming Attachments")},
        {0x8300, QStringLiteral("Sidetone")},
        {0x8310, QStringLiteral("Equalizer")},
        {0x8320, QStringLiteral("Headset Output")}
    };

    const auto it = names.constFind(featureId);
    if (it != names.cend()) {
        return it.value();
    }
    return QStringLiteral("Unknown feature");
}

QString HidppProbe::formatReport(
    const DeviceInfo& device,
    const HidppProbeResult& result,
    const HidppLiveStateResult* liveState)
{
    QString report;
    report += QStringLiteral("OpenHub v0.2.2 HID++ Control Report\n");
    report += QStringLiteral("Device: %1\n").arg(device.name);
    report += QStringLiteral("VID:PID: %1\n").arg(device.idString());
    report += QStringLiteral("Current connection: %1\n").arg(device.currentConnection);
    report += QStringLiteral("Candidate endpoints: %1\n").arg(
        result.candidateEndpoints.isEmpty()
            ? QStringLiteral("none")
            : result.candidateEndpoints.join(QStringLiteral(", ")));

    if (!result.success) {
        report += QStringLiteral("Probe result: FAILED\n");
        report += QStringLiteral("Reason: %1\n").arg(result.error);
    } else {
        report += QStringLiteral("Probe result: SUCCESS\n");
        report += QStringLiteral("HID++ endpoint: %1\n").arg(result.endpoint);
        report += QStringLiteral("HID++ device index: 0x%1\n").arg(hexByte(result.deviceIndex));
        report += QStringLiteral("Protocol version: %1.%2\n")
            .arg(result.protocolMajor)
            .arg(result.protocolMinor);
        report += QStringLiteral("Feature count: %1\n").arg(result.features.size());

        const QStringList capabilities = capabilitySummary(result.features);
        report += QStringLiteral("Capability groups: %1\n")
            .arg(capabilities.isEmpty()
                ? QStringLiteral("none recognized")
                : capabilities.join(QStringLiteral(", ")));

        if (liveState) {
            report += QStringLiteral("\nLive state:\n");
            if (liveState->success) {
                for (const HidppLiveValue& value : liveState->values) {
                    report += QStringLiteral("- %1: %2 — %3\n")
                        .arg(value.name, value.current, value.details);
                }
            } else {
                report += QStringLiteral("- unavailable: %1\n").arg(liveState->error);
            }

            for (const QString& warning : liveState->warnings) {
                report += QStringLiteral("- warning: %1\n").arg(warning);
            }

            if (!liveState->configurationActions.isEmpty()) {
                report += QStringLiteral("\nConfiguration actions:\n");
                for (const QString& action : liveState->configurationActions) {
                    report += QStringLiteral("- %1\n").arg(action);
                }
            }
        }

        report += QStringLiteral("\nFeatures:\n");
        for (const HidppFeatureInfo& feature : result.features) {
            const QString version = feature.version >= 0
                ? QString::number(feature.version)
                : QStringLiteral("?");
            report += QStringLiteral(
                "- 0x%1 | index 0x%2 | v%3 | type 0x%4 (%5) | %6\n")
                .arg(hexWord(feature.id),
                     hexByte(feature.index),
                     version,
                     hexByte(feature.type),
                     typeText(feature.type),
                     feature.name);
        }
    }

    report += QStringLiteral("\nProbe trace:\n");
    for (const QString& line : result.trace) {
        report += line + QLatin1Char('\n');
    }

    if (liveState) {
        report += QStringLiteral("\nLive-state trace:\n");
        for (const QString& line : liveState->trace) {
            report += line + QLatin1Char('\n');
        }
    }

    if (liveState && liveState->configurationWriteAttempted) {
        report += QStringLiteral(
            "\nSafety note: v0.2.2 configuration was explicitly requested by the user. "
            "Only validated DPI/report-rate SET commands plus verification GETs are implemented; "
            "no profile-memory, lighting, button-remap, or firmware writes are used.\n");
    } else {
        report += QStringLiteral(
            "\nSafety note: no configuration write was attempted in this session.\n");
    }
    return report;
}

} // namespace openhub
