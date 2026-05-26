#pragma once

#include <QDialog>

namespace FlashSentry {

/** First-run overview of ISO verify vs watch-folder modes. */
class WelcomeWizard : public QDialog {
    Q_OBJECT

public:
    explicit WelcomeWizard(QWidget* parent = nullptr);
};

} // namespace FlashSentry
