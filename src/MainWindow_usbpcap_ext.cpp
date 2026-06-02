#include "MainWindow.h"

#include <QGuiApplication>

namespace FlashSpartan {

void MainWindow::refreshUsbPcapIntegration()
{
    if (!m_usbPcapInstaller) {
        return;
    }
    m_usbPcapInstaller->refreshInstallState();
}

} // namespace FlashSpartan
