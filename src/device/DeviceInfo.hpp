#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace openhub {

struct DeviceInfo {
    QString name;
    QString manufacturer;
    quint32 bus{0};
    quint32 vendorId{0};
    quint32 productId{0};
    QString sysPath;
    QString transport;
    QStringList hidrawNodes;
    QStringList reportedNames;
    bool readable{false};
    bool writable{false};
    bool isLogitechFamily{false};

    [[nodiscard]] QString idString() const
    {
        return QStringLiteral("%1:%2")
            .arg(vendorId, 4, 16, QLatin1Char('0'))
            .arg(productId, 4, 16, QLatin1Char('0'))
            .toUpper();
    }
};

} // namespace openhub
