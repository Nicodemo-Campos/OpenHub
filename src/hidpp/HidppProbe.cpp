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

quint16 le16(const QByteArray& bytes, int offset)
{
    if (offset < 0 || offset + 1 >= bytes.size()) {
        return 0;
    }
    return static_cast<quint16>(
        static_cast<quint8>(bytes.at(offset))
        | (static_cast<quint8>(bytes.at(offset + 1)) << 8));
}

void putLe16(QByteArray& bytes, int offset, quint16 value)
{
    if (offset < 0 || offset + 1 >= bytes.size()) {
        return;
    }
    bytes[offset] = static_cast<char>(value & 0xFF);
    bytes[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
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

QString hidppErrorName(int code)
{
    switch (code) {
    case 0x01: return QStringLiteral("UNKNOWN");
    case 0x02: return QStringLiteral("INVALID_ARGUMENT");
    case 0x03: return QStringLiteral("OUT_OF_RANGE");
    case 0x04: return QStringLiteral("HARDWARE_ERROR");
    case 0x05: return QStringLiteral("LOGITECH_INTERNAL");
    case 0x06: return QStringLiteral("INVALID_FEATURE_INDEX");
    case 0x07: return QStringLiteral("INVALID_FUNCTION_ID");
    case 0x08: return QStringLiteral("BUSY");
    case 0x09: return QStringLiteral("UNSUPPORTED");
    default: return QStringLiteral("UNKNOWN_CODE");
    }
}

RequestResult sendRequest(int fd,
                          const EndpointCaps& caps,
                          quint8 deviceIndex,
                          quint8 featureIndex,
                          quint8 functionId,
                          const QByteArray& params,
                          QStringList& trace,
                          bool forceLong = false)
{
    RequestResult result;

    quint8 reportId = 0;
    int reportLength = 0;
    if (forceLong && caps.longReport) {
        reportId = kReportLong;
        reportLength = kLongLength;
    } else if (forceLong) {
        result.error = QStringLiteral("endpoint does not expose the long HID++ report required by this operation");
        return result;
    } else if (caps.shortReport) {
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
            result.error = QStringLiteral("HID++ %1 (0x%2)")
                .arg(hidppErrorName(result.hidppError), hexByte(result.hidppError));
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

quint16 crcCcitt(const QByteArray& data, int length)
{
    quint16 crc = 0xFFFF;
    const int count = std::clamp(length, 0, static_cast<int>(data.size()));

    for (int i = 0; i < count; ++i) {
        quint16 temp = static_cast<quint16>((crc >> 8)
            ^ static_cast<quint8>(data.at(i)));
        crc = static_cast<quint16>(crc << 8);
        quint16 quick = static_cast<quint16>(temp ^ (temp >> 4));
        crc = static_cast<quint16>(crc ^ quick);
        quick = static_cast<quint16>(quick << 5);
        crc = static_cast<quint16>(crc ^ quick);
        quick = static_cast<quint16>(quick << 7);
        crc = static_cast<quint16>(crc ^ quick);
    }

    return crc;
}

bool sectorCrcValid(const QByteArray& sector)
{
    if (sector.size() < 2) {
        return false;
    }
    return crcCcitt(sector, sector.size() - 2) == be16(sector, sector.size() - 2);
}

bool readOnboardSector(int fd,
                       const EndpointCaps& caps,
                       quint8 deviceIndex,
                       quint8 featureIndex,
                       quint16 sector,
                       quint16 sectorSize,
                       QByteArray& data,
                       QStringList& trace,
                       QString& error)
{
    if (!caps.longReport) {
        error = QStringLiteral("On-board profile memory requires HID++ long reports.");
        return false;
    }
    if (sectorSize < 16 || sectorSize > 1024) {
        error = QStringLiteral("Unsupported on-board sector size %1 bytes.").arg(sectorSize);
        return false;
    }

    data = QByteArray(sectorSize, '\0');

    for (int offset = 0; offset < sectorSize; offset += 16) {
        const int requestOffset = (sectorSize - offset < 16)
            ? sectorSize - 16
            : offset;

        QByteArray params;
        params.push_back(static_cast<char>((sector >> 8) & 0xFF));
        params.push_back(static_cast<char>(sector & 0xFF));
        params.push_back(static_cast<char>((requestOffset >> 8) & 0xFF));
        params.push_back(static_cast<char>(requestOffset & 0xFF));

        const RequestResult response = sendRequest(
            fd, caps, deviceIndex, featureIndex, 0x05, params, trace, true);
        if (!response.ok || response.response.size() < kLongLength) {
            error = QStringLiteral("Failed to read on-board sector 0x%1 at offset %2: %3")
                .arg(sector, 4, 16, QLatin1Char('0'))
                .arg(requestOffset)
                .arg(response.error);
            return false;
        }

        const int copyLength = std::min(16, static_cast<int>(sectorSize) - requestOffset);
        std::memcpy(data.data() + requestOffset, response.response.constData() + 4,
                    static_cast<size_t>(copyLength));

        if (requestOffset != offset) {
            break;
        }
    }

    return true;
}

bool writeOnboardSectorRaw(int fd,
                           const EndpointCaps& caps,
                           quint8 deviceIndex,
                           quint8 featureIndex,
                           quint16 sector,
                           const QByteArray& data,
                           QStringList& trace,
                           QString& error)
{
    if (!caps.longReport) {
        error = QStringLiteral("On-board profile memory requires HID++ long reports.");
        return false;
    }
    if (data.size() < 16 || data.size() > 1024) {
        error = QStringLiteral("Refusing profile write with unsupported sector size %1.")
            .arg(data.size());
        return false;
    }

    QByteArray start;
    start.push_back(static_cast<char>((sector >> 8) & 0xFF));
    start.push_back(static_cast<char>(sector & 0xFF));
    start.push_back('\0');
    start.push_back('\0');
    start.push_back(static_cast<char>((data.size() >> 8) & 0xFF));
    start.push_back(static_cast<char>(data.size() & 0xFF));

    const RequestResult begin = sendRequest(
        fd, caps, deviceIndex, featureIndex, 0x06, start, trace, true);
    if (!begin.ok) {
        error = QStringLiteral("Profile memory write-start failed: %1").arg(begin.error);
        return false;
    }

    for (int offset = 0; offset < data.size(); offset += 16) {
        // HID++ profile sectors are not required to be a multiple of 16 bytes
        // (the G502 LIGHTSPEED reports 255 bytes). The write address command
        // declares the exact byte count; the last long report may therefore
        // contain fewer than 16 meaningful data bytes and is zero-padded by
        // sendRequest().
        const QByteArray block = data.mid(offset, 16);
        const RequestResult write = sendRequest(
            fd, caps, deviceIndex, featureIndex, 0x07, block, trace, true);
        if (!write.ok) {
            (void)sendRequest(fd, caps, deviceIndex, featureIndex, 0x08, {}, trace);
            error = QStringLiteral("Profile memory write failed at offset %1: %2")
                .arg(offset)
                .arg(write.error);
            return false;
        }
    }

    const RequestResult end = sendRequest(
        fd, caps, deviceIndex, featureIndex, 0x08, {}, trace);
    if (!end.ok) {
        error = QStringLiteral("Profile memory write-end failed: %1").arg(end.error);
        return false;
    }

    return true;
}

struct OnboardDescriptor {
    bool ok{false};
    quint8 memoryModel{0};
    quint8 profileFormat{0};
    quint8 macroFormat{0};
    quint8 profileCount{0};
    quint8 profileCountOob{0};
    quint8 buttonCount{0};
    quint8 sectorCount{0};
    quint16 sectorSize{0};
    quint8 mechanicalLayout{0};
    quint8 variousInfo{0};
};

OnboardDescriptor readOnboardDescriptor(int fd,
                                        const EndpointCaps& caps,
                                        quint8 deviceIndex,
                                        quint8 featureIndex,
                                        QStringList& trace)
{
    OnboardDescriptor info;
    const RequestResult response = sendRequest(
        fd, caps, deviceIndex, featureIndex, 0x00, {}, trace);

    if (!response.ok || response.response.size() < kLongLength) {
        return info;
    }

    info.memoryModel = static_cast<quint8>(response.response.at(4));
    info.profileFormat = static_cast<quint8>(response.response.at(5));
    info.macroFormat = static_cast<quint8>(response.response.at(6));
    info.profileCount = static_cast<quint8>(response.response.at(7));
    info.profileCountOob = static_cast<quint8>(response.response.at(8));
    info.buttonCount = static_cast<quint8>(response.response.at(9));
    info.sectorCount = static_cast<quint8>(response.response.at(10));
    info.sectorSize = be16(response.response, 11);
    info.mechanicalLayout = static_cast<quint8>(response.response.at(13));
    info.variousInfo = static_cast<quint8>(response.response.at(14));
    info.ok = true;
    return info;
}

QVector<int> activeProfileIndexCandidates(quint16 choice, quint8 profileCount)
{
    QVector<int> candidates;
    const int low = choice & 0xFF;

    // HID++ devices are inconsistent here: some return a zero-based index,
    // while others return a one-based index. Try both and resolve them against
    // the CRC-valid directory/profile plus the current live report rate.
    if (low >= 0 && low < profileCount) {
        candidates.push_back(low);
    }
    if (low > 0 && (low - 1) < profileCount && !candidates.contains(low - 1)) {
        candidates.push_back(low - 1);
    }

    return candidates;
}

struct ActiveProfileResolution {
    bool ok{false};
    bool directoryCrcValid{false};
    bool profileCrcValid{false};
    bool enabled{false};
    int index{-1};
    quint16 sector{0xFFFF};
    QByteArray sectorData;
    QString error;
};

ActiveProfileResolution resolveActiveProfile(
    int fd,
    const EndpointCaps& caps,
    quint8 deviceIndex,
    quint8 featureIndex,
    const OnboardDescriptor& info,
    quint16 activeChoice,
    quint8 liveReportIntervalMs,
    QStringList& trace)
{
    ActiveProfileResolution resolved;

    QByteArray directory;
    QString memoryError;
    if (!readOnboardSector(
            fd, caps, deviceIndex, featureIndex,
            0x0000, info.sectorSize, directory, trace, memoryError)) {
        resolved.error = memoryError;
        return resolved;
    }

    resolved.directoryCrcValid = sectorCrcValid(directory);
    if (!resolved.directoryCrcValid) {
        resolved.error = QStringLiteral("User profile directory CRC is invalid.");
        return resolved;
    }

    struct Candidate {
        int index{-1};
        quint16 sector{0xFFFF};
        bool enabled{false};
        QByteArray data;
        bool crcValid{false};
        bool liveRateMatches{false};
    };

    QVector<Candidate> validCandidates;
    const QVector<int> candidates =
        activeProfileIndexCandidates(activeChoice, info.profileCount);

    for (const int index : candidates) {
        const int entryOffset = index * 4;
        if (entryOffset < 0 || entryOffset + 3 >= directory.size()) {
            continue;
        }

        Candidate candidate;
        candidate.index = index;
        candidate.sector = be16(directory, entryOffset);
        candidate.enabled =
            static_cast<quint8>(directory.at(entryOffset + 2)) != 0;

        if (!candidate.enabled
            || candidate.sector == 0xFFFF
            || candidate.sector >= info.sectorCount) {
            continue;
        }

        QString candidateError;
        if (!readOnboardSector(
                fd, caps, deviceIndex, featureIndex,
                candidate.sector, info.sectorSize,
                candidate.data, trace, candidateError)) {
            trace.push_back(
                QStringLiteral("profile candidate %1 (sector 0x%2) read failed: %3")
                    .arg(index)
                    .arg(candidate.sector, 4, 16, QLatin1Char('0'))
                    .arg(candidateError));
            continue;
        }

        candidate.crcValid = sectorCrcValid(candidate.data);
        if (!candidate.crcValid || candidate.data.isEmpty()) {
            trace.push_back(
                QStringLiteral("profile candidate %1 (sector 0x%2) rejected: invalid CRC")
                    .arg(index)
                    .arg(candidate.sector, 4, 16, QLatin1Char('0')));
            continue;
        }

        candidate.liveRateMatches =
            liveReportIntervalMs > 0
            && static_cast<quint8>(candidate.data.at(0)) == liveReportIntervalMs;

        trace.push_back(
            QStringLiteral(
                "profile candidate %1 -> sector 0x%2, enabled, CRC valid, stored rate %3%4")
                .arg(index)
                .arg(candidate.sector, 4, 16, QLatin1Char('0'))
                .arg(rateText(static_cast<quint8>(candidate.data.at(0))))
                .arg(candidate.liveRateMatches
                    ? QStringLiteral(" (matches live rate)")
                    : QString()));

        validCandidates.push_back(std::move(candidate));
    }

    if (validCandidates.isEmpty()) {
        resolved.error = QStringLiteral(
            "No CRC-valid enabled profile candidate matched the active profile choice 0x%1.")
            .arg(activeChoice, 4, 16, QLatin1Char('0'));
        return resolved;
    }

    QVector<int> matchingIndexes;
    for (int i = 0; i < validCandidates.size(); ++i) {
        if (validCandidates.at(i).liveRateMatches) {
            matchingIndexes.push_back(i);
        }
    }

    int selected = -1;
    if (matchingIndexes.size() == 1) {
        selected = matchingIndexes.constFirst();
    } else if (matchingIndexes.isEmpty() && validCandidates.size() == 1) {
        selected = 0;
    } else {
        resolved.error = matchingIndexes.size() > 1
            ? QStringLiteral(
                "Active profile indexing is ambiguous: multiple CRC-valid candidates match the live report rate.")
            : QStringLiteral(
                "Active profile indexing is ambiguous: multiple CRC-valid candidates exist and none matches the live report rate.");
        return resolved;
    }

    const Candidate& candidate = validCandidates.at(selected);
    resolved.ok = true;
    resolved.profileCrcValid = candidate.crcValid;
    resolved.enabled = candidate.enabled;
    resolved.index = candidate.index;
    resolved.sector = candidate.sector;
    resolved.sectorData = candidate.data;
    return resolved;
}

bool knownWritableProfileLayout(const OnboardDescriptor& info)
{
    return info.ok
        && info.memoryModel == 0x01
        && info.profileFormat >= 0x01
        && info.profileFormat <= 0x05
        && info.macroFormat == 0x01
        && info.profileCount > 0
        && info.sectorCount > 1
        && info.sectorSize >= 32
        && info.sectorSize <= 1024;
}

bool readOnboardProfileState(int fd,
                           const EndpointCaps& caps,
                           const HidppProbeResult& probe,
                           HidppLiveStateResult& state)
{
    const HidppFeatureInfo* feature = findFeature(probe, 0x8100);
    if (!feature) {
        return false;
    }

    HidppOnboardProfileState& profileState = state.onboardProfile;
    profileState.present = true;

    const RequestResult mode = sendRequest(
        fd, caps, probe.deviceIndex, feature->index, 0x02, {}, state.trace);
    if (!mode.ok || mode.response.size() < 5) {
        state.warnings.push_back(
            QStringLiteral("On-board Profiles: current mode could not be read."));
        return true;
    }

    profileState.mode = static_cast<quint8>(mode.response.at(4));

    QString modeText;
    if (profileState.mode == 0x01) {
        modeText = QStringLiteral("enabled");

        const OnboardDescriptor info = readOnboardDescriptor(
            fd, caps, probe.deviceIndex, feature->index, state.trace);
        if (!info.ok) {
            state.warnings.push_back(
                QStringLiteral("On-board Profiles: profile-memory descriptor could not be read."));
        } else {
            profileState.metadataReady = true;
            profileState.memoryModel = info.memoryModel;
            profileState.profileFormat = info.profileFormat;
            profileState.macroFormat = info.macroFormat;
            profileState.profileCount = info.profileCount;
            profileState.sectorCount = info.sectorCount;
            profileState.sectorSize = info.sectorSize;
            profileState.writableLayout = knownWritableProfileLayout(info);

            if (!profileState.writableLayout) {
                state.warnings.push_back(
                    QStringLiteral(
                        "On-board Profiles: unsupported memory layout "
                        "(memory 0x%1, profile 0x%2, macro 0x%3, sector %4 B).")
                        .arg(hexByte(info.memoryModel))
                        .arg(hexByte(info.profileFormat))
                        .arg(hexByte(info.macroFormat))
                        .arg(info.sectorSize));
            }

            const RequestResult currentProfile = sendRequest(
                fd, caps, probe.deviceIndex, feature->index, 0x04, {}, state.trace);
            if (!currentProfile.ok || currentProfile.response.size() < 6) {
                state.warnings.push_back(
                    QStringLiteral("On-board Profiles: active profile choice could not be read."));
            } else {
                profileState.activeChoice = be16(currentProfile.response, 4);

                modeText += QStringLiteral(" · active choice 0x%1")
                    .arg(profileState.activeChoice, 4, 16, QLatin1Char('0'))
                    .toUpper();

                if (profileState.writableLayout) {
                    const ActiveProfileResolution resolved = resolveActiveProfile(
                        fd,
                        caps,
                        probe.deviceIndex,
                        feature->index,
                        info,
                        profileState.activeChoice,
                        state.reportRate.available
                            ? state.reportRate.currentIntervalMs
                            : 0,
                        state.trace);

                    profileState.directoryCrcValid = resolved.directoryCrcValid;
                    profileState.profileCrcValid = resolved.profileCrcValid;
                    profileState.activeEnabled = resolved.enabled;
                    profileState.activeIndex = resolved.index;
                    profileState.activeSector = resolved.sector;

                    if (!resolved.ok) {
                        state.warnings.push_back(
                            QStringLiteral("On-board Profiles: %1").arg(resolved.error));
                    } else {
                        profileState.activeReportIntervalMs =
                            static_cast<quint8>(resolved.sectorData.at(0));

                        if (resolved.sectorData.size() >= 13) {
                            profileState.defaultDpiIndex =
                                static_cast<quint8>(resolved.sectorData.at(1));
                            profileState.shiftedDpiIndex =
                                static_cast<quint8>(resolved.sectorData.at(2));
                            profileState.dpiSlots.clear();
                            for (int i = 0; i < 5; ++i) {
                                profileState.dpiSlots.push_back(
                                    le16(resolved.sectorData, 3 + (2 * i)));
                            }

                            const RequestResult currentDpiIndex = sendRequest(
                                fd, caps, probe.deviceIndex, feature->index,
                                0x0B, {}, state.trace);
                            if (currentDpiIndex.ok
                                && currentDpiIndex.response.size() >= 5) {
                                profileState.currentDpiIndex =
                                    static_cast<quint8>(currentDpiIndex.response.at(4));
                            } else {
                                state.warnings.push_back(
                                    QStringLiteral(
                                        "On-board Profiles: current DPI stage index could not be read."));
                            }
                        }

                        QStringList dpiStageText;
                        for (int i = 0; i < profileState.dpiSlots.size(); ++i) {
                            const quint16 dpi = profileState.dpiSlots.at(i);
                            dpiStageText.push_back(
                                dpi == 0
                                    ? QStringLiteral("%1:off").arg(i + 1)
                                    : QStringLiteral("%1:%2").arg(i + 1).arg(dpi));
                        }

                        modeText += QStringLiteral(
                            " · resolved index %1 · profile rate %2")
                            .arg(profileState.activeIndex)
                            .arg(rateText(profileState.activeReportIntervalMs));

                        if (!dpiStageText.isEmpty()) {
                            modeText += QStringLiteral(" · DPI [%1]")
                                .arg(dpiStageText.join(QStringLiteral(", ")));
                        }
                    }
                }
            }
        }
    } else if (profileState.mode == 0x02) {
        modeText = QStringLiteral("disabled / host mode");
    } else {
        modeText = QStringLiteral("unknown mode 0x%1").arg(hexByte(profileState.mode));
    }

    QString details = QStringLiteral("Feature 0x8100 v%1").arg(feature->version);
    if (profileState.metadataReady) {
        details += QStringLiteral(
            " · memory 0x%1 · profile format 0x%2 · %3 profile(s) · sector %4 B")
            .arg(hexByte(profileState.memoryModel))
            .arg(hexByte(profileState.profileFormat))
            .arg(profileState.profileCount)
            .arg(profileState.sectorSize);

        if (profileState.activeSector != 0xFFFF) {
            details += QStringLiteral(
                " · active index %1 · active sector 0x%2 · dir CRC %3 · profile CRC %4")
                .arg(profileState.activeIndex)
                .arg(profileState.activeSector, 4, 16, QLatin1Char('0'))
                .arg(profileState.directoryCrcValid
                    ? QStringLiteral("valid")
                    : QStringLiteral("invalid"))
                .arg(profileState.profileCrcValid
                    ? QStringLiteral("valid")
                    : QStringLiteral("invalid"));

            if (!profileState.dpiSlots.isEmpty()) {
                details += QStringLiteral(
                    " · default DPI stage %1 · current DPI stage %2 · shift stage %3")
                    .arg(profileState.defaultDpiIndex < 5
                        ? QString::number(profileState.defaultDpiIndex + 1)
                        : QStringLiteral("?"))
                    .arg(profileState.currentDpiIndex < 5
                        ? QString::number(profileState.currentDpiIndex + 1)
                        : QStringLiteral("?"))
                    .arg(profileState.shiftedDpiIndex < 5
                        ? QString::number(profileState.shiftedDpiIndex + 1)
                        : QStringLiteral("none/unknown"));
            }
        } else {
            details += QStringLiteral(" · active profile not safely resolved");
        }
    }

    state.values.push_back({
        QStringLiteral("On-board profiles"),
        modeText,
        details,
        0x8100
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
            "This v0.2.3 HID++ path is limited to directly attached Logitech HID++ device interfaces. "
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
    const bool profileAvailable = readOnboardProfileState(fd, caps, probeResult, state);
    const bool batteryAvailable = readBatteryState(fd, caps, probeResult, state);

    ::close(fd);

    if (!dpiAvailable && !rateAvailable && !profileAvailable && !batteryAvailable) {
        state.error = QStringLiteral(
            "The device was probed successfully, but no implemented live-state reader matched its exposed features.");
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

    const ResolvedFeature freshFeature = rootGetFeature(
        fd, caps, probeResult.deviceIndex, 0x2201, result.trace);
    if (!freshFeature.ok) {
        result.error = QStringLiteral("Adjustable DPI disappeared before the write. Refusing to continue.");
        ::close(fd);
        return result;
    }

    const quint8 featureIndex = freshFeature.index;

    const RequestResult count = sendRequest(
        fd, caps, probeResult.deviceIndex, featureIndex, 0x00, {}, result.trace);
    if (!count.ok || count.response.size() < 5
        || sensorIndex >= static_cast<quint8>(count.response.at(4))) {
        result.error = QStringLiteral("The requested DPI sensor is no longer reported by the device.");
        ::close(fd);
        return result;
    }

    QByteArray sensorParameter(1, static_cast<char>(sensorIndex));
    const RequestResult list = sendRequest(
        fd, caps, probeResult.deviceIndex, featureIndex, 0x01, sensorParameter, result.trace);

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
        fd, caps, probeResult.deviceIndex, featureIndex, 0x03, params, result.trace);
    if (!setResponse.ok) {
        result.error = QStringLiteral("SET_SENSOR_DPI failed: %1").arg(setResponse.error);
        ::close(fd);
        return result;
    }

    if (freshFeature.version > 0 && setResponse.response.size() >= 7) {
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
        fd, caps, probeResult.deviceIndex, featureIndex, 0x02, sensorParameter, result.trace);
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

    if (findFeature(probeResult, 0x8100)) {
        const ResolvedFeature profileFeature = rootGetFeature(
            fd, caps, probeResult.deviceIndex, 0x8100, result.trace);
        if (profileFeature.ok) {
            const RequestResult mode = sendRequest(
                fd, caps, probeResult.deviceIndex, profileFeature.index, 0x02, {}, result.trace);
            if (mode.ok && mode.response.size() >= 5
                && static_cast<quint8>(mode.response.at(4)) == 0x01) {
                result.error = QStringLiteral(
                    "Direct report-rate SET is blocked while On-board Profiles (0x8100) are enabled. "
                    "Use the v0.2.3 active-profile writer instead.");
                ::close(fd);
                return result;
            }
        }
    }

    const ResolvedFeature freshFeature = rootGetFeature(
        fd, caps, probeResult.deviceIndex, 0x8060, result.trace);
    if (!freshFeature.ok) {
        result.error = QStringLiteral("Adjustable Report Rate disappeared before the write. Refusing to continue.");
        ::close(fd);
        return result;
    }

    const quint8 featureIndex = freshFeature.index;

    const RequestResult list = sendRequest(
        fd, caps, probeResult.deviceIndex, featureIndex, 0x00, {}, result.trace);
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
        fd, caps, probeResult.deviceIndex, featureIndex, 0x02, params, result.trace);
    if (!setResponse.ok) {
        result.error = QStringLiteral("SET_REPORT_RATE failed: %1").arg(setResponse.error);
        ::close(fd);
        return result;
    }

    QByteArray verifyParameter(1, '\0');
    const RequestResult verify = sendRequest(
        fd, caps, probeResult.deviceIndex, featureIndex, 0x01, verifyParameter, result.trace);
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

HidppWriteResult HidppProbe::setOnboardProfileReportRate(
    const HidppProbeResult& probeResult,
    quint8 intervalMs)
{
    HidppWriteResult result;
    result.trace.push_back(
        QStringLiteral("PROFILE SET report rate requested: %1").arg(rateText(intervalMs)));

    if (!findFeature(probeResult, 0x8100)) {
        result.error = QStringLiteral("On-board Profiles (0x8100) was not discovered.");
        return result;
    }
    if (!findFeature(probeResult, 0x8060)) {
        result.error = QStringLiteral(
            "Adjustable Report Rate (0x8060) is required to validate supported rates.");
        return result;
    }

    EndpointCaps caps;
    const int fd = openVerifiedEndpoint(probeResult, caps, result);
    if (fd < 0) {
        return result;
    }

    const ResolvedFeature profileFeature = rootGetFeature(
        fd, caps, probeResult.deviceIndex, 0x8100, result.trace);
    const ResolvedFeature rateFeature = rootGetFeature(
        fd, caps, probeResult.deviceIndex, 0x8060, result.trace);
    if (!profileFeature.ok || !rateFeature.ok) {
        result.error = QStringLiteral(
            "Required HID++ features disappeared before the profile write.");
        ::close(fd);
        return result;
    }

    const RequestResult mode = sendRequest(
        fd, caps, probeResult.deviceIndex, profileFeature.index, 0x02, {}, result.trace);
    if (!mode.ok || mode.response.size() < 5
        || static_cast<quint8>(mode.response.at(4)) != 0x01) {
        result.error = QStringLiteral(
            "On-board profile mode is not active; persistent profile-rate editing is unavailable.");
        ::close(fd);
        return result;
    }

    const RequestResult supportedResponse = sendRequest(
        fd, caps, probeResult.deviceIndex, rateFeature.index, 0x00, {}, result.trace);
    if (!supportedResponse.ok || supportedResponse.response.size() < 5) {
        result.error = QStringLiteral(
            "Could not re-read the device-supported report-rate mask.");
        ::close(fd);
        return result;
    }

    const quint8 mask = static_cast<quint8>(supportedResponse.response.at(4));
    const QVector<quint8> supported = reportIntervalsFromMask(mask);
    if (!supported.contains(intervalMs)) {
        result.error = QStringLiteral(
            "%1 is not present in the device-reported report-rate mask 0x%2.")
            .arg(rateText(intervalMs), hexByte(mask));
        ::close(fd);
        return result;
    }

    const OnboardDescriptor info = readOnboardDescriptor(
        fd, caps, probeResult.deviceIndex, profileFeature.index, result.trace);
    if (!knownWritableProfileLayout(info)) {
        result.error = info.ok
            ? QStringLiteral(
                "Unsupported profile-memory layout: memory 0x%1, profile 0x%2, macro 0x%3, sector %4 B.")
                  .arg(hexByte(info.memoryModel))
                  .arg(hexByte(info.profileFormat))
                  .arg(hexByte(info.macroFormat))
                  .arg(info.sectorSize)
            : QStringLiteral("Could not read the on-board profile-memory descriptor.");
        ::close(fd);
        return result;
    }

    const RequestResult currentProfile = sendRequest(
        fd, caps, probeResult.deviceIndex, profileFeature.index, 0x04, {}, result.trace);
    if (!currentProfile.ok || currentProfile.response.size() < 6) {
        result.error = QStringLiteral("Could not determine the active on-board profile.");
        ::close(fd);
        return result;
    }

    const quint16 activeChoice = be16(currentProfile.response, 4);

    QByteArray getRateParameter(1, '\0');
    const RequestResult currentRateResponse = sendRequest(
        fd, caps, probeResult.deviceIndex, rateFeature.index, 0x01,
        getRateParameter, result.trace);
    const quint8 currentLiveInterval =
        currentRateResponse.ok && currentRateResponse.response.size() >= 5
            ? static_cast<quint8>(currentRateResponse.response.at(4))
            : 0;

    const ActiveProfileResolution active = resolveActiveProfile(
        fd,
        caps,
        probeResult.deviceIndex,
        profileFeature.index,
        info,
        activeChoice,
        currentLiveInterval,
        result.trace);

    if (!active.ok) {
        result.error = QStringLiteral(
            "Active profile could not be resolved safely: %1").arg(active.error);
        ::close(fd);
        return result;
    }

    const int activeIndex = active.index;
    const quint16 activeSector = active.sector;
    QByteArray original = active.sectorData;
    QString memoryError;

    result.trace.push_back(
        QStringLiteral(
            "resolved active profile choice 0x%1 -> index %2 -> sector 0x%3")
            .arg(activeChoice, 4, 16, QLatin1Char('0'))
            .arg(activeIndex)
            .arg(activeSector, 4, 16, QLatin1Char('0')));

    const quint8 previousInterval = static_cast<quint8>(original.at(0));
    if (previousInterval == intervalMs) {
        result.success = true;
        result.summary = QStringLiteral(
            "Active profile already stores %1; no flash write was needed.")
            .arg(rateText(intervalMs));
        ::close(fd);
        return result;
    }

    QByteArray modified = original;
    modified[0] = static_cast<char>(intervalMs);
    const quint16 crc = crcCcitt(modified, modified.size() - 2);
    modified[modified.size() - 2] = static_cast<char>((crc >> 8) & 0xFF);
    modified[modified.size() - 1] = static_cast<char>(crc & 0xFF);

    result.trace.push_back(
        QStringLiteral("writing active profile choice 0x%1, sector 0x%2: %3 -> %4")
            .arg(activeChoice, 4, 16, QLatin1Char('0'))
            .arg(activeSector, 4, 16, QLatin1Char('0'))
            .arg(rateText(previousInterval))
            .arg(rateText(intervalMs)));

    if (!writeOnboardSectorRaw(
            fd, caps, probeResult.deviceIndex, profileFeature.index,
            activeSector, modified, result.trace, memoryError)) {
        result.error = memoryError;
        ::close(fd);
        return result;
    }

    QByteArray verifySector;
    if (!readOnboardSector(
            fd, caps, probeResult.deviceIndex, profileFeature.index,
            activeSector, info.sectorSize, verifySector, result.trace, memoryError)
        || !sectorCrcValid(verifySector)
        || verifySector != modified) {
        result.error = QStringLiteral(
            "Profile flash verification failed after writing sector 0x%1.")
            .arg(activeSector, 4, 16, QLatin1Char('0'));
        ::close(fd);
        return result;
    }

    RequestResult liveRate = sendRequest(
        fd, caps, probeResult.deviceIndex, rateFeature.index, 0x01,
        getRateParameter, result.trace);

    if (!liveRate.ok || liveRate.response.size() < 5
        || static_cast<quint8>(liveRate.response.at(4)) != intervalMs) {
        QByteArray reselect;
        reselect.push_back(currentProfile.response.at(4));
        reselect.push_back(currentProfile.response.at(5));

        const RequestResult reload = sendRequest(
            fd, caps, probeResult.deviceIndex, profileFeature.index, 0x03,
            reselect, result.trace);

        if (reload.ok) {
            liveRate = sendRequest(
                fd, caps, probeResult.deviceIndex, rateFeature.index, 0x01,
                getRateParameter, result.trace);
        }
    }

    if (!liveRate.ok || liveRate.response.size() < 5
        || static_cast<quint8>(liveRate.response.at(4)) != intervalMs) {
        QString rollbackError;
        const bool rollback = writeOnboardSectorRaw(
            fd, caps, probeResult.deviceIndex, profileFeature.index,
            activeSector, original, result.trace, rollbackError);

        QByteArray reselect;
        reselect.push_back(currentProfile.response.at(4));
        reselect.push_back(currentProfile.response.at(5));
        (void)sendRequest(
            fd, caps, probeResult.deviceIndex, profileFeature.index, 0x03,
            reselect, result.trace);

        result.error = rollback
            ? QStringLiteral(
                "Profile sector was written but the active report rate did not reload. "
                "The original sector was restored.")
            : QStringLiteral(
                "Profile sector was written but the active report rate did not reload, "
                "and rollback also failed: %1").arg(rollbackError);
        ::close(fd);
        return result;
    }

    ::close(fd);
    result.success = true;
    result.summary = QStringLiteral(
        "Active on-board profile verified at %1 (sector 0x%2).")
        .arg(rateText(intervalMs))
        .arg(activeSector, 4, 16, QLatin1Char('0'));
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
    report += QStringLiteral("OpenHub v0.2.3 HID++ Control Report\n");
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
            "\nSafety note: v0.2.3 configuration was explicitly requested by the user. "
            "DPI uses a validated active-state SET. Report rate may use either host-mode 0x8060 or "
            "a CRC-validated clone-and-patch write of the active 0x8100 profile sector, followed by read-back verification. "
            "Lighting, button-remap, macro, directory, and firmware writes remain disabled.\n");
    } else {
        report += QStringLiteral(
            "\nSafety note: no configuration write was attempted in this session.\n");
    }
    return report;
}

} // namespace openhub
