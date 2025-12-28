#include "TrayIcon.h"
#include "StyleManager.h"

#include <QApplication>
#include <QPainter>
#include <QBuffer>
#include <QDebug>

namespace FlashSentry {

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent)
{
    createIcon();
    createMenu();
    
    // Setup animation timer
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(ANIMATION_INTERVAL_MS);
    connect(m_animationTimer, &QTimer::timeout, this, &TrayIcon::updateHashingAnimation);
}

TrayIcon::~TrayIcon()
{
    if (m_animationTimer->isActive()) {
        m_animationTimer->stop();
    }
}

bool TrayIcon::isSystemTrayAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon::show()
{
    if (m_trayIcon) {
        m_trayIcon->show();
    }
}

void TrayIcon::hide()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

bool TrayIcon::isVisible() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

void TrayIcon::setIconState(IconState state)
{
    if (m_iconState == state) return;
    
    m_iconState = state;
    
    // Start/stop animation based on state
    if (state == IconState::Hashing) {
        m_animationFrame = 0;
        m_animationTimer->start();
    } else {
        m_animationTimer->stop();
        m_trayIcon->setIcon(getIconForState(state));
    }
    
    updateTooltip();
}

void TrayIcon::setDeviceCount(int connected, int whitelisted)
{
    m_connectedDevices = connected;
    m_whitelistedDevices = whitelisted;
    updateTooltip();
}

void TrayIcon::setHashingActive(bool active)
{
    m_hashingActive = active;
    
    if (active && m_iconState != IconState::Warning && m_iconState != IconState::Error) {
        setIconState(IconState::Hashing);
    } else if (!active && m_iconState == IconState::Hashing) {
        setIconState(IconState::Normal);
    }
}

void TrayIcon::showNotification(const QString& title, 
                                 const QString& message,
                                 QSystemTrayIcon::MessageIcon icon,
                                 int duration)
{
    if (!m_notificationsEnabled || !m_trayIcon) return;
    
    m_trayIcon->showMessage(title, message, icon, duration);
}

void TrayIcon::notifyDeviceConnected(const DeviceInfo& device, bool isKnown)
{
    if (!m_notificationsEnabled) return;
    
    QString title = isKnown ? "Known Device Connected" : "New Device Detected";
    QString message = QString("%1\n%2")
        .arg(device.displayName())
        .arg(device.deviceNode);
    
    QSystemTrayIcon::MessageIcon icon = isKnown ? 
        QSystemTrayIcon::Information : QSystemTrayIcon::Warning;
    
    showNotification(title, message, icon);
    
    if (!isKnown) {
        setIconState(IconState::Warning);
    }
}

void TrayIcon::notifyDeviceDisconnected(const QString& deviceName)
{
    if (!m_notificationsEnabled) return;
    
    showNotification("Device Disconnected", 
                     deviceName + " was safely removed.",
                     QSystemTrayIcon::Information);
}

void TrayIcon::notifyVerificationResult(const QString& deviceName, 
                                         VerificationStatus status)
{
    if (!m_notificationsEnabled) return;
    
    QString title;
    QString message;
    QSystemTrayIcon::MessageIcon icon;
    
    switch (status) {
        case VerificationStatus::Verified:
            title = "Device Verified ✓";
            message = deviceName + " matches stored hash.";
            icon = QSystemTrayIcon::Information;
            setIconState(IconState::Normal);
            break;
            
        case VerificationStatus::Modified:
            title = "⚠️ SECURITY ALERT";
            message = deviceName + " has been MODIFIED since last use!";
            icon = QSystemTrayIcon::Critical;
            setIconState(IconState::Warning);
            break;
            
        case VerificationStatus::NewDevice:
            title = "New Device";
            message = deviceName + " is not in the whitelist.";
            icon = QSystemTrayIcon::Warning;
            setIconState(IconState::Warning);
            break;
            
        case VerificationStatus::Error:
            title = "Verification Error";
            message = "Failed to verify " + deviceName;
            icon = QSystemTrayIcon::Critical;
            setIconState(IconState::Error);
            break;
            
        default:
            return;
    }
    
    showNotification(title, message, icon);
}

