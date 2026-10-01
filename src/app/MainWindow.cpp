#include "MainWindow.hpp"

#include "../device/DeviceKnowledge.hpp"
#include "../hidpp/HidppProbe.hpp"

#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QClipboard>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#ifndef OPENHUB_VERSION
#define OPENHUB_VERSION "dev"
#endif

namespace openhub {
namespace {

QString versionString()
{
    return QString::fromLatin1(OPENHUB_VERSION);
}

QString permissionSummary(const DeviceInfo& device)
{
    if (device.hidrawNodes.isEmpty()) {
        return QStringLiteral("no hidraw endpoint");
    }
    if (!device.readable) {
        return QStringLiteral("permission needed");
    }
    if (!device.writable) {
        return QStringLiteral("read only");
    }
    return QStringLiteral("read + write");
}

QString wirelessSummary(const DeviceInfo& device)
{
    return device.wirelessCapabilities.isEmpty()
        ? QStringLiteral("none identified")
        : device.wirelessCapabilities.join(QStringLiteral(" + "));
}

QString relatedSummary(const DeviceInfo& device)
{
    return device.relatedDevices.isEmpty()
        ? QStringLiteral("None linked")
        : device.relatedDevices.join(QStringLiteral("\n"));
}

QString statusColor(const QString& status)
{
    if (status == QStringLiteral("Implemented") || status == QStringLiteral("Ready")) {
        return QStringLiteral("#34d399");
    }
    if (status == QStringLiteral("Detected") || status == QStringLiteral("Read only")) {
        return QStringLiteral("#60a5fa");
    }
    if (status == QStringLiteral("Planned")) {
        return QStringLiteral("#c084fc");
    }
    if (status == QStringLiteral("Permission needed") || status == QStringLiteral("Unavailable")) {
        return QStringLiteral("#fb7185");
    }
    return QStringLiteral("#fbbf24");
}

QString protocolSummary(const HidppProbeResult& result)
{
    return QStringLiteral("HID++ %1.%2 · endpoint %3 · device index 0x%4 · %5 feature(s)")
        .arg(result.protocolMajor)
        .arg(result.protocolMinor)
        .arg(result.endpoint)
        .arg(result.deviceIndex, 2, 16, QLatin1Char('0'))
        .arg(result.features.size());
}

bool featurePresent(const HidppProbeResult& result, std::initializer_list<quint16> ids)
{
    for (const HidppFeatureInfo& feature : result.features) {
        for (const quint16 id : ids) {
            if (feature.id == id) {
                return true;
            }
        }
    }
    return false;
}

bool isHighlightedFeature(quint16 id)
{
    switch (id) {
    case 0x1000:
    case 0x1001:
    case 0x1004:
    case 0x1B00:
    case 0x1B01:
    case 0x1B02:
    case 0x1B03:
    case 0x1B04:
    case 0x1B05:
    case 0x1B10:
    case 0x1C00:
    case 0x2201:
    case 0x2202:
    case 0x8040:
    case 0x8060:
    case 0x8061:
    case 0x8070:
    case 0x8071:
    case 0x8080:
    case 0x8081:
    case 0x8100:
    case 0x8101:
        return true;
    default:
        return false;
    }
}

QString rateDisplay(quint8 intervalMs)
{
    if (intervalMs == 0) {
        return QStringLiteral("unknown");
    }
    const int hz = static_cast<int>(1000.0 / static_cast<double>(intervalMs) + 0.5);
    return QStringLiteral("%1 Hz (%2 ms)").arg(hz).arg(intervalMs);
}

QString detectedCapabilityText(const HidppProbeResult& result)
{
    QStringList capabilities;
    if (featurePresent(result, {0x1000, 0x1001, 0x1004})) {
        capabilities << QStringLiteral("battery");
    }
    if (featurePresent(result, {0x2201, 0x2202})) {
        capabilities << QStringLiteral("DPI");
    }
    if (featurePresent(result, {0x8060, 0x8061})) {
        capabilities << QStringLiteral("report rate");
    }
    if (featurePresent(result, {0x1B00, 0x1B01, 0x1B02, 0x1B03, 0x1B04, 0x1B05, 0x1B10, 0x1C00})) {
        capabilities << QStringLiteral("buttons/remapping");
    }
    if (featurePresent(result, {0x8070, 0x8071, 0x8080, 0x8081, 0x8040, 0x1981, 0x1982, 0x1983, 0x1990})) {
        capabilities << QStringLiteral("lighting");
    }
    if (featurePresent(result, {0x8100, 0x8101})) {
        capabilities << QStringLiteral("profiles");
    }

    return capabilities.isEmpty()
        ? QStringLiteral("No high-level capability group recognized yet.")
        : QStringLiteral("Detected capability groups: %1.").arg(capabilities.join(QStringLiteral(", ")));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("OpenHub"));
    setMinimumSize(980, 680);
    resize(1120, 780);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(18);

    auto* headerLayout = new QHBoxLayout();
    auto* titleLayout = new QVBoxLayout();

    auto* title = new QLabel(QStringLiteral("OpenHub"), root);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* subtitle = new QLabel(
        QStringLiteral("Linux peripheral control center · v%1").arg(versionString()), root);
    subtitle->setObjectName(QStringLiteral("muted"));

    titleLayout->addWidget(title);
    titleLayout->addWidget(subtitle);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    auto* rescanButton = new QPushButton(QStringLiteral("Rescan devices"), root);
    rescanButton->setObjectName(QStringLiteral("primaryButton"));
    headerLayout->addWidget(rescanButton);
    rootLayout->addLayout(headerLayout);

    auto* safetyFrame = new QFrame(root);
    safetyFrame->setObjectName(QStringLiteral("safetyFrame"));
    auto* safetyLayout = new QHBoxLayout(safetyFrame);
    safetyLayout->setContentsMargins(16, 12, 16, 12);
    auto* safetyLabel = new QLabel(
        QStringLiteral("<b>v0.2.5 button assignments:</b> OpenHub now decodes the active profile's base/G-Shift button table "
                       "in read-only mode. DPI stages and report rate remain writable through the already-validated profile backend; "
                       "button remapping, macros, lighting and firmware writes are still disabled."),
        safetyFrame);
    safetyLabel->setWordWrap(true);
    safetyLayout->addWidget(safetyLabel);
    rootLayout->addWidget(safetyFrame);

