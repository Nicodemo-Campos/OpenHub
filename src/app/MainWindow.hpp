#pragma once

#include "../device/DeviceInfo.hpp"
#include "../device/DeviceScanner.hpp"

#include <QMainWindow>
#include <QVector>

class QCheckBox;
class QLabel;
class QVBoxLayout;

namespace openhub {

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void refreshDevices();
    void rebuildDeviceCards();
    void showInspector(const DeviceInfo& device);
    [[nodiscard]] QString buildReport(const DeviceInfo& device) const;

    DeviceScanner scanner_;
    QVector<DeviceInfo> devices_;

    QLabel* statsLabel_{nullptr};
    QCheckBox* showAllCheck_{nullptr};
    QVBoxLayout* deviceLayout_{nullptr};
};

} // namespace openhub
