#include "HashOptionsDialog.h"
#include <QCheckBox>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

namespace FlashSentry {

HashOptionsDialog::HashOptionsDialog(const DeviceInfo& device,
                                     int partitionCount,
                                     bool hasWatchBaseline,
                                     bool hasCheckpoint,
                                     HashScope defaultScope,
                                     HashScanMode defaultMode,
                                     QWidget* parent)
    : QDialog(parent)
    , m_device(device)
{
    setWindowTitle(QStringLiteral("Hash options"));
    setMinimumWidth(460);

    m_result.scope = defaultScope;
    m_result.scanMode = defaultMode;
    m_result.resumeFromCheckpoint = hasCheckpoint;

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(QStringLiteral(
        "<b>%1</b><br>Choose what to fingerprint and how thorough the scan should be.")
                         .arg(device.displayName())));

    auto* scopeGroup = new QGroupBox(QStringLiteral("Target"));
    auto* scopeLayout = new QVBoxLayout(scopeGroup);

    m_partitionRadio = new QRadioButton(
        QStringLiteral("This partition only (%1)").arg(device.deviceNode));
    m_wholeDiskRadio = new QRadioButton(
        QStringLiteral("Entire drive (%1) — all %2 partition(s) on this USB stick")
            .arg(device.parentDevice.isEmpty() ? device.deviceNode : device.parentDevice)
            .arg(qMax(1, partitionCount)));

    scopeLayout->addWidget(m_partitionRadio);
    scopeLayout->addWidget(m_wholeDiskRadio);

    if (device.parentDevice.isEmpty() || partitionCount <= 1) {
        m_wholeDiskRadio->setEnabled(false);
        m_wholeDiskRadio->setToolTip(QStringLiteral("Only one partition is visible for this drive."));
        m_partitionRadio->setChecked(true);
    } else if (defaultScope == HashScope::WholeDisk) {
        m_wholeDiskRadio->setChecked(true);
    } else {
        m_partitionRadio->setChecked(true);
    }

    layout->addWidget(scopeGroup);

    auto* modeGroup = new QGroupBox(QStringLiteral("Scan mode"));
    auto* modeLayout = new QFormLayout(modeGroup);
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem(QStringLiteral("Full partition read (slow, strongest)"),
                         static_cast<int>(HashScanMode::Full));
    m_modeCombo->addItem(QStringLiteral("Quick sample (first/last + spaced 1 MiB chunks)"),
                         static_cast<int>(HashScanMode::QuickSample));
    if (hasWatchBaseline) {
        m_modeCombo->addItem(QStringLiteral("Watch folders only (Merkle manifest, no raw read)"),
                             static_cast<int>(HashScanMode::WatchManifestOnly));
    }
    const int modeIdx = m_modeCombo->findData(static_cast<int>(defaultMode));
    m_modeCombo->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);
    modeLayout->addRow(QStringLiteral("Mode:"), m_modeCombo);
    layout->addWidget(modeGroup);

    m_resumeCheck = new QCheckBox(QStringLiteral("Resume from last saved checkpoint"));
    m_resumeCheck->setChecked(hasCheckpoint);
    m_resumeCheck->setEnabled(hasCheckpoint);
    if (!hasCheckpoint) {
        m_resumeCheck->setToolTip(QStringLiteral("No checkpoint found for this device and mode."));
    }
    layout->addWidget(m_resumeCheck);

    m_hintLabel = new QLabel;
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(m_hintLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &HashOptionsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void HashOptionsDialog::onAccepted()
{
    m_result.scope = m_wholeDiskRadio->isChecked() ? HashScope::WholeDisk : HashScope::Partition;
    m_result.scanMode = static_cast<HashScanMode>(m_modeCombo->currentData().toInt());
    m_result.resumeFromCheckpoint = m_resumeCheck->isChecked();
    m_result.accepted = true;
    accept();
}

} // namespace FlashSentry
