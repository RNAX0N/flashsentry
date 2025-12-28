#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <memory>

#include "Types.h"

namespace FlashSentry {

/**
 * @brief TrayIcon - System tray integration for FlashSentry
 * 
 * Provides:
 * - Status icon with dynamic updates
 * - Context menu for quick actions
 * - Desktop notifications
 * - Minimize to tray functionality
 */
class TrayIcon : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Tray icon states
     */
    enum class IconState {
        Normal,         // Default state
        Hashing,        // Hash operation in progress
        Warning,        // Hash mismatch or unknown device
        Error,          // Error state
        Notification    // Temporary notification state
    };
    Q_ENUM(IconState)

    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    // Prevent copying
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    /**
     * @brief Check if system tray is available
     */
    static bool isSystemTrayAvailable();

    /**
     * @brief Show the tray icon
     */
    void show();

    /**
     * @brief Hide the tray icon
     */
    void hide();

    /**
     * @brief Check if tray icon is visible
     */
    bool isVisible() const;

    /**
     * @brief Set the icon state
     */
    void setIconState(IconState state);

    /**
     * @brief Get current icon state
     */
    IconState iconState() const { return m_iconState; }

    /**
     * @brief Update device count in tooltip
     */
    void setDeviceCount(int connected, int whitelisted);

    /**
     * @brief Set whether hashing is in progress
     */
    void setHashingActive(bool active);

    /**
     * @brief Show a notification
     * @param title Notification title
     * @param message Notification body
     * @param icon Icon type
     * @param duration Duration in milliseconds (0 for system default)
     */
    void showNotification(const QString& title, 
                          const QString& message,
                          QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                          int duration = 3000);

    /**
     * @brief Show device connected notification
     */
    void notifyDeviceConnected(const DeviceInfo& device, bool isKnown);

    /**
     * @brief Show device disconnected notification
     */
    void notifyDeviceDisconnected(const QString& deviceName);

    /**
     * @brief Show verification result notification
     */
    void notifyVerificationResult(const QString& deviceName, 
                                   VerificationStatus status);

    /**
     * @brief Show hash completed notification
     */
    void notifyHashCompleted(const QString& deviceName, 
                              uint64_t durationMs,
                              double speedMBps);

    /**
     * @brief Enable or disable notifications
     */
    void setNotificationsEnabled(bool enabled) { m_notificationsEnabled = enabled; }
    bool notificationsEnabled() const { return m_notificationsEnabled; }

signals:
    /**
     * @brief Emitted when user clicks show/hide in context menu
     */
    void showWindowRequested();

    /**
     * @brief Emitted when user double-clicks the tray icon
     */
    void activated();

    /**
     * @brief Emitted when user clicks quit in context menu
     */
    void quitRequested();

    /**
     * @brief Emitted when user clicks settings in context menu
     */
    void settingsRequested();

    /**
     * @brief Emitted when user clicks on a notification
     */
    void notificationClicked();

    /**
     * @brief Emitted when user selects a device from the menu
     */
    void deviceSelected(const QString& deviceNode);

    /**
     * @brief Emitted when user requests to eject a device
     */
    void deviceEjectRequested(const QString& deviceNode);

public slots:
    /**
     * @brief Update the list of connected devices in the menu
     */
    void updateDeviceList(const QList<DeviceInfo>& devices);

    /**
     * @brief Update menu item to show/hide window
     */
    void updateWindowVisibility(bool windowVisible);

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onMessageClicked();
    void updateHashingAnimation();

private:
    /**
     * @brief Create the tray icon
     */
    void createIcon();

    /**
     * @brief Create the context menu
     */
    void createMenu();

    /**
     * @brief Update the tooltip
     */
    void updateTooltip();

    /**
     * @brief Get icon for current state
     */
    QIcon getIconForState(IconState state, int frame = 0) const;

    /**
     * @brief Create SVG icon with specified color
     */
    QIcon createColoredIcon(const QColor& color) const;

    // System tray icon
    std::unique_ptr<QSystemTrayIcon> m_trayIcon;

    // Context menu
    std::unique_ptr<QMenu> m_menu;
    QAction* m_showHideAction = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_quitAction = nullptr;
    QMenu* m_devicesMenu = nullptr;

    // State
    IconState m_iconState = IconState::Normal;
    int m_connectedDevices = 0;
    int m_whitelistedDevices = 0;
    bool m_hashingActive = false;
    bool m_notificationsEnabled = true;

    // Animation
    QTimer* m_animationTimer = nullptr;
    int m_animationFrame = 0;
    static constexpr int ANIMATION_FRAMES = 8;
    static constexpr int ANIMATION_INTERVAL_MS = 100;

    // Current devices for menu
    QList<DeviceInfo> m_currentDevices;
};

} // namespace FlashSentry