void TrayIcon::notifyHashCompleted(const QString& deviceName, 
                                    uint64_t durationMs,
                                    double speedMBps)
{
    if (!m_notificationsEnabled) return;
    
    double seconds = durationMs / 1000.0;
    QString duration;
    if (seconds < 60) {
        duration = QString("%1 seconds").arg(seconds, 0, 'f', 1);
    } else {
        int minutes = static_cast<int>(seconds / 60);
        int secs = static_cast<int>(seconds) % 60;
        duration = QString("%1m %2s").arg(minutes).arg(secs);
    }
    
    QString message = QString("%1\nCompleted in %2 (%3 MB/s)")
        .arg(deviceName)
        .arg(duration)
        .arg(speedMBps, 0, 'f', 1);
    
    showNotification("Hash Complete", message, QSystemTrayIcon::Information);
}

void TrayIcon::updateDeviceList(const QList<DeviceInfo>& devices)
{
    m_currentDevices = devices;
    
    if (!m_devicesMenu) return;
    
    m_devicesMenu->clear();
    
    if (devices.isEmpty()) {
        QAction* noDevices = m_devicesMenu->addAction("No devices connected");
        noDevices->setEnabled(false);
        return;
    }
    
    for (const auto& device : devices) {
        QString label = QString("%1 (%2)")
            .arg(device.displayName())
            .arg(device.deviceNode.split('/').last());
        
        QMenu* deviceMenu = m_devicesMenu->addMenu(label);
        
        if (device.isMounted) {
            QAction* openAction = deviceMenu->addAction("📂 Open");
            connect(openAction, &QAction::triggered, this, [this, device]() {
                emit deviceSelected(device.deviceNode);
            });
            
            deviceMenu->addSeparator();
        }
        
        QAction* ejectAction = deviceMenu->addAction("⏏ Eject");
        connect(ejectAction, &QAction::triggered, this, [this, device]() {
            emit deviceEjectRequested(device.deviceNode);
        });
    }
    
    setDeviceCount(devices.size(), m_whitelistedDevices);
}

void TrayIcon::updateWindowVisibility(bool windowVisible)
{
    if (m_showHideAction) {
        m_showHideAction->setText(windowVisible ? "Hide Window" : "Show Window");
    }
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::DoubleClick:
        case QSystemTrayIcon::Trigger:
            emit activated();
            emit showWindowRequested();
            break;
        case QSystemTrayIcon::MiddleClick:
            // Could be used for quick action
            break;
        default:
            break;
    }
}

void TrayIcon::onMessageClicked()
{
    emit notificationClicked();
    emit showWindowRequested();
}

void TrayIcon::updateHashingAnimation()
{
    m_animationFrame = (m_animationFrame + 1) % ANIMATION_FRAMES;
    m_trayIcon->setIcon(getIconForState(IconState::Hashing, m_animationFrame));
}

void TrayIcon::createIcon()
{
    m_trayIcon = std::make_unique<QSystemTrayIcon>(this);
    
    m_trayIcon->setIcon(getIconForState(IconState::Normal));
    updateTooltip();
    
    connect(m_trayIcon.get(), &QSystemTrayIcon::activated,
            this, &TrayIcon::onActivated);
    connect(m_trayIcon.get(), &QSystemTrayIcon::messageClicked,
            this, &TrayIcon::onMessageClicked);
}

