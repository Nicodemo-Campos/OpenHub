#pragma once

#include "DeviceInfo.hpp"

#include <QVector>

namespace openhub {

class DeviceScanner {
public:
    [[nodiscard]] QVector<DeviceInfo> scan() const;
};

} // namespace openhub
