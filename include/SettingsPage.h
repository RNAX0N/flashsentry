#pragma once

#include "Types.h"
#include "StyleManager.h"

#include <QWidget>

namespace FlashSpartan {

class SettingsDialog;

/** Full settings UI embedded in the main window (no modal). */
class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

    void loadSettings(const AppSettings& settings);
    AppSettings currentSettings() const;
    void setDatabaseStatistics(int deviceCount, const QString& databasePath);

signals:
    void settingsApplyRequested(const AppSettings& settings);
    void liveSettingsChanged(const AppSettings& settings);
    void themeChanged(StyleManager::Theme theme);
    void exportDatabaseRequested(const QString& path);
    void importDatabaseRequested(const QString& path);
    void backupDatabaseRequested();
    void clearDatabaseRequested();

private:
    SettingsDialog* m_form = nullptr;
};

} // namespace FlashSpartan
