#pragma once

#include <QWidget>

class QLabel;

namespace FlashSpartan {

/** Standard page chrome: title + scrollable content. */
class ContentPageShell : public QWidget {
public:
    explicit ContentPageShell(const QString& title, QWidget* content, QWidget* parent = nullptr);

    void setContent(QWidget* content);
};

} // namespace FlashSpartan
