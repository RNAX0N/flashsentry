#pragma once

#include "Types.h"

#include <QWizard>

namespace FlashSentry {

/**
 * First-run wizard: profiles, storage group, and desktop automount guidance.
 */
class WelcomeWizard : public QWizard {
    Q_OBJECT

public:
    explicit WelcomeWizard(QWidget* parent = nullptr);

    /** Profile id chosen on the profile page (normalized). */
    QString selectedProfileId() const;

    /** Apply profile + wizard flags into settings (does not save QSettings). */
    void applyToSettings(AppSettings& settings) const;

private:
    void setupPages();
};

} // namespace FlashSentry
