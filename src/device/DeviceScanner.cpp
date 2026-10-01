#include "DeviceScanner.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>

#include <algorithm>
#include <unistd.h>

namespace openhub {
namespace {

struct RawHidNode {
    QString node;
    QString sysPath;
    QString name;
    quint32 bus{0};
    quint32 vendorId{0};
    quint32 productId{0};
    bool readable{false};
    bool writable{false};
};

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QMap<QString, QString> readUevent(const QString& path)
{
    QMap<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return values;
    }

    const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const qsizetype separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue;
        }
        values.insert(line.left(separator), line.mid(separator + 1));
    }
    return values;
}

quint32 parseHex(const QString& value)
{
    bool ok = false;
    const quint32 parsed = value.trimmed().toUInt(&ok, 16);
    return ok ? parsed : 0;
}

void parseHidId(const QString& hidId, quint32& bus, quint32& vendor, quint32& product)
{
    const QStringList parts = hidId.split(QLatin1Char(':'));
    if (parts.size() != 3) {
        return;
    }

    bus = parseHex(parts.at(0));
    vendor = parseHex(parts.at(1));
    product = parseHex(parts.at(2));
}

bool containsFamilyName(const QString& text)
{
    return text.contains(QStringLiteral("logitech"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("astro"), Qt::CaseInsensitive);
}

bool isLogitechFamily(quint32 vendorId,
                      const QString& manufacturer,
                      const QString& product,
                      const QStringList& reportedNames)
{
    if (vendorId == 0x046d) {
        return true;
    }
    if (containsFamilyName(manufacturer) || containsFamilyName(product)) {
        return true;
    }

    return std::any_of(reportedNames.cbegin(), reportedNames.cend(), [](const QString& name) {
        return containsFamilyName(name);
    });
}

void appendUnique(QStringList& list, const QString& value)
{
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty() && !list.contains(trimmed)) {
        list.push_back(trimmed);
    }
}

QString baseConnectionName(quint32 bus)
{
    switch (bus) {
    case 0x03:
        return QStringLiteral("USB");
    case 0x05:
        return QStringLiteral("Bluetooth");
    default:
        return bus == 0
            ? QStringLiteral("Unknown")
            : QStringLiteral("HID bus 0x%1").arg(bus, 2, 16, QLatin1Char('0')).toUpper();
    }
}

QString searchableName(const DeviceInfo& device)
{
    return (device.name + QLatin1Char(' ')
        + device.reportedNames.join(QLatin1Char(' '))).toLower();
}

QString familyKey(const DeviceInfo& device)
{
    const QString name = searchableName(device);
    if (name.contains(QStringLiteral("g502"))) {
        return QStringLiteral("g502");
    }
    if (name.contains(QStringLiteral("g915"))) {
        return QStringLiteral("g915");
    }
    if (name.contains(QStringLiteral("a50 x"))
        || name.contains(QStringLiteral("a50x"))
        || (name.contains(QStringLiteral("a50")) && name.contains(QStringLiteral("astro")))) {
        return QStringLiteral("a50x");
    }
    return {};
}

void applyKnownMetadata(DeviceInfo& device)
{
    device.currentConnection = baseConnectionName(device.bus);
    device.role = QStringLiteral("HID device");

    const QString name = searchableName(device);

    if (name.contains(QStringLiteral("receiver"))) {
        device.role = QStringLiteral("Wireless receiver");
        if (device.bus == 0x03) {
            device.currentConnection = QStringLiteral("USB receiver");
        }
    }

    if (device.vendorId != 0x046d) {
        return;
    }

    // Known IDs are used only for identity/connection metadata in v0.1.1.
    // No protocol commands are sent based on these values.
    switch (device.productId) {
    case 0xc539:
        device.role = QStringLiteral("LIGHTSPEED receiver");
        device.currentConnection = QStringLiteral("USB receiver");
        appendUnique(device.wirelessCapabilities, QStringLiteral("LIGHTSPEED"));
        break;
    case 0xc08d:
        device.role = QStringLiteral("Physical device");
        device.currentConnection = QStringLiteral("USB (wired)");
        appendUnique(device.wirelessCapabilities, QStringLiteral("LIGHTSPEED"));
        break;
    case 0xc356:
        device.role = QStringLiteral("Physical device");
        device.currentConnection = QStringLiteral("USB (wired)");
        appendUnique(device.wirelessCapabilities, QStringLiteral("LIGHTSPEED"));
        appendUnique(device.wirelessCapabilities, QStringLiteral("Bluetooth"));
        break;
    case 0x0b0b:
        device.role = QStringLiteral("Base station / headset interface");
        device.currentConnection = QStringLiteral("USB (base station)");
        appendUnique(device.wirelessCapabilities, QStringLiteral("LIGHTSPEED"));
        break;
    default:
        break;
    }

    if (name.contains(QStringLiteral("lightspeed"))) {
        appendUnique(device.wirelessCapabilities, QStringLiteral("LIGHTSPEED"));
    }
}

