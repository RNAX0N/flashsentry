#pragma once

#include "AppNavigation.h"

#include <QWidget>
#include <QList>

class QPushButton;
class QVBoxLayout;

namespace FlashSentry {

class NavSidebar : public QWidget {
    Q_OBJECT

public:
    explicit NavSidebar(QWidget* parent = nullptr);

    void setCurrentPage(AppPage page);
    AppPage currentPage() const { return m_current; }

signals:
    void pageSelected(AppPage page);

private:
    void rebuildButtons();
    void onButtonClicked();

    QList<QPushButton*> m_buttons;
    QVBoxLayout* m_layout = nullptr;
    AppPage m_current = AppPage::UsbMonitor;
};

} // namespace FlashSentry
