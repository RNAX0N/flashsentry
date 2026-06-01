#include "EventDetailDialog.h"
#include "StyleManager.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace FlashSpartan {

EventDetailDialog::EventDetailDialog(const UiEventEntry& entry, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Event details"));
    setMinimumSize(480, 360);
    setStyleSheet(FSStyle.dialogStyleSheet());

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Time:"), new QLabel(entry.time.toString(Qt::ISODate)));
    form->addRow(QStringLiteral("Event:"), new QLabel(entry.event));
    form->addRow(QStringLiteral("Device:"), new QLabel(entry.device));
    form->addRow(QStringLiteral("Type:"), new QLabel(entry.type));
    form->addRow(QStringLiteral("Result:"), new QLabel(entry.result));
    layout->addLayout(form);

    m_body = new QTextEdit;
    m_body->setReadOnly(true);
    m_body->setPlainText(entry.detail.isEmpty() ? entry.event : entry.detail);
    m_body->setStyleSheet(FSStyle.inputFieldStyleSheet());
    layout->addWidget(m_body, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

} // namespace FlashSpartan
