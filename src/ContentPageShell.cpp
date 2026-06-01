#include "ContentPageShell.h"
#include "StyleManager.h"

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace FlashSpartan {

ContentPageShell::ContentPageShell(const QString& title, QWidget* content, QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* titleLabel = new QLabel(title);
    titleLabel->setFont(FSFont(Heading2));
    titleLabel->setStyleSheet(QString("color: %1; padding: 20px 24px 8px 24px;")
                                  .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));
    root->addWidget(titleLabel);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(FSStyle.scrollAreaStyleSheet());
    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

void ContentPageShell::setContent(QWidget* content)
{
    if (auto* scroll = findChild<QScrollArea*>()) {
        scroll->setWidget(content);
    }
}

} // namespace FlashSpartan