    auto* filterLayout = new QHBoxLayout();
    statsLabel_ = new QLabel(QStringLiteral("Scanning…"), root);
    statsLabel_->setObjectName(QStringLiteral("muted"));
    showAllCheck_ = new QCheckBox(QStringLiteral("Show non-Logitech HID devices"), root);

    filterLayout->addWidget(statsLabel_);
    filterLayout->addStretch();
    filterLayout->addWidget(showAllCheck_);
    rootLayout->addLayout(filterLayout);

    auto* scroll = new QScrollArea(root);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    deviceLayout_ = new QVBoxLayout(content);
    deviceLayout_->setContentsMargins(0, 0, 0, 0);
    deviceLayout_->setSpacing(12);
    scroll->setWidget(content);
    rootLayout->addWidget(scroll, 1);

    setCentralWidget(root);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget {
            background: #111318;
            color: #f4f4f5;
            font-size: 14px;
        }
        QLabel#muted {
            color: #9ca3af;
        }
        QLabel#permissionWarning {
            color: #fda4af;
        }
        QFrame#safetyFrame {
            background: #181b22;
            border: 1px solid #2f3542;
            border-radius: 10px;
        }
        QFrame#deviceCard {
            background: #181b22;
            border: 1px solid #2b303b;
            border-radius: 12px;
        }
        QPushButton {
            background: #242936;
            border: 1px solid #363d4d;
            border-radius: 8px;
            padding: 8px 14px;
        }
        QPushButton:hover {
            background: #2d3342;
        }
        QPushButton#primaryButton {
            background: #7c3aed;
            border-color: #8b5cf6;
            color: white;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover {
            background: #8b5cf6;
        }
        QCheckBox {
            color: #d1d5db;
            spacing: 8px;
        }
        QScrollArea {
            background: transparent;
        }
        QTreeWidget, QPlainTextEdit {
            background: #14171d;
            border: 1px solid #2f3542;
            border-radius: 8px;
            alternate-background-color: #181b22;
        }
        QHeaderView::section {
            background: #20242e;
            color: #d1d5db;
            padding: 7px;
            border: 0;
        }
    )"));

    connect(rescanButton, &QPushButton::clicked, this, [this] {
        refreshDevices();
    });
    connect(showAllCheck_, &QCheckBox::toggled, this, [this] {
        rebuildDeviceCards();
    });

    QTimer::singleShot(0, this, [this] {
        refreshDevices();
    });
}

void MainWindow::refreshDevices()
{
    statsLabel_->setText(QStringLiteral("Scanning…"));
    QApplication::processEvents();

    devices_ = scanner_.scan();

    int managed = 0;
    for (const DeviceInfo& device : devices_) {
        if (device.isLogitechFamily) {
            ++managed;
        }
    }
    const int other = devices_.size() - managed;

    statsLabel_->setText(
        QStringLiteral("%1 Logitech/ASTRO device(s) · %2 other HID device(s)")
            .arg(managed)
            .arg(other));

    rebuildDeviceCards();
}

void MainWindow::rebuildDeviceCards()
{
    while (QLayoutItem* item = deviceLayout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    int visibleCount = 0;
    for (const DeviceInfo& device : devices_) {
        if (!showAllCheck_->isChecked() && !device.isLogitechFamily) {
            continue;
        }

        ++visibleCount;
        const SupportProfile profile = DeviceKnowledge::analyze(device);

        auto* card = new QFrame();
        card->setObjectName(QStringLiteral("deviceCard"));
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 18, 16);
        cardLayout->setSpacing(8);

        auto* top = new QHBoxLayout();
        auto* name = new QLabel(device.name, card);
        QFont nameFont = name->font();
        nameFont.setPointSize(16);
        nameFont.setBold(true);
        name->setFont(nameFont);

        auto* badge = new QLabel(profile.badge, card);
        badge->setStyleSheet(QStringLiteral(
            "QLabel { background: #30234a; color: #d8b4fe; border: 1px solid #6d3aa8; "
            "border-radius: 8px; padding: 4px 8px; font-weight: 600; }"));

        auto* inspect = new QPushButton(QStringLiteral("Inspect"), card);

        top->addWidget(name);
        top->addWidget(badge);
        top->addStretch();
        top->addWidget(inspect);
        cardLayout->addLayout(top);

        const QString manufacturer = device.manufacturer.isEmpty()
            ? QStringLiteral("Unknown manufacturer")
            : device.manufacturer;

        auto* identity = new QLabel(
            QStringLiteral("%1 · VID:PID %2 · %3")
                .arg(manufacturer, device.idString(), device.role),
            card);
        identity->setObjectName(QStringLiteral("muted"));
        cardLayout->addWidget(identity);

        auto* connection = new QLabel(
            QStringLiteral("Connected now: %1 · Wireless capability: %2")
                .arg(device.currentConnection, wirelessSummary(device)),
            card);
        connection->setObjectName(QStringLiteral("muted"));
        cardLayout->addWidget(connection);

        auto* support = new QLabel(profile.summary, card);
        support->setWordWrap(true);
        cardLayout->addWidget(support);

        auto* endpoints = new QLabel(
            QStringLiteral("%1 hidraw endpoint(s) · access: %2")
                .arg(device.hidrawNodes.size())
                .arg(permissionSummary(device)),
            card);
        endpoints->setObjectName(device.readable
            ? QStringLiteral("muted")
            : QStringLiteral("permissionWarning"));
        cardLayout->addWidget(endpoints);

        if (HidppProbe::isEligible(device)) {
            auto* probeHint = new QLabel(
                QStringLiteral("Validated HID++ controls available · open Inspect to read state and configure supported active values."),
                card);
            probeHint->setObjectName(QStringLiteral("muted"));
            cardLayout->addWidget(probeHint);
        }

        if (!device.relatedDevices.isEmpty()) {
            auto* related = new QLabel(
                QStringLiteral("Linked interface: %1").arg(device.relatedDevices.constFirst()),
                card);
            related->setObjectName(QStringLiteral("muted"));
            related->setWordWrap(true);
            cardLayout->addWidget(related);
        }

        connect(inspect, &QPushButton::clicked, this, [this, device] {
            showInspector(device);
        });

        deviceLayout_->addWidget(card);
    }

    if (visibleCount == 0) {
        auto* empty = new QLabel(
            showAllCheck_->isChecked()
                ? QStringLiteral("No HID devices were found in /sys/class/hidraw.")
                : QStringLiteral("No Logitech/ASTRO HID devices were found. "
                                 "You can enable “Show non-Logitech HID devices” to verify that Linux HID discovery is working."),
            centralWidget());
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        empty->setObjectName(QStringLiteral("muted"));
        empty->setMinimumHeight(180);
        deviceLayout_->addWidget(empty);
    }

    deviceLayout_->addStretch();
}

