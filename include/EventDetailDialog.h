#pragma once

#include "UiEventTypes.h"

#include <QDialog>

class QTextEdit;

namespace FlashSpartan {

class EventDetailDialog : public QDialog {
    Q_OBJECT

public:
    explicit EventDetailDialog(const UiEventEntry& entry, QWidget* parent = nullptr);

private:
    QTextEdit* m_body = nullptr;
};

} // namespace FlashSpartan
