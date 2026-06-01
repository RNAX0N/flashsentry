#pragma once

#include "StyleManager.h"

#include <QMessageBox>

namespace FlashSpartan {

inline void applyDialogStyle(QMessageBox& box)
{
    box.setStyleSheet(FSStyle.messageBoxStyleSheet());
}

inline int showStyledRichQuestion(QWidget* parent, const QString& title, const QString& html,
                                  QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::StandardButton defaultButton = QMessageBox::No)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.setIcon(QMessageBox::Question);
    box.setStandardButtons(buttons);
    box.setDefaultButton(defaultButton);
    applyDialogStyle(box);
    return box.exec();
}

inline int showStyledQuestion(QWidget* parent, const QString& title, const QString& text,
                              QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::StandardButton defaultButton = QMessageBox::No)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(text);
    box.setIcon(QMessageBox::Question);
    box.setStandardButtons(buttons);
    box.setDefaultButton(defaultButton);
    applyDialogStyle(box);
    return box.exec();
}

inline int showStyledWarning(QWidget* parent, const QString& title, const QString& text,
                             QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                             QMessageBox::StandardButton defaultButton = QMessageBox::Ok)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(text);
    box.setIcon(QMessageBox::Warning);
    box.setStandardButtons(buttons);
    box.setDefaultButton(defaultButton);
    applyDialogStyle(box);
    return box.exec();
}

inline int showStyledCritical(QWidget* parent, const QString& title, const QString& text,
                              QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                              QMessageBox::StandardButton defaultButton = QMessageBox::Ok)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(text);
    box.setIcon(QMessageBox::Critical);
    box.setStandardButtons(buttons);
    box.setDefaultButton(defaultButton);
    applyDialogStyle(box);
    return box.exec();
}

} // namespace FlashSpartan
