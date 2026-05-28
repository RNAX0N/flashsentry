#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace FlashSentry {

class PlaceholderModulePage : public QWidget {
    Q_OBJECT

public:
    explicit PlaceholderModulePage(const QString& title, const QString& description,
                                   QWidget* parent = nullptr);

    void setPrimaryAction(const QString& text, bool visible = true);

signals:
    void primaryActionTriggered();

private:
    QLabel* m_titleLabel = nullptr;
    QLabel* m_descLabel = nullptr;
    QPushButton* m_actionBtn = nullptr;
};

} // namespace FlashSentry
