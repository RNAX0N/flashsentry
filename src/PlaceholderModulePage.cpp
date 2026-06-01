#include "PlaceholderModulePage.h"
#include "StyleManager.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace FlashSpartan {

PlaceholderModulePage::PlaceholderModulePage(const QString& title, const QString& description,
                                             QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);

    m_titleLabel = new QLabel(title);
    m_titleLabel->setFont(FSFont(Heading2));
    m_titleLabel->setStyleSheet(QString("color: %1;")
                                    .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));
    layout->addWidget(m_titleLabel);

    m_descLabel = new QLabel(description);
    m_descLabel->setWordWrap(true);
    m_descLabel->setStyleSheet(QString("color: %1;")
                                   .arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    layout->addWidget(m_descLabel);

    m_actionBtn = new QPushButton;
    m_actionBtn->setVisible(false);
    m_actionBtn->setCursor(Qt::PointingHandCursor);
    m_actionBtn->setStyleSheet(FSStyle.primaryButtonStyleSheet());
    connect(m_actionBtn, &QPushButton::clicked, this, &PlaceholderModulePage::primaryActionTriggered);
    layout->addWidget(m_actionBtn, 0, Qt::AlignLeft);

    layout->addStretch();
}

void PlaceholderModulePage::setPrimaryAction(const QString& text, bool visible)
{
    m_actionBtn->setText(text);
    m_actionBtn->setVisible(visible);
}

} // namespace FlashSpartan