void TrayIcon::createMenu()
{
    m_menu = std::make_unique<QMenu>();
    m_menu->setStyleSheet(FSStyle.menuStyleSheet());
    
    // Title/header
    QAction* titleAction = m_menu->addAction("FlashSentry");
    titleAction->setEnabled(false);
    QFont titleFont = titleAction->font();
    titleFont.setBold(true);
    titleAction->setFont(titleFont);
    
    m_menu->addSeparator();
    
    // Devices submenu
    m_devicesMenu = m_menu->addMenu("📱 Devices");
    QAction* noDevices = m_devicesMenu->addAction("No devices connected");
    noDevices->setEnabled(false);
    
    m_menu->addSeparator();
    
    // Show/Hide window
    m_showHideAction = m_menu->addAction("Show Window");
    connect(m_showHideAction, &QAction::triggered, this, &TrayIcon::showWindowRequested);
    
    // Settings
    m_settingsAction = m_menu->addAction("⚙️ Settings");
    connect(m_settingsAction, &QAction::triggered, this, &TrayIcon::settingsRequested);
    
    m_menu->addSeparator();
    
    // Quit
    m_quitAction = m_menu->addAction("❌ Quit");
    connect(m_quitAction, &QAction::triggered, this, &TrayIcon::quitRequested);
    
    m_trayIcon->setContextMenu(m_menu.get());
}

void TrayIcon::updateTooltip()
{
    QString tooltip = "FlashSentry\n";
    
    switch (m_iconState) {
        case IconState::Hashing:
            tooltip += "Hashing in progress...";
            break;
        case IconState::Warning:
            tooltip += "⚠️ Attention required";
            break;
        case IconState::Error:
            tooltip += "❌ Error occurred";
            break;
        default:
            tooltip += QString("%1 device(s) connected\n%2 whitelisted")
                .arg(m_connectedDevices)
                .arg(m_whitelistedDevices);
            break;
    }
    
    if (m_trayIcon) {
        m_trayIcon->setToolTip(tooltip);
    }
}

QIcon TrayIcon::getIconForState(IconState state, int frame) const
{
    QColor color;
    
    switch (state) {
        case IconState::Normal:
            color = FSColor(AccentPrimary);
            break;
        case IconState::Hashing:
            // Animate between accent colors
            {
                double phase = static_cast<double>(frame) / ANIMATION_FRAMES;
                QColor c1 = FSColor(AccentPrimary);
                QColor c2 = FSColor(AccentSecondary);
                color = QColor(
                    c1.red() + (c2.red() - c1.red()) * phase,
                    c1.green() + (c2.green() - c1.green()) * phase,
                    c1.blue() + (c2.blue() - c1.blue()) * phase
                );
            }
            break;
        case IconState::Warning:
            color = FSColor(Warning);
            break;
        case IconState::Error:
            color = FSColor(Error);
            break;
        case IconState::Notification:
            color = FSColor(Info);
            break;
    }
    
    return createColoredIcon(color);
}

QIcon TrayIcon::createColoredIcon(const QColor& color) const
{
    // Create a simple shield icon programmatically
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Shield shape
    QPainterPath shield;
    shield.moveTo(32, 4);
    shield.lineTo(58, 14);
    shield.quadTo(58, 40, 32, 58);
    shield.quadTo(6, 40, 6, 14);
    shield.closeSubpath();
    
    // Draw shield outline
    QPen pen(color, 3);
    painter.setPen(pen);
    painter.setBrush(Qt::transparent);
    painter.drawPath(shield);
    
    // Draw USB symbol inside
    painter.setPen(QPen(color, 2));
    
    // USB connector shape (simplified)
    painter.drawLine(32, 18, 32, 42);  // Vertical line
    painter.drawLine(24, 25, 40, 25);  // Top horizontal
    painter.drawLine(24, 35, 40, 35);  // Bottom horizontal
    painter.drawEllipse(QPointF(24, 25), 3, 3);
    painter.drawEllipse(QPointF(40, 35), 3, 3);
    painter.drawRect(28, 42, 8, 6);  // USB connector
    
    return QIcon(pixmap);
}

} // namespace FlashSentry