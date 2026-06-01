#pragma once

#include "Types.h"

#include <QDialog>

class QListWidget;
class QLineEdit;
class QPushButton;
class QComboBox;

namespace FlashSpartan {

/**
 * @brief Edit watch groups (paths) and build Merkle baselines for a mounted device.
 */
class WatchListDialog : public QDialog {
    Q_OBJECT

public:
    explicit WatchListDialog(const QString& mountPoint, const QString& deviceDisplayName,
                             WatchManifest manifest, QWidget* parent = nullptr);

    WatchManifest manifest() const { return m_manifest; }
    VerificationProfile selectedProfile() const;
    bool baselineBuilt() const { return m_baselineBuilt; }

signals:
    void buildBaselineRequested(const WatchManifest& spec);

private slots:
    void onAddGroup();
    void onRemoveGroup();
    void onGroupSelectionChanged();
    void onAddFiles();
    void onAddDirectory();
    void onRemovePath();
    void onBuildBaseline();

private:
    void refreshGroupList();
    void loadGroupIntoEditor(int index);
    void saveEditorToCurrentGroup();
    WatchGroup currentGroupFromEditor() const;

    QString m_mountPoint;
    QString m_deviceName;
    WatchManifest m_manifest;
    int m_currentGroupIndex = -1;
    bool m_baselineBuilt = false;

    QListWidget* m_groupList = nullptr;
    QLineEdit* m_groupNameEdit = nullptr;
    QListWidget* m_pathList = nullptr;
    QComboBox* m_profileCombo = nullptr;
    QPushButton* m_buildBtn = nullptr;
};

} // namespace FlashSpartan
