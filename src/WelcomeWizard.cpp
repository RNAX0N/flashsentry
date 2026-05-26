#include "WelcomeWizard.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace FlashSentry {

WelcomeWizard::WelcomeWizard(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Welcome to FlashSentry"));
    setMinimumWidth(480);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral(
        "<h2>Verify USB images with confidence</h2>"
        "<p><b>ISO / image verify</b> — Drop Linux ISOs, Windows installers, or Raspberry Pi "
        "<code>.img.xz</code> files on a stick (or use Ventoy). FlashSentry matches publisher "
        "checksums and signatures automatically.</p>"
        "<p><b>Watch folders</b> — Pick folders on a drive and save a Merkle baseline. "
        "Later connects compare only those paths (fast).</p>"
        "<p><b>Full partition hash</b> — Optional, slow byte-for-byte scan for advanced users.</p>"
        "<p>Defaults: ISO verify on USB mount is <b>on</b>; full-disk hash on connect is <b>off</b>.</p>")));

    auto* ok = new QPushButton(QStringLiteral("Get started"));
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(ok);
}

} // namespace FlashSentry