QVector<RawHidNode> scanRawHidNodes()
{
    QVector<RawHidNode> nodes;
    QDir hidrawDir(QStringLiteral("/sys/class/hidraw"));
    if (!hidrawDir.exists()) {
        return nodes;
    }

    const QStringList entries = hidrawDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries) {
        const QString classPath = hidrawDir.filePath(entry);
        const QString deviceLink = QDir(classPath).filePath(QStringLiteral("device"));
        QString sysPath = QFileInfo(deviceLink).canonicalFilePath();
        if (sysPath.isEmpty()) {
            sysPath = deviceLink;
        }

        const auto uevent = readUevent(QDir(deviceLink).filePath(QStringLiteral("uevent")));

        RawHidNode node;
        node.node = QStringLiteral("/dev/") + entry;
        node.sysPath = sysPath;
        node.name = uevent.value(QStringLiteral("HID_NAME")).trimmed();
        parseHidId(uevent.value(QStringLiteral("HID_ID")), node.bus, node.vendorId, node.productId);

        // access(2) observes the permissions/ACLs that the current process
        // actually has, including udev/logind uaccess ACLs.
        const QByteArray nativePath = QFile::encodeName(node.node);
        node.readable = ::access(nativePath.constData(), R_OK) == 0;
        node.writable = ::access(nativePath.constData(), W_OK) == 0;

        nodes.push_back(node);
    }

    return nodes;
}

QString nearestUsbDevice(QString path)
{
    for (int depth = 0; depth < 20 && !path.isEmpty(); ++depth) {
        const QString vendor = readTextFile(QDir(path).filePath(QStringLiteral("idVendor")));
        const QString product = readTextFile(QDir(path).filePath(QStringLiteral("idProduct")));
        if (!vendor.isEmpty() && !product.isEmpty()) {
            return path;
        }

        const QString parent = QFileInfo(path).dir().absolutePath();
        if (parent == path || parent == QStringLiteral(".")) {
            break;
        }
        path = parent;
    }

    return {};
}

QString relationLabel(const DeviceInfo& device)
{
    return QStringLiteral("%1 — %2 (%3)")
        .arg(device.name, device.idString(), device.role);
}

void linkRelatedDevices(QVector<DeviceInfo>& devices)
{
    for (int i = 0; i < devices.size(); ++i) {
        const QString family = familyKey(devices.at(i));
        if (family.isEmpty()) {
            continue;
        }

        for (int j = i + 1; j < devices.size(); ++j) {
            if (familyKey(devices.at(j)) != family) {
                continue;
            }

            const bool iReceiver = devices.at(i).role.contains(QStringLiteral("receiver"), Qt::CaseInsensitive);
            const bool jReceiver = devices.at(j).role.contains(QStringLiteral("receiver"), Qt::CaseInsensitive);

            // v0.1.1 links receiver/direct views of the same known family, but
            // avoids merging two physical devices merely because names match.
            if (iReceiver == jReceiver) {
                continue;
            }

            appendUnique(devices[i].relatedDevices, relationLabel(devices.at(j)));
            appendUnique(devices[j].relatedDevices, relationLabel(devices.at(i)));
        }
    }
}

} // namespace

