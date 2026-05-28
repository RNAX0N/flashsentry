#pragma once

#include "Types.h"

#include <QDialog>

class QComboBox;
class QRadioButton;
class QCheckBox;
class QLabel;

namespace FlashSentry {

/**
 * Lets the user pick partition vs whole-disk target and full vs quick vs watch-only scan.
 */
class HashOptionsDialog : public QDialog {
    Q_OBJECT

public:
    struct Result {
        HashScope scope = HashScope::Partition;
        HashScanMode scanMode = HashScanMode::Full;
        bool resumeFromCheckpoint = false;
        bool accepted = false;
    };

    explicit HashOptionsDialog(const DeviceInfo& device,
                               int partitionCount,
                               bool hasWatchBaseline,
                               bool hasCheckpoint,
                               HashScope defaultScope,
                               HashScanMode defaultMode,
                               QWidget* parent = nullptr);

    Result choice() const { return m_result; }

private:
    void onAccepted();

    DeviceInfo m_device;
    Result m_result;
    QRadioButton* m_partitionRadio = nullptr;
    QRadioButton* m_wholeDiskRadio = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QCheckBox* m_resumeCheck = nullptr;
    QLabel* m_hintLabel = nullptr;
};

} // namespace FlashSentry