QString MainWindow::buildReport(const DeviceInfo& device) const
{
    const SupportProfile profile = DeviceKnowledge::analyze(device);

    QString report;
    report += QStringLiteral("OpenHub v%1 Device Report\n").arg(versionString());
    report += QStringLiteral("Name: %1\n").arg(device.name);
    report += QStringLiteral("Manufacturer: %1\n").arg(
        device.manufacturer.isEmpty() ? QStringLiteral("Unknown") : device.manufacturer);
    report += QStringLiteral("VID:PID: %1\n").arg(device.idString());
    report += QStringLiteral("Device role: %1\n").arg(device.role);
    report += QStringLiteral("Current connection: %1\n").arg(device.currentConnection);
    report += QStringLiteral("Wireless capabilities: %1\n").arg(wirelessSummary(device));
    report += QStringLiteral("Related interfaces: %1\n").arg(
        device.relatedDevices.isEmpty()
            ? QStringLiteral("none")
            : device.relatedDevices.join(QStringLiteral(" | ")));
    report += QStringLiteral("Sysfs path: %1\n").arg(device.sysPath);
    report += QStringLiteral("hidraw nodes: %1\n").arg(
        device.hidrawNodes.isEmpty() ? QStringLiteral("none") : device.hidrawNodes.join(QStringLiteral(", ")));
    report += QStringLiteral("HID access: %1\n").arg(permissionSummary(device));
    report += QStringLiteral("Kernel-reported names: %1\n").arg(
        device.reportedNames.isEmpty() ? QStringLiteral("none") : device.reportedNames.join(QStringLiteral(" | ")));
    report += QStringLiteral("Support profile: %1\n").arg(profile.title);
    report += QStringLiteral("\nCapabilities:\n");

    for (const Capability& capability : profile.capabilities) {
        report += QStringLiteral("- %1: %2 — %3\n")
            .arg(capability.name, capability.status, capability.note);
    }

    report += QStringLiteral(
        "\nNote: startup discovery is passive. HID++ live-state reads are a separate explicit action in v0.2.1.\n");
    return report;
}