QVector<DeviceInfo> DeviceScanner::scan() const
{
    const QVector<RawHidNode> rawNodes = scanRawHidNodes();
    QVector<DeviceInfo> devices;

    QMap<QString, QVector<int>> usbGroups;
    QVector<int> nonUsbNodes;

    for (int i = 0; i < rawNodes.size(); ++i) {
        const QString usbPath = nearestUsbDevice(rawNodes.at(i).sysPath);
        if (usbPath.isEmpty()) {
            nonUsbNodes.push_back(i);
        } else {
            usbGroups[usbPath].push_back(i);
        }
    }

    for (auto it = usbGroups.cbegin(); it != usbGroups.cend(); ++it) {
        const QString& usbPath = it.key();
        const QVector<int>& matches = it.value();

        DeviceInfo device;
        device.vendorId = parseHex(readTextFile(QDir(usbPath).filePath(QStringLiteral("idVendor"))));
        device.productId = parseHex(readTextFile(QDir(usbPath).filePath(QStringLiteral("idProduct"))));
        device.manufacturer = readTextFile(QDir(usbPath).filePath(QStringLiteral("manufacturer")));
        const QString usbProduct = readTextFile(QDir(usbPath).filePath(QStringLiteral("product")));
        device.sysPath = usbPath;

        quint32 inferredBus = 0x03;
        for (const int index : matches) {
            const RawHidNode& node = rawNodes.at(index);
            device.hidrawNodes.push_back(node.node);
            appendUnique(device.reportedNames, node.name);
            device.readable = device.readable || node.readable;
            device.writable = device.writable || node.writable;
            if (node.bus != 0) {
                inferredBus = node.bus;
            }
        }

        device.bus = inferredBus;
        device.name = usbProduct.trimmed();
        if (device.name.isEmpty()) {
            device.name = device.reportedNames.isEmpty()
                ? QStringLiteral("HID device")
                : device.reportedNames.constFirst();
        }

        // Prefer a more specific kernel HID name when the USB product is just
        // a generic receiver label.
        if (device.name.contains(QStringLiteral("receiver"), Qt::CaseInsensitive)) {
            for (const QString& reported : device.reportedNames) {
                if (!reported.contains(QStringLiteral("receiver"), Qt::CaseInsensitive)) {
                    device.name = reported;
                    break;
                }
            }
        }

        device.isLogitechFamily = isLogitechFamily(
            device.vendorId, device.manufacturer, usbProduct, device.reportedNames);

        if (device.manufacturer.isEmpty() && device.isLogitechFamily) {
            device.manufacturer = QStringLiteral("Logitech / ASTRO");
        }

        applyKnownMetadata(device);
        devices.push_back(device);
    }

    QMap<QString, DeviceInfo> remainingGroups;
    for (const int index : nonUsbNodes) {
        const RawHidNode& node = rawNodes.at(index);
        const QString key = QStringLiteral("%1:%2:%3:%4")
            .arg(node.bus)
            .arg(node.vendorId)
            .arg(node.productId)
            .arg(node.name.toLower());

        DeviceInfo& device = remainingGroups[key];
        if (device.name.isEmpty()) {
            device.name = node.name.isEmpty() ? QStringLiteral("HID device") : node.name;
            device.vendorId = node.vendorId;
            device.productId = node.productId;
            device.bus = node.bus;
            device.sysPath = node.sysPath;
        }

        device.hidrawNodes.push_back(node.node);
        device.readable = device.readable || node.readable;
        device.writable = device.writable || node.writable;
        appendUnique(device.reportedNames, node.name);
    }

    for (DeviceInfo device : remainingGroups) {
        device.isLogitechFamily = isLogitechFamily(
            device.vendorId, device.manufacturer, device.name, device.reportedNames);
        if (device.manufacturer.isEmpty() && device.isLogitechFamily) {
            device.manufacturer = QStringLiteral("Logitech / ASTRO");
        }
        applyKnownMetadata(device);
        devices.push_back(device);
    }

    linkRelatedDevices(devices);

    std::sort(devices.begin(), devices.end(), [](const DeviceInfo& a, const DeviceInfo& b) {
        if (a.isLogitechFamily != b.isLogitechFamily) {
            return a.isLogitechFamily > b.isLogitechFamily;
        }

        const QString aFamily = familyKey(a);
        const QString bFamily = familyKey(b);
        if (aFamily != bFamily) {
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        }

        const bool aReceiver = a.role.contains(QStringLiteral("receiver"), Qt::CaseInsensitive);
        const bool bReceiver = b.role.contains(QStringLiteral("receiver"), Qt::CaseInsensitive);
        if (aReceiver != bReceiver) {
            return !aReceiver;
        }

        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return devices;
}

} // namespace openhub
