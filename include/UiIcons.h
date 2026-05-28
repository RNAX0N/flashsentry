#pragma once

#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>

namespace FlashSentry::UiIcons {

inline QIcon icon(const char* resourcePath, int size = 24)
{
    QIcon ico(QString::fromUtf8(resourcePath));
    if (!ico.isNull()) {
        return ico;
    }
    return {};
}

inline void setLabelPixmap(QLabel* label, const char* resourcePath, int size)
{
    if (!label) {
        return;
    }
    const QPixmap px = QIcon(QString::fromUtf8(resourcePath)).pixmap(size, size);
    if (!px.isNull()) {
        label->setPixmap(px);
        label->setText({});
    }
}

inline void setButtonIcon(QPushButton* button, const char* resourcePath, int size = 18)
{
    if (!button) {
        return;
    }
    button->setIcon(icon(resourcePath, size));
    button->setIconSize(QSize(size, size));
}

inline void addLeadingSearchAction(QLineEdit* edit, int size = 16)
{
    Q_UNUSED(edit);
    Q_UNUSED(size);
}

} // namespace FlashSentry::UiIcons
