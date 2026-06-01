#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace FlashSpartan {

class AboutPage : public QWidget {
    Q_OBJECT

public:
    explicit AboutPage(QWidget* parent = nullptr);

    void setVersion(const QString& version);
    void setDatabaseSummary(int deviceCount, const QString& storePath);
    void setRuntimeInfo(const QString& qtVersion, const QString& osName);

signals:
    void openRepositoryRequested();
    void openUserGuideRequested();

private:
    void setupUi();

    QLabel* m_versionLabel = nullptr;
    QLabel* m_dbLabel = nullptr;
    QLabel* m_runtimeLabel = nullptr;
};

} // namespace FlashSpartan