void MainWindow::showInspector(const DeviceInfo& device)
{
    const SupportProfile profile = DeviceKnowledge::analyze(device);

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Device Inspector — %1").arg(device.name));
    dialog.resize(840, 650);

    auto* layout = new QVBoxLayout(&dialog);

    auto* heading = new QLabel(device.name, &dialog);
    QFont headingFont = heading->font();
    headingFont.setPointSize(18);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto* summary = new QLabel(profile.summary, &dialog);
    summary->setWordWrap(true);
    summary->setObjectName(QStringLiteral("muted"));
    layout->addWidget(summary);

    auto* form = new QFormLayout();
    form->addRow(QStringLiteral("Profile:"), new QLabel(profile.title, &dialog));
    form->addRow(QStringLiteral("Manufacturer:"), new QLabel(
        device.manufacturer.isEmpty() ? QStringLiteral("Unknown") : device.manufacturer, &dialog));
    form->addRow(QStringLiteral("VID:PID:"), new QLabel(device.idString(), &dialog));
    form->addRow(QStringLiteral("Device role:"), new QLabel(device.role, &dialog));
    form->addRow(QStringLiteral("Connected now:"), new QLabel(device.currentConnection, &dialog));
    form->addRow(QStringLiteral("Wireless capability:"), new QLabel(wirelessSummary(device), &dialog));
    form->addRow(QStringLiteral("Related interfaces:"), new QLabel(relatedSummary(device), &dialog));
    form->addRow(QStringLiteral("hidraw:"), new QLabel(
        device.hidrawNodes.isEmpty() ? QStringLiteral("None") : device.hidrawNodes.join(QStringLiteral("\n")), &dialog));
    form->addRow(QStringLiteral("HID access:"), new QLabel(permissionSummary(device), &dialog));
    form->addRow(QStringLiteral("Kernel names:"), new QLabel(
        device.reportedNames.isEmpty() ? QStringLiteral("None") : device.reportedNames.join(QStringLiteral("\n")), &dialog));

    auto* sysPath = new QLabel(device.sysPath, &dialog);
    sysPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sysPath->setWordWrap(true);
    form->addRow(QStringLiteral("Sysfs path:"), sysPath);
    layout->addLayout(form);

    auto* tree = new QTreeWidget(&dialog);
    tree->setColumnCount(3);
    tree->setHeaderLabels({QStringLiteral("Capability"), QStringLiteral("Status"), QStringLiteral("Notes")});
    tree->setAlternatingRowColors(true);
    tree->setRootIsDecorated(false);

    for (const Capability& capability : profile.capabilities) {
        auto* item = new QTreeWidgetItem(tree, {capability.name, capability.status, capability.note});
        item->setForeground(1, QBrush(QColor(statusColor(capability.status))));
    }
    tree->header()->setStretchLastSection(true);
    tree->resizeColumnToContents(0);
    tree->resizeColumnToContents(1);
    layout->addWidget(tree, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);

    if (HidppProbe::isEligible(device)) {
        auto* probeButton = buttons->addButton(
            QStringLiteral("Open HID++ controls"),
            QDialogButtonBox::ActionRole);
        probeButton->setToolTip(
            QStringLiteral("Reads HID++ state and exposes only validated DPI/report-rate controls when those features are present."));
        connect(probeButton, &QPushButton::clicked, &dialog, [this, device] {
            showHidppProbe(device);
        });
    }

    auto* copyButton = buttons->addButton(QStringLiteral("Copy report"), QDialogButtonBox::ActionRole);
    connect(copyButton, &QPushButton::clicked, &dialog, [this, device] {
        QApplication::clipboard()->setText(buildReport(device));
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::showHidppProbe(const DeviceInfo& device)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const HidppProbeResult result = HidppProbe::probe(device);
    HidppLiveStateResult liveState;
    if (result.success) {
        liveState = HidppProbe::readLiveState(device, result);
    }
    QApplication::restoreOverrideCursor();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("HID++ Controls — %1").arg(device.name));
    dialog.resize(960, 760);

    auto* layout = new QVBoxLayout(&dialog);

    auto* heading = new QLabel(
        result.success ? QStringLiteral("HID++ control session ready")
                       : QStringLiteral("HID++ session did not complete"),
        &dialog);
    QFont headingFont = heading->font();
    headingFont.setPointSize(17);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto* safety = new QLabel(
        QStringLiteral("Reading remains non-mutating. Writes occur only after an explicit Apply/Save action. "
                       "v0.2.4 can persist report rate and the five DPI stages in the CRC-validated active profile sector. "
                       "RGB, button remaps, macros, profile-directory changes and firmware writes remain disabled."),
        &dialog);
    safety->setWordWrap(true);
    safety->setObjectName(QStringLiteral("muted"));
    layout->addWidget(safety);

    if (result.success) {
        auto* protocol = new QLabel(protocolSummary(result), &dialog);
        protocol->setWordWrap(true);
        layout->addWidget(protocol);

        auto* capabilities = new QLabel(detectedCapabilityText(result), &dialog);
        capabilities->setWordWrap(true);
        capabilities->setObjectName(QStringLiteral("muted"));
        layout->addWidget(capabilities);

        auto* liveTitle = new QLabel(QStringLiteral("Live state"), &dialog);
        QFont liveTitleFont = liveTitle->font();
        liveTitleFont.setBold(true);
        liveTitle->setFont(liveTitleFont);
        layout->addWidget(liveTitle);

        QTreeWidget* stateTree = nullptr;
        if (liveState.success) {
            stateTree = new QTreeWidget(&dialog);
            stateTree->setColumnCount(3);
            stateTree->setHeaderLabels({
                QStringLiteral("Value"),
                QStringLiteral("Current"),
                QStringLiteral("Details")
            });
            stateTree->setRootIsDecorated(false);
            stateTree->setAlternatingRowColors(true);
            stateTree->setMaximumHeight(190);

            for (const HidppLiveValue& value : liveState.values) {
                auto* item = new QTreeWidgetItem(stateTree, {
                    value.name,
                    value.current,
                    value.details
                });
                item->setForeground(1, QBrush(QColor(QStringLiteral("#34d399"))));
            }

            stateTree->resizeColumnToContents(0);
            stateTree->resizeColumnToContents(1);
            stateTree->header()->setStretchLastSection(true);
            layout->addWidget(stateTree);
        } else {
            auto* liveError = new QLabel(liveState.error, &dialog);
            liveError->setWordWrap(true);
            liveError->setStyleSheet(QStringLiteral("QLabel { color: #fbbf24; }"));
            layout->addWidget(liveError);
        }

        if (!liveState.warnings.isEmpty()) {
            auto* warning = new QLabel(
                QStringLiteral("Partial read warnings: %1")
                    .arg(liveState.warnings.join(QStringLiteral(" · "))),
                &dialog);
            warning->setWordWrap(true);
            warning->setStyleSheet(QStringLiteral("QLabel { color: #fbbf24; }"));
            layout->addWidget(warning);
        }

        if (!liveState.dpiSensors.isEmpty() || liveState.reportRate.available) {
            auto* controls = new QGroupBox(QStringLiteral("Validated controls"), &dialog);
            auto* controlsLayout = new QVBoxLayout(controls);

            const bool onboardMode =
                liveState.onboardProfile.present && liveState.onboardProfile.mode == 0x01;
            const bool profileRateWritable =
                onboardMode
                && liveState.onboardProfile.metadataReady
                && liveState.onboardProfile.writableLayout
                && liveState.onboardProfile.directoryCrcValid
                && liveState.onboardProfile.profileCrcValid
                && liveState.onboardProfile.activeEnabled
                && liveState.onboardProfile.activeSector != 0xFFFF;

            QString controlMessage;
            if (profileRateWritable) {
                controlMessage = QStringLiteral(
                    "Active DPI can still be changed directly. The on-board profile editor below can persist "
                    "report rate and the five DPI stages by cloning the exact sector, changing only documented bytes "
                    "plus CRC, then verifying the full sector and live state.");
            } else if (onboardMode) {
                QStringList blockers;
                if (!liveState.onboardProfile.metadataReady) {
                    blockers << QStringLiteral("profile descriptor unavailable");
                }
                if (liveState.onboardProfile.metadataReady
                    && !liveState.onboardProfile.writableLayout) {
                    blockers << QStringLiteral("unsupported memory layout");
                }
                if (liveState.onboardProfile.metadataReady
                    && liveState.onboardProfile.writableLayout
                    && !liveState.onboardProfile.directoryCrcValid) {
                    blockers << QStringLiteral("profile directory CRC/index resolution failed");
                }
                if (liveState.onboardProfile.activeSector == 0xFFFF) {
                    blockers << QStringLiteral("active profile sector unresolved");
                }
                if (liveState.onboardProfile.activeSector != 0xFFFF
                    && !liveState.onboardProfile.activeEnabled) {
                    blockers << QStringLiteral("active profile entry disabled");
                }
                if (liveState.onboardProfile.activeSector != 0xFFFF
                    && !liveState.onboardProfile.profileCrcValid) {
                    blockers << QStringLiteral("active profile CRC failed");
                }

                controlMessage = QStringLiteral(
                    "DPI remains available, but persistent report-rate editing is blocked: %1.")
                    .arg(blockers.isEmpty()
                        ? QStringLiteral("profile safety validation did not complete")
                        : blockers.join(QStringLiteral(", ")));
            } else {
                controlMessage = QStringLiteral(
                    "Host mode: DPI and report rate use direct validated HID++ SET + verification GET.");
            }

            auto* controlNote = new QLabel(controlMessage, controls);
            controlNote->setWordWrap(true);
            controlNote->setObjectName(QStringLiteral("muted"));
            controlsLayout->addWidget(controlNote);

            for (int dpiIndex = 0; dpiIndex < liveState.dpiSensors.size(); ++dpiIndex) {
                const HidppDpiState dpiState = liveState.dpiSensors.at(dpiIndex);
                if (!dpiState.available || dpiState.supportedValues.isEmpty()) {
                    continue;
                }

                auto* row = new QHBoxLayout();
                auto* label = new QLabel(
                    liveState.dpiSensors.size() == 1
                        ? QStringLiteral("DPI")
                        : QStringLiteral("DPI sensor %1").arg(dpiState.sensorIndex),
                    controls);
                row->addWidget(label);

                QSpinBox* spin = nullptr;
                QComboBox* combo = nullptr;

                if (dpiState.stepDpi > 0
                    && dpiState.minimumDpi > 0
                    && dpiState.maximumDpi >= dpiState.minimumDpi) {
                    spin = new QSpinBox(controls);
                    spin->setRange(dpiState.minimumDpi, dpiState.maximumDpi);
                    spin->setSingleStep(dpiState.stepDpi);
                    spin->setSuffix(QStringLiteral(" DPI"));
                    spin->setValue(dpiState.currentDpi);
                    row->addWidget(spin, 1);
                } else {
                    combo = new QComboBox(controls);
                    for (const quint16 supportedDpi : dpiState.supportedValues) {
                        combo->addItem(
                            QStringLiteral("%1 DPI").arg(supportedDpi),
                            static_cast<int>(supportedDpi));
                    }
                    const int currentIndex = combo->findData(static_cast<int>(dpiState.currentDpi));
                    if (currentIndex >= 0) {
                        combo->setCurrentIndex(currentIndex);
                    }
                    row->addWidget(combo, 1);
                }

                auto* applyDpi = new QPushButton(QStringLiteral("Apply DPI"), controls);
                row->addWidget(applyDpi);
                controlsLayout->addLayout(row);

                connect(applyDpi, &QPushButton::clicked, &dialog,
                        [&, spin, combo, dpiState, stateTree] {
                    const quint16 requested = static_cast<quint16>(
                        spin ? spin->value() : combo->currentData().toInt());

                    QApplication::setOverrideCursor(Qt::WaitCursor);
                    const HidppWriteResult write = HidppProbe::setDpi(
                        result, dpiState.sensorIndex, requested);
                    QApplication::restoreOverrideCursor();

                    liveState.configurationWriteAttempted = true;
                    liveState.trace += write.trace;

                    if (!write.success) {
                        liveState.configurationActions.push_back(
                            QStringLiteral("DPI sensor %1 -> %2 DPI: FAILED — %3")
                                .arg(dpiState.sensorIndex)
                                .arg(requested)
                                .arg(write.error));
                        QMessageBox::warning(
                            &dialog,
                            QStringLiteral("DPI change failed"),
                            write.error);
                        return;
                    }

                    liveState.configurationActions.push_back(
                        QStringLiteral("DPI sensor %1 -> %2 DPI: verified")
                            .arg(dpiState.sensorIndex)
                            .arg(requested));

                    for (HidppDpiState& state : liveState.dpiSensors) {
                        if (state.sensorIndex == dpiState.sensorIndex) {
                            state.currentDpi = requested;
                        }
                    }
                    for (HidppLiveValue& value : liveState.values) {
                        if (value.featureId == 0x2201) {
                            const QString expectedName = liveState.dpiSensors.size() == 1
                                ? QStringLiteral("DPI")
                                : QStringLiteral("DPI sensor %1").arg(dpiState.sensorIndex);
                            if (value.name == expectedName) {
                                value.current = QStringLiteral("%1 DPI").arg(requested);
                            }
                        }
                    }

                    const QString rowName = liveState.dpiSensors.size() == 1
                        ? QStringLiteral("DPI")
                        : QStringLiteral("DPI sensor %1").arg(dpiState.sensorIndex);
                    for (int i = 0; i < stateTree->topLevelItemCount(); ++i) {
                        QTreeWidgetItem* item = stateTree->topLevelItem(i);
                        if (item->text(0) == rowName) {
                            item->setText(1, QStringLiteral("%1 DPI").arg(requested));
                            break;
                        }
                    }

                    QMessageBox::information(
                        &dialog,
                        QStringLiteral("DPI applied"),
                        write.summary);
                });
            }

            if (onboardMode
                && liveState.onboardProfile.dpiSlots.size() == 5
                && !liveState.dpiSensors.isEmpty()) {
                const HidppDpiState bounds = liveState.dpiSensors.constFirst();
                const bool dpiStageEditorSupported =
                    bounds.available
                    && bounds.minimumDpi > 0
                    && bounds.maximumDpi >= bounds.minimumDpi
                    && bounds.stepDpi > 0;

                auto* stagesGroup = new QGroupBox(QStringLiteral("On-board DPI stages"), controls);
                auto* stagesLayout = new QVBoxLayout(stagesGroup);

                auto* stagesNote = new QLabel(
                    QStringLiteral(
                        "Five profile slots are stored on the mouse. A disabled slot is encoded as 0 DPI. "
                        "Default and current stage indexes are kept separate."),
                    stagesGroup);
                stagesNote->setWordWrap(true);
                stagesNote->setObjectName(QStringLiteral("muted"));
                stagesLayout->addWidget(stagesNote);

                QVector<QCheckBox*> stageEnabled;
                QVector<QSpinBox*> stageSpins;

                for (int i = 0; i < 5; ++i) {
                    auto* row = new QHBoxLayout();
                    auto* enabled = new QCheckBox(
                        QStringLiteral("Stage %1").arg(i + 1), stagesGroup);
                    const quint16 storedDpi = liveState.onboardProfile.dpiSlots.at(i);
                    enabled->setChecked(storedDpi != 0);
                    row->addWidget(enabled);

                    auto* spin = new QSpinBox(stagesGroup);
                    if (dpiStageEditorSupported) {
                        spin->setRange(bounds.minimumDpi, bounds.maximumDpi);
                        spin->setSingleStep(bounds.stepDpi);
                    } else {
                        spin->setRange(1, 65535);
                        spin->setSingleStep(1);
                    }
                    spin->setSuffix(QStringLiteral(" DPI"));
                    spin->setValue(
                        storedDpi != 0
                            ? storedDpi
                            : (bounds.minimumDpi > 0 ? bounds.minimumDpi : 100));
                    spin->setEnabled(enabled->isChecked() && dpiStageEditorSupported);
                    row->addWidget(spin, 1);

                    QStringList tags;
                    if (liveState.onboardProfile.defaultDpiIndex == i) {
                        tags << QStringLiteral("default");
                    }
                    if (liveState.onboardProfile.currentDpiIndex == i) {
                        tags << QStringLiteral("current");
                    }
                    if (liveState.onboardProfile.shiftedDpiIndex == i) {
                        tags << QStringLiteral("shift");
                    }
                    auto* tagLabel = new QLabel(
                        tags.isEmpty() ? QString() : QStringLiteral("(%1)").arg(tags.join(QStringLiteral(", "))),
                        stagesGroup);
                    tagLabel->setObjectName(QStringLiteral("muted"));
                    row->addWidget(tagLabel);

                    connect(enabled, &QCheckBox::toggled, spin, [spin, dpiStageEditorSupported](bool checked) {
                        spin->setEnabled(checked && dpiStageEditorSupported);
                    });

                    stageEnabled.push_back(enabled);
                    stageSpins.push_back(spin);
                    stagesLayout->addLayout(row);
                }

                auto* selectors = new QHBoxLayout();
                selectors->addWidget(new QLabel(QStringLiteral("Default stage"), stagesGroup));

                auto* defaultStage = new QComboBox(stagesGroup);
                for (int i = 0; i < 5; ++i) {
                    defaultStage->addItem(QStringLiteral("Stage %1").arg(i + 1), i);
                }
                if (liveState.onboardProfile.defaultDpiIndex < 5) {
                    defaultStage->setCurrentIndex(liveState.onboardProfile.defaultDpiIndex);
                }
                selectors->addWidget(defaultStage);

                selectors->addSpacing(16);
                selectors->addWidget(new QLabel(QStringLiteral("Current stage"), stagesGroup));

                auto* currentStage = new QComboBox(stagesGroup);
                auto refillCurrentStages = [currentStage, &liveState] {
                    currentStage->clear();
                    for (int i = 0; i < liveState.onboardProfile.dpiSlots.size(); ++i) {
                        const quint16 dpi = liveState.onboardProfile.dpiSlots.at(i);
                        if (dpi != 0) {
                            currentStage->addItem(
                                QStringLiteral("Stage %1 — %2 DPI").arg(i + 1).arg(dpi), i);
                        }
                    }
                    const int current = currentStage->findData(
                        static_cast<int>(liveState.onboardProfile.currentDpiIndex));
                    if (current >= 0) {
                        currentStage->setCurrentIndex(current);
                    }
                };
                refillCurrentStages();
                selectors->addWidget(currentStage, 1);

                auto* activateStage = new QPushButton(QStringLiteral("Activate stage"), stagesGroup);
                activateStage->setEnabled(currentStage->count() > 0 && profileRateWritable);
                selectors->addWidget(activateStage);
                stagesLayout->addLayout(selectors);

                auto* saveStages = new QPushButton(
                    QStringLiteral("Save DPI stages to active profile"), stagesGroup);
                saveStages->setEnabled(profileRateWritable && dpiStageEditorSupported);
                if (!dpiStageEditorSupported) {
                    saveStages->setToolTip(
                        QStringLiteral("This profile editor currently requires a device-reported DPI range with a fixed step."));
                }
                stagesLayout->addWidget(saveStages);

                connect(activateStage, &QPushButton::clicked, &dialog,
                        [&, currentStage, stateTree] {
                    if (currentStage->currentIndex() < 0) {
                        return;
                    }
                    const quint8 requestedIndex =
                        static_cast<quint8>(currentStage->currentData().toInt());

                    QApplication::setOverrideCursor(Qt::WaitCursor);
                    const HidppWriteResult write =
                        HidppProbe::setOnboardCurrentDpiIndex(result, requestedIndex);
                    QApplication::restoreOverrideCursor();

                    liveState.configurationWriteAttempted = true;
                    liveState.trace += write.trace;

                    if (!write.success) {
                        liveState.configurationActions.push_back(
                            QStringLiteral("Activate DPI stage %1: FAILED — %2")
                                .arg(requestedIndex + 1)
                                .arg(write.error));
                        QMessageBox::warning(
                            &dialog, QStringLiteral("DPI stage change failed"), write.error);
                        return;
                    }

                    liveState.onboardProfile.currentDpiIndex = requestedIndex;
                    const quint16 dpi = liveState.onboardProfile.dpiSlots.at(requestedIndex);
                    if (!liveState.dpiSensors.isEmpty()) {
                        liveState.dpiSensors[0].currentDpi = dpi;
                    }
                    for (HidppLiveValue& value : liveState.values) {
                        if (value.featureId == 0x2201) {
                            value.current = QStringLiteral("%1 DPI").arg(dpi);
                        }
                    }
                    for (int i = 0; i < stateTree->topLevelItemCount(); ++i) {
                        QTreeWidgetItem* item = stateTree->topLevelItem(i);
                        if (item->text(0) == QStringLiteral("DPI")) {
                            item->setText(1, QStringLiteral("%1 DPI").arg(dpi));
                            break;
                        }
                    }

                    liveState.configurationActions.push_back(
                        QStringLiteral("DPI stage %1 -> %2 DPI: verified")
                            .arg(requestedIndex + 1)
                            .arg(dpi));
                    QMessageBox::information(
                        &dialog, QStringLiteral("DPI stage activated"), write.summary);
                });

                connect(saveStages, &QPushButton::clicked, &dialog,
                        [&, stageEnabled, stageSpins, defaultStage, currentStage, refillCurrentStages, stateTree] {
                    QVector<quint16> requestedSlots;
                    requestedSlots.reserve(5);
                    for (int i = 0; i < 5; ++i) {
                        requestedSlots.push_back(
                            stageEnabled.at(i)->isChecked()
                                ? static_cast<quint16>(stageSpins.at(i)->value())
                                : 0);
                    }

                    const quint8 requestedDefault =
                        static_cast<quint8>(defaultStage->currentData().toInt());
                    if (requestedDefault >= 5 || requestedSlots.at(requestedDefault) == 0) {
                        QMessageBox::warning(
                            &dialog,
                            QStringLiteral("Invalid default DPI stage"),
                            QStringLiteral("The default stage must be enabled before saving."));
                        return;
                    }

                    const auto answer = QMessageBox::question(
                        &dialog,
                        QStringLiteral("Write DPI stages to on-board profile?"),
                        QStringLiteral(
                            "This updates the five DPI-stage values and default-stage index in active profile 0x%1 "
                            "(sector 0x%2).\n\n"
                            "OpenHub preserves the rest of the sector, recomputes CRC, reads it back, reloads the same "
                            "profile, and verifies the live DPI.\n\nContinue?")
                            .arg(liveState.onboardProfile.activeChoice, 4, 16, QLatin1Char('0'))
                            .arg(liveState.onboardProfile.activeSector, 4, 16, QLatin1Char('0')),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
                    if (answer != QMessageBox::Yes) {
                        return;
                    }

                    QApplication::setOverrideCursor(Qt::WaitCursor);
                    const HidppWriteResult write = HidppProbe::setOnboardProfileDpiSlots(
                        result, requestedSlots, requestedDefault);
                    QApplication::restoreOverrideCursor();

                    liveState.configurationWriteAttempted = true;
                    liveState.trace += write.trace;

                    if (!write.success) {
                        liveState.configurationActions.push_back(
                            QStringLiteral("Save active-profile DPI stages: FAILED — %1")
                                .arg(write.error));
                        QMessageBox::warning(
                            &dialog, QStringLiteral("DPI stage save failed"), write.error);
                        return;
                    }

                    quint8 newCurrent = liveState.onboardProfile.currentDpiIndex;
                    if (newCurrent >= 5 || requestedSlots.at(newCurrent) == 0) {
                        newCurrent = requestedDefault;
                    }

                    liveState.onboardProfile.dpiSlots = requestedSlots;
                    liveState.onboardProfile.defaultDpiIndex = requestedDefault;
                    liveState.onboardProfile.currentDpiIndex = newCurrent;
                    refillCurrentStages();

                    const quint16 activeDpi = requestedSlots.at(newCurrent);
                    if (!liveState.dpiSensors.isEmpty()) {
                        liveState.dpiSensors[0].currentDpi = activeDpi;
                    }
                    for (HidppLiveValue& value : liveState.values) {
                        if (value.featureId == 0x2201) {
                            value.current = QStringLiteral("%1 DPI").arg(activeDpi);
                        }
                    }
                    for (int i = 0; i < stateTree->topLevelItemCount(); ++i) {
                        QTreeWidgetItem* item = stateTree->topLevelItem(i);
                        if (item->text(0) == QStringLiteral("DPI")) {
                            item->setText(1, QStringLiteral("%1 DPI").arg(activeDpi));
                            break;
                        }
                    }

                    liveState.configurationActions.push_back(
                        QStringLiteral("Active-profile DPI stages saved and verified"));
                    QMessageBox::information(
                        &dialog, QStringLiteral("DPI stages saved"), write.summary);
                });

                controlsLayout->addWidget(stagesGroup);
            }

            if (!liveState.onboardProfile.buttonAssignments.isEmpty()) {
                auto* assignmentsGroup = new QGroupBox(
                    QStringLiteral("On-board button assignments — read-only"), controls);
                auto* assignmentsLayout = new QVBoxLayout(assignmentsGroup);

                auto* assignmentNote = new QLabel(
                    QStringLiteral(
                        "v0.2.5 decodes the four-byte assignment records stored in the active profile. "
                        "Button numbers are profile slots for now; remapping is intentionally disabled until "
                        "the real G502 table is validated against your hardware."),
                    assignmentsGroup);
                assignmentNote->setWordWrap(true);
                assignmentNote->setObjectName(QStringLiteral("muted"));
                assignmentsLayout->addWidget(assignmentNote);

                auto* assignmentTree = new QTreeWidget(assignmentsGroup);
                assignmentTree->setColumnCount(6);
                assignmentTree->setHeaderLabels({
                    QStringLiteral("Button"),
                    QStringLiteral("Layer"),
                    QStringLiteral("Kind"),
                    QStringLiteral("Assignment"),
                    QStringLiteral("Details"),
                    QStringLiteral("Raw")
                });
                assignmentTree->setRootIsDecorated(false);
                assignmentTree->setAlternatingRowColors(true);
                assignmentTree->setMinimumHeight(220);
                assignmentTree->setMaximumHeight(320);

                for (const HidppButtonAssignment& assignment
                     : liveState.onboardProfile.buttonAssignments) {
                    const QString raw = QString::fromLatin1(
                        assignment.raw.toHex(' ').toUpper());

                    auto* item = new QTreeWidgetItem(assignmentTree, {
                        QString::number(assignment.buttonIndex),
                        assignment.alternateLayer
                            ? QStringLiteral("G-Shift")
                            : QStringLiteral("Base"),
                        assignment.kind,
                        assignment.action,
                        assignment.detail,
                        raw
                    });

                    if (assignment.kind == QStringLiteral("Unknown")
                        || assignment.kind == QStringLiteral("Invalid")) {
                        item->setForeground(2, QBrush(QColor(QStringLiteral("#fb7185"))));
                    } else if (assignment.kind == QStringLiteral("Macro")
                               || assignment.kind == QStringLiteral("Macro stop")) {
                        item->setForeground(2, QBrush(QColor(QStringLiteral("#fbbf24"))));
                    } else if (assignment.kind != QStringLiteral("Unused")) {
                        item->setForeground(3, QBrush(QColor(QStringLiteral("#60a5fa"))));
                    }
                }

                assignmentTree->resizeColumnToContents(0);
                assignmentTree->resizeColumnToContents(1);
                assignmentTree->resizeColumnToContents(2);
                assignmentTree->resizeColumnToContents(3);
                assignmentTree->resizeColumnToContents(5);
                assignmentTree->header()->setStretchLastSection(false);
                assignmentTree->header()->setSectionResizeMode(4, QHeaderView::Stretch);
                assignmentsLayout->addWidget(assignmentTree);

                auto* descriptor = new QLabel(
                    QStringLiteral("%1 base button slot(s)%2 · profile format 0x%3")
                        .arg(liveState.onboardProfile.buttonCount)
                        .arg(liveState.onboardProfile.hasAlternateButtonLayer
                            ? QStringLiteral(" + G-Shift layer")
                            : QString())
                        .arg(liveState.onboardProfile.profileFormat, 2, 16, QLatin1Char('0'))
                        .toUpper(),
                    assignmentsGroup);
                descriptor->setObjectName(QStringLiteral("muted"));
                assignmentsLayout->addWidget(descriptor);

                controlsLayout->addWidget(assignmentsGroup);
            }

            if (liveState.reportRate.available
                && !liveState.reportRate.supportedIntervalsMs.isEmpty()) {
                auto* row = new QHBoxLayout();
                row->addWidget(new QLabel(QStringLiteral("Report rate"), controls));

                auto* combo = new QComboBox(controls);
                for (const quint8 interval : liveState.reportRate.supportedIntervalsMs) {
                    combo->addItem(rateDisplay(interval), static_cast<int>(interval));
                }
                const int currentIndex = combo->findData(
                    static_cast<int>(liveState.reportRate.currentIntervalMs));
                if (currentIndex >= 0) {
                    combo->setCurrentIndex(currentIndex);
                }
                row->addWidget(combo, 1);

                auto* applyRate = new QPushButton(
                    onboardMode
                        ? QStringLiteral("Save active profile rate")
                        : QStringLiteral("Apply report rate"),
                    controls);

                const bool reportRateControlEnabled = !onboardMode || profileRateWritable;
                applyRate->setEnabled(reportRateControlEnabled);

                // Keep the selector interactive even when writing is blocked.
                // This makes the device-supported choices visible and avoids
                // making a safety lock look like a broken combo box.
                combo->setEnabled(true);

                if (onboardMode && !profileRateWritable) {
                    const QString reason = QStringLiteral(
                        "You can inspect supported rates, but saving is disabled until the active profile passes all memory/CRC safety checks.");
                    applyRate->setToolTip(reason);
                    combo->setToolTip(reason);
                } else if (profileRateWritable) {
                    applyRate->setToolTip(
                        QStringLiteral("Persist the selected rate in the currently active on-board profile."));
                }

                row->addWidget(applyRate);
                controlsLayout->addLayout(row);

                connect(applyRate, &QPushButton::clicked, &dialog,
                        [&, combo, stateTree, onboardMode, profileRateWritable] {
                    const quint8 requested = static_cast<quint8>(combo->currentData().toInt());

                    if (onboardMode && profileRateWritable) {
                        const auto answer = QMessageBox::question(
                            &dialog,
                            QStringLiteral("Write active on-board profile?"),
                            QStringLiteral(
                                "This will persist %1 in active profile 0x%2 (sector 0x%3).\n\n"
                                "OpenHub will preserve every other byte, recompute the profile CRC, "
                                "read the sector back, and restore the original sector if the live rate does not verify.\n\n"
                                "Continue?")
                                .arg(rateDisplay(requested))
                                .arg(liveState.onboardProfile.activeChoice, 4, 16, QLatin1Char('0'))
                                .arg(liveState.onboardProfile.activeSector, 4, 16, QLatin1Char('0')),
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No);
                        if (answer != QMessageBox::Yes) {
                            return;
                        }
                    }

                    QApplication::setOverrideCursor(Qt::WaitCursor);
                    const HidppWriteResult write = onboardMode
                        ? HidppProbe::setOnboardProfileReportRate(result, requested)
                        : HidppProbe::setReportRate(result, requested);
                    QApplication::restoreOverrideCursor();

                    liveState.configurationWriteAttempted = true;
                    liveState.trace += write.trace;

                    const QString actionPrefix = onboardMode
                        ? QStringLiteral("Active profile report rate")
                        : QStringLiteral("Report rate");

                    if (!write.success) {
                        liveState.configurationActions.push_back(
                            QStringLiteral("%1 -> %2: FAILED — %3")
                                .arg(actionPrefix, rateDisplay(requested), write.error));
                        QMessageBox::warning(
                            &dialog,
                            QStringLiteral("Report-rate change failed"),
                            write.error);
                        return;
                    }

                    liveState.reportRate.currentIntervalMs = requested;
                    if (onboardMode) {
                        liveState.onboardProfile.activeReportIntervalMs = requested;
                    }

                    for (HidppLiveValue& value : liveState.values) {
                        if (value.featureId == 0x8060) {
                            value.current = rateDisplay(requested);
                        }
                        if (onboardMode && value.featureId == 0x8100) {
                            value.details += QStringLiteral(" · profile rate now %1")
                                .arg(rateDisplay(requested));
                        }
                    }

                    liveState.configurationActions.push_back(
                        QStringLiteral("%1 -> %2: verified")
                            .arg(actionPrefix, rateDisplay(requested)));

                    for (int i = 0; i < stateTree->topLevelItemCount(); ++i) {
                        QTreeWidgetItem* item = stateTree->topLevelItem(i);
                        if (item->text(0) == QStringLiteral("Report rate")) {
                            item->setText(1, rateDisplay(requested));
                            break;
                        }
                    }

                    QMessageBox::information(
                        &dialog,
                        onboardMode
                            ? QStringLiteral("Profile report rate saved")
                            : QStringLiteral("Report rate applied"),
                        write.summary);
                });
            }

            layout->addWidget(controls);
        }

        auto* featuresTitle = new QLabel(QStringLiteral("Discovered features"), &dialog);
        QFont featuresTitleFont = featuresTitle->font();
        featuresTitleFont.setBold(true);
        featuresTitle->setFont(featuresTitleFont);
        layout->addWidget(featuresTitle);

        auto* tree = new QTreeWidget(&dialog);
        tree->setColumnCount(5);
        tree->setHeaderLabels({
            QStringLiteral("Feature ID"),
            QStringLiteral("Name"),
            QStringLiteral("Index"),
            QStringLiteral("Type"),
            QStringLiteral("Version")
        });
        tree->setRootIsDecorated(false);
        tree->setAlternatingRowColors(true);

        for (const HidppFeatureInfo& feature : result.features) {
            const QString version = feature.version >= 0
                ? QString::number(feature.version)
                : QStringLiteral("?");
            auto* item = new QTreeWidgetItem(tree, {
                QStringLiteral("0x%1").arg(feature.id, 4, 16, QLatin1Char('0')).toUpper(),
                feature.name,
                QStringLiteral("0x%1").arg(feature.index, 2, 16, QLatin1Char('0')).toUpper(),
                QStringLiteral("0x%1").arg(feature.type, 2, 16, QLatin1Char('0')).toUpper(),
                version
            });

            if (isHighlightedFeature(feature.id)) {
                item->setForeground(0, QBrush(QColor(QStringLiteral("#c084fc"))));
            }
        }

        for (int column = 0; column < 5; ++column) {
            tree->resizeColumnToContents(column);
        }
        tree->header()->setStretchLastSection(true);
        layout->addWidget(tree, 1);
    } else {
        auto* error = new QLabel(result.error, &dialog);
        error->setWordWrap(true);
        error->setStyleSheet(QStringLiteral("QLabel { color: #fda4af; }"));
        layout->addWidget(error);

        auto* trace = new QPlainTextEdit(&dialog);
        trace->setReadOnly(true);
        trace->setPlainText(result.trace.join(QLatin1Char('\n')));
        layout->addWidget(trace, 1);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* copyButton = buttons->addButton(
        QStringLiteral("Copy control report"),
        QDialogButtonBox::ActionRole);
    connect(copyButton, &QPushButton::clicked, &dialog, [&device, &result, &liveState] {
        QApplication::clipboard()->setText(HidppProbe::formatReport(
            device,
            result,
            result.success ? &liveState : nullptr));
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

} // namespace openhub
