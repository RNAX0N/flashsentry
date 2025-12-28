#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <memory>

#include "Types.h"

namespace FlashSentry {

/**
 * @brief DeviceCard - Futuristic card widget displaying USB device information
 * 
 * Features:
 * - Animated status indicator with glow effects
 * - Real-time hash progress display
 * - Quick action buttons (mount, unmount, eject, rehash)
 * - Responsive hover effects
 * - Color-coded verification status
 */
class DeviceCard : public QFrame {
    Q_OBJECT
    Q_PROPERTY(qreal glowIntensity READ glowIntensity WRITE setGlowIntensity)
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    /**
     * @brief Card display modes
     */
    enum class DisplayMode {
        Compact,    // Minimal info, single line
        Normal,     // Standard display
        Expanded    // Full details with all actions
    };
    Q_ENUM(DisplayMode)

    explicit DeviceCard(QWidget* parent = nullptr);
    explicit DeviceCard(const DeviceInfo& device, QWidget* parent = nullptr);
    ~DeviceCard() override;

    // Prevent copying
    DeviceCard(const DeviceCard&) = delete;
    DeviceCard& operator=(const DeviceCard&) = delete;

    /**
     * @brief Set the device info to display
     */
    void setDevice(const DeviceInfo& device);

    /**
     * @brief Get the current device info
     */
    DeviceInfo device() const { return m_device; }

    /**
     * @brief Set the device record (from database)
     */
    void setDeviceRecord(const DeviceRecord& record);

    /**
     * @brief Get device unique ID
     */
    QString deviceId() const { return m_device.uniqueId(); }

    /**
     * @brief Get device node path
     */
    QString deviceNode() const { return m_device.deviceNode; }

    /**
     * @brief Set verification status
     */
    void setVerificationStatus(VerificationStatus status);

    /**
     * @brief Get current verification status
     */
    VerificationStatus verificationStatus() const { return m_status; }

    /**
     * @brief Set hash progress (0.0 - 1.0)
     */
    void setHashProgress(double progress);

    /**
     * @brief Set hash speed in MB/s
     */
    void setHashSpeed(double speedMBps);

    /**
     * @brief Set display mode
     */
    void setDisplayMode(DisplayMode mode);

    /**
     * @brief Get current display mode
     */
    DisplayMode displayMode() const { return m_displayMode; }

    /**
     * @brief Enable or disable action buttons
     */
    void setActionsEnabled(bool enabled);

    /**
     * @brief Show/hide the progress section
     */
    void setProgressVisible(bool visible);

    /**
     * @brief Start the status indicator pulse animation
     */
    void startPulseAnimation();

    /**
     * @brief Stop any running animations
     */
    void stopAnimations();

    /**
     * @brief Flash the card (for notifications)
     */
    void flash(const QColor& color = QColor(), int duration = 500);

    // Animation properties
    qreal glowIntensity() const { return m_glowIntensity; }
    void setGlowIntensity(qreal intensity);
    
    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal progress);

signals:
    /**
     * @brief Emitted when mount button is clicked
     */
    void mountRequested(const QString& deviceNode);

    /**
     * @brief Emitted when unmount button is clicked
     */
    void unmountRequested(const QString& deviceNode);

    /**
     * @brief Emitted when eject button is clicked
     */
    void ejectRequested(const QString& deviceNode);

    /**
     * @brief Emitted when rehash button is clicked
     */
    void rehashRequested(const QString& deviceNode);

    /**
     * @brief Emitted when the card is clicked
     */
    void clicked(const QString& deviceNode);

    /**
     * @brief Emitted when the card is double-clicked
     */
    void doubleClicked(const QString& deviceNode);

    /**
     * @brief Emitted when user requests to open the mount point
     */
    void openMountPointRequested(const QString& mountPoint);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onMountClicked();
    void onUnmountClicked();
    void onEjectClicked();
    void onRehashClicked();
    void updatePulse();

private:
    /**
     * @brief Initialize the UI components
     */
    void setupUi();

    /**
     * @brief Update the display based on current state
     */
    void updateDisplay();

    /**
     * @brief Update status indicator appearance
     */
    void updateStatusIndicator();

    /**
     * @brief Update action buttons visibility and state
     */
    void updateActionButtons();

    /**
     * @brief Get color for current verification status
     */
    QColor statusColor() const;

    /**
     * @brief Format size in bytes to human-readable string
     */
    QString formatSize(uint64_t bytes) const;

    /**
     * @brief Create glow effect for widget
     */
    void applyGlowEffect(QWidget* widget, const QColor& color, int blur = 15);

    // Device data
    DeviceInfo m_device;
    DeviceRecord m_record;
    VerificationStatus m_status = VerificationStatus::Unknown;
    DisplayMode m_displayMode = DisplayMode::Normal;

    // UI Components - Header
    QLabel* m_iconLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_statusIndicator = nullptr;
    QLabel* m_statusLabel = nullptr;

    // UI Components - Info
    QLabel* m_devicePathLabel = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QLabel* m_fsTypeLabel = nullptr;
    QLabel* m_mountPointLabel = nullptr;
    QLabel* m_serialLabel = nullptr;
    QLabel* m_lastSeenLabel = nullptr;
    QLabel* m_hashLabel = nullptr;

    // UI Components - Progress
    QWidget* m_progressWidget = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_progressLabel = nullptr;
    QLabel* m_speedLabel = nullptr;

    // UI Components - Actions
    QWidget* m_actionsWidget = nullptr;
    QPushButton* m_mountBtn = nullptr;
    QPushButton* m_unmountBtn = nullptr;
    QPushButton* m_ejectBtn = nullptr;
    QPushButton* m_rehashBtn = nullptr;
    QPushButton* m_openBtn = nullptr;

    // Layouts
    QVBoxLayout* m_mainLayout = nullptr;
    QHBoxLayout* m_headerLayout = nullptr;
    QGridLayout* m_infoLayout = nullptr;
    QHBoxLayout* m_actionsLayout = nullptr;

    // Effects and animations
    QGraphicsDropShadowEffect* m_glowEffect = nullptr;
    QPropertyAnimation* m_glowAnimation = nullptr;
    QPropertyAnimation* m_hoverAnimation = nullptr;
    QTimer* m_pulseTimer = nullptr;

    // Animation state
    qreal m_glowIntensity = 0.0;
    qreal m_hoverProgress = 0.0;
    int m_pulsePhase = 0;

    // Configuration
    static constexpr int CARD_PADDING = 16;
    static constexpr int ICON_SIZE = 48;
    static constexpr int STATUS_INDICATOR_SIZE = 12;
    static constexpr int ANIMATION_DURATION = 200;
    static constexpr int PULSE_INTERVAL = 50;
};

} // namespace FlashSentry