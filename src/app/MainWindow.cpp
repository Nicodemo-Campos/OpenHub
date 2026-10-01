#include "MainWindow.hpp"

#include "../device/DeviceKnowledge.hpp"
#include "../hidpp/HidppProbe.hpp"

#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
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
        .arg(result.features.size())
        .toUpper();
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
    if (featurePresent(result, {0x1B00, 0x1B01, 0x1B02, 0x1B03, 0x1B04, 0x1C00})) {
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
        QStringLiteral("<b>v0.2 capability probe:</b> startup remains passive. "
                       "The HID++ probe runs only when you press the probe button and sends GET/discovery requests only — "
                       "no DPI, lighting, profile, button, or other configuration writes."),
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
                QStringLiteral("HID++ probe available · open Inspect to enumerate live device features."),
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
        "\nNote: startup discovery is passive. HID++ probing is a separate explicit action in v0.2.\n");
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
            QStringLiteral("Probe HID++ (GET only)"),
            QDialogButtonBox::ActionRole);
        probeButton->setToolTip(
            QStringLiteral("Enumerates HID++ protocol/features with non-mutating GET requests."));
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
    QApplication::restoreOverrideCursor();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("HID++ Probe — %1").arg(device.name));
    dialog.resize(900, 680);

    auto* layout = new QVBoxLayout(&dialog);

    auto* heading = new QLabel(
        result.success ? QStringLiteral("HID++ capability probe succeeded")
                       : QStringLiteral("HID++ capability probe did not complete"),
        &dialog);
    QFont headingFont = heading->font();
    headingFont.setPointSize(17);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto* safety = new QLabel(
        QStringLiteral("This probe sends only Root/Feature Set GET requests. "
                       "No SET function, DPI change, lighting command, profile write, or remap command is sent."),
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

            if (featurePresent(result, {feature.id})
                && feature.id != 0x0000
                && feature.id != 0x0001) {
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
        QStringLiteral("Copy probe report"),
        QDialogButtonBox::ActionRole);
    connect(copyButton, &QPushButton::clicked, &dialog, [device, result] {
        QApplication::clipboard()->setText(HidppProbe::formatReport(device, result));
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

} // namespace openhub
