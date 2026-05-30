#pragma once

#include <QWidget>

class QLabel;

namespace FlashSentry {

/** Standard page chrome: title + scrollable content. */
class ContentPageShell : public QWidget {
public:
    explicit ContentPageShell(const QString& title, QWidget* content, QWidget* parent = nullptr);

    void setContent(QWidget* content);
};

} // namespace FlashSentry
