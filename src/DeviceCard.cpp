#include "DeviceCard.h"
#include "StyleManager.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QDebug>

namespace FlashSentry {

DeviceCard::DeviceCard(QWidget* parent)
    : QFrame(parent)
{
    setupUi();
}

DeviceCard::DeviceCard(const DeviceInfo& device, QWidget* parent)
    : QFrame(parent)
    , m_device(device)
{
    setupUi();
    updateDisplay();
}

DeviceCard::~DeviceCard()
{
    stopAnimations();
}

void DeviceCard::setupUi()
{
    setObjectName("DeviceCard");
    setFrameStyle(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(120);
    
    // Apply card styling
    setStyleSheet(FSStyle.deviceCardStyleSheet());
    
    // Create glow effect
    m_glowEffect = new QGraphicsDropShadowEffect(this);
    m_glowEffect->setBlurRadius(0);
    m_glowEffect->setColor(FSColor(AccentPrimary));
    m_glowEffect->setOffset(0, 0);
    setGraphicsEffect(m_glowEffect);
    
    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(CARD_PADDING, CARD_PADDING, CARD_PADDING, CARD_PADDING);
    m_mainLayout->setSpacing(12);
    
    // === Header Section ===
    QWidget* headerWidget = new QWidget;
    m_headerLayout = new QHBoxLayout(headerWidget);
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(12);
    
    // Device icon
    m_iconLabel = new QLabel;
    m_iconLabel->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet(QString(R"(
        QLabel {
            background-color: %1;
            border-radius: %2px;
            font-size: 24px;
        }
    )").arg(FSStyle.colorCss(StyleManager::ColorRole::BackgroundDark))
       .arg(ICON_SIZE / 4));
    m_iconLabel->setText("💾");  // USB drive emoji
    m_headerLayout->addWidget(m_iconLabel);
    
    // Name and status container
    QVBoxLayout* nameLayout = new QVBoxLayout;
    nameLayout->setSpacing(4);
    
    // Device name
    m_nameLabel = new QLabel;
    m_nameLabel->setFont(FSFont(Heading3));
    m_nameLabel->setStyleSheet(QString("color: %1; font-weight: 600;")
        .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));
    nameLayout->addWidget(m_nameLabel);
    
    // Device path
    m_devicePathLabel = new QLabel;
    m_devicePathLabel->setFont(FSFont(Small));
    m_devicePathLabel->setStyleSheet(QString("color: %1;")
        .arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    nameLayout->addWidget(m_devicePathLabel);
    
    m_headerLayout->addLayout(nameLayout, 1);
    
    // Status indicator
    QVBoxLayout* statusLayout = new QVBoxLayout;
    statusLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusLayout->setSpacing(4);
    
    QHBoxLayout* indicatorLayout = new QHBoxLayout;
    indicatorLayout->setSpacing(8);
    
    m_statusIndicator = new QLabel;
    m_statusIndicator->setObjectName("StatusIndicator");
    m_statusIndicator->setFixedSize(STATUS_INDICATOR_SIZE, STATUS_INDICATOR_SIZE);
    indicatorLayout->addWidget(m_statusIndicator);
    
    m_statusLabel = new QLabel;
    m_statusLabel->setFont(FSFont(Small));
    indicatorLayout->addWidget(m_statusLabel);
    
    statusLayout->addLayout(indicatorLayout);
    m_headerLayout->addLayout(statusLayout);
    
    m_mainLayout->addWidget(headerWidget);
    
    // === Info Section ===
    QWidget* infoWidget = new QWidget;
    m_infoLayout = new QGridLayout(infoWidget);
    m_infoLayout->setContentsMargins(0, 0, 0, 0);
    m_infoLayout->setSpacing(8);
    m_infoLayout->setColumnStretch(1, 1);
    m_infoLayout->setColumnStretch(3, 1);
    
    auto addInfoRow = [this](int row, int col, const QString& label, QLabel*& valueLabel) {
        QLabel* lblLabel = new QLabel(label);
        lblLabel->setFont(FSFont(Small));
        lblLabel->setStyleSheet(QString("color: %1;")
            .arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
        m_infoLayout->addWidget(lblLabel, row, col * 2);
        
        valueLabel = new QLabel("-");
        valueLabel->setFont(FSFont(Small));
        valueLabel->setStyleSheet(QString("color: %1;")
            .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
        m_infoLayout->addWidget(valueLabel, row, col * 2 + 1);
    };
    
    addInfoRow(0, 0, "Size:", m_sizeLabel);
    addInfoRow(0, 1, "Type:", m_fsTypeLabel);
    addInfoRow(1, 0, "Mount:", m_mountPointLabel);
    addInfoRow(1, 1, "Serial:", m_serialLabel);
    
    m_mainLayout->addWidget(infoWidget);
    
    // === Progress Section ===
    m_progressWidget = new QWidget;
    QVBoxLayout* progressLayout = new QVBoxLayout(m_progressWidget);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(6);
    
    QHBoxLayout* progressHeaderLayout = new QHBoxLayout;
    
    m_progressLabel = new QLabel("Calculating hash...");
    m_progressLabel->setFont(FSFont(Small));
    m_progressLabel->setStyleSheet(QString("color: %1;")
        .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    progressHeaderLayout->addWidget(m_progressLabel);
    
    m_speedLabel = new QLabel;
    m_speedLabel->setFont(FSFont(Monospace));
    m_speedLabel->setStyleSheet(QString("color: %1;")
        .arg(FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    progressHeaderLayout->addWidget(m_speedLabel);
    
    progressLayout->addLayout(progressHeaderLayout);
    
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setStyleSheet(FSStyle.progressBarStyleSheet());
    progressLayout->addWidget(m_progressBar);
    
    m_progressWidget->setVisible(false);
    m_mainLayout->addWidget(m_progressWidget);
    
    // === Actions Section ===
    m_actionsWidget = new QWidget;
    m_actionsLayout = new QHBoxLayout(m_actionsWidget);
    m_actionsLayout->setContentsMargins(0, 8, 0, 0);
    m_actionsLayout->setSpacing(8);
    
    auto createActionButton = [this](const QString& text, const QString& tooltip) -> QPushButton* {
        QPushButton* btn = new QPushButton(text);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(32);
        btn->setStyleSheet(FSStyle.buttonStyleSheet());
        return btn;
    };
    
    m_mountBtn = createActionButton("Mount", "Mount this device");
    m_unmountBtn = createActionButton("Unmount", "Safely unmount this device");
    m_ejectBtn = createActionButton("⏏ Eject", "Eject and power off device");
    m_rehashBtn = createActionButton("↻ Rehash", "Recalculate device hash");
    m_openBtn = createActionButton("📂 Open", "Open in file manager");
    
    // Style the eject button as danger
    m_ejectBtn->setStyleSheet(FSStyle.dangerButtonStyleSheet());
    
    // Style rehash as primary
    m_rehashBtn->setStyleSheet(FSStyle.primaryButtonStyleSheet());
    
    connect(m_mountBtn, &QPushButton::clicked, this, &DeviceCard::onMountClicked);
    connect(m_unmountBtn, &QPushButton::clicked, this, &DeviceCard::onUnmountClicked);
    connect(m_ejectBtn, &QPushButton::clicked, this, &DeviceCard::onEjectClicked);
    connect(m_rehashBtn, &QPushButton::clicked, this, &DeviceCard::onRehashClicked);
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        if (!m_device.mountPoint.isEmpty()) {
            emit openMountPointRequested(m_device.mountPoint);
        }
    });
    
    m_actionsLayout->addWidget(m_mountBtn);
    m_actionsLayout->addWidget(m_unmountBtn);
    m_actionsLayout->addWidget(m_openBtn);
    m_actionsLayout->addStretch();
    m_actionsLayout->addWidget(m_rehashBtn);
    m_actionsLayout->addWidget(m_ejectBtn);
    
    m_mainLayout->addWidget(m_actionsWidget);
    
    // Setup hover animation
    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setDuration(ANIMATION_DURATION);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    
    // Setup glow animation
    m_glowAnimation = new QPropertyAnimation(this, "glowIntensity", this);
    m_glowAnimation->setDuration(1000);
    m_glowAnimation->setLoopCount(-1);
    
    // Setup pulse timer
    m_pulseTimer = new QTimer(this);
    connect(m_pulseTimer, &QTimer::timeout, this, &DeviceCard::updatePulse);
    
    updateDisplay();
}

void DeviceCard::setDevice(const DeviceInfo& device)
{
    m_device = device;
    updateDisplay();
}

void DeviceCard::setDeviceRecord(const DeviceRecord& record)
{
    m_record = record;
    updateDisplay();
}

void DeviceCard::setVerificationStatus(VerificationStatus status)
{
    m_status = status;
    updateStatusIndicator();
    
    // Start pulse animation for certain states
    if (status == VerificationStatus::Modified || 
        status == VerificationStatus::Hashing) {
        startPulseAnimation();
    } else {
        stopAnimations();
    }
}

void DeviceCard::setHashProgress(double progress)
{
    int value = static_cast<int>(progress * 1000);
    m_progressBar->setValue(value);
    m_progressLabel->setText(QString("Hashing... %1%").arg(static_cast<int>(progress * 100)));
}

void DeviceCard::setHashSpeed(double speedMBps)
{
    m_speedLabel->setText(QString("%1 MB/s").arg(speedMBps, 0, 'f', 1));
}

void DeviceCard::setDisplayMode(DisplayMode mode)
{
    m_displayMode = mode;
    updateDisplay();
}

void DeviceCard::setActionsEnabled(bool enabled)
{
    m_mountBtn->setEnabled(enabled);
    m_unmountBtn->setEnabled(enabled);
    m_ejectBtn->setEnabled(enabled);
    m_rehashBtn->setEnabled(enabled);
    m_openBtn->setEnabled(enabled);
}

void DeviceCard::setProgressVisible(bool visible)
{
    m_progressWidget->setVisible(visible);
    
    if (visible) {
        setVerificationStatus(VerificationStatus::Hashing);
    }
}

void DeviceCard::startPulseAnimation()
{
    if (!m_pulseTimer->isActive()) {
        m_pulsePhase = 0;
        m_pulseTimer->start(PULSE_INTERVAL);
    }
}

void DeviceCard::stopAnimations()
{
    m_pulseTimer->stop();
    m_glowAnimation->stop();
    m_glowEffect->setBlurRadius(0);
}

void DeviceCard::flash(const QColor& color, int duration)
{
    QColor flashColor = color.isValid() ? color : FSColor(AccentPrimary);
    
    m_glowEffect->setColor(flashColor);
    
    QPropertyAnimation* flashAnim = new QPropertyAnimation(this, "glowIntensity", this);
    flashAnim->setDuration(duration);
    flashAnim->setStartValue(0.0);
    flashAnim->setKeyValueAt(0.5, 1.0);
    flashAnim->setEndValue(0.0);
    flashAnim->start(QPropertyAnimation::DeleteWhenStopped);
}

void DeviceCard::setGlowIntensity(qreal intensity)
{
    m_glowIntensity = intensity;
    m_glowEffect->setBlurRadius(intensity * 25);
    update();
}

void DeviceCard::setHoverProgress(qreal progress)
{
    m_hoverProgress = progress;
    update();
}

void DeviceCard::enterEvent(QEnterEvent* event)
{
    QFrame::enterEvent(event);
    
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
    
    m_glowEffect->setColor(FSColor(AccentPrimary));
    m_glowEffect->setBlurRadius(8);
}

void DeviceCard::leaveEvent(QEvent* event)
{
    QFrame::leaveEvent(event);
    
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(0.0);
    m_hoverAnimation->start();
    
    if (m_status != VerificationStatus::Modified && 
        m_status != VerificationStatus::Hashing) {
        m_glowEffect->setBlurRadius(0);
    }
}

void DeviceCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_device.deviceNode);
    }
    QFrame::mousePressEvent(event);
}

void DeviceCard::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_device.deviceNode);
    }
    QFrame::mouseDoubleClickEvent(event);
}

void DeviceCard::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background with border
    QRectF rect = this->rect();
    rect.adjust(1, 1, -1, -1);
    
    QPainterPath path;
    path.addRoundedRect(rect, FSStyle.borderRadius(), FSStyle.borderRadius());
    
    // Background
    QColor bgColor = FSColor(Surface);
    if (m_hoverProgress > 0) {
        QColor hoverColor = FSColor(SurfaceHover);
        bgColor = QColor(
            bgColor.red() + (hoverColor.red() - bgColor.red()) * m_hoverProgress,
            bgColor.green() + (hoverColor.green() - bgColor.green()) * m_hoverProgress,
            bgColor.blue() + (hoverColor.blue() - bgColor.blue()) * m_hoverProgress
        );
    }
    painter.fillPath(path, bgColor);
    
    // Border
    QColor borderColor = FSColor(Border);
    if (m_hoverProgress > 0) {
        QColor activeColor = FSColor(BorderActive);
        borderColor = QColor(
            borderColor.red() + (activeColor.red() - borderColor.red()) * m_hoverProgress,
            borderColor.green() + (activeColor.green() - borderColor.green()) * m_hoverProgress,
            borderColor.blue() + (activeColor.blue() - borderColor.blue()) * m_hoverProgress,
            borderColor.alpha() + (activeColor.alpha() - borderColor.alpha()) * m_hoverProgress
        );
    }
    
    QPen borderPen(borderColor, 1);
    painter.setPen(borderPen);
    painter.drawPath(path);
    
    // Status accent line on left
    if (m_status != VerificationStatus::Unknown) {
        QRectF accentRect(0, 6, 3, rect.height() - 12);
        QPainterPath accentPath;
        accentPath.addRoundedRect(accentRect, 1.5, 1.5);
        painter.fillPath(accentPath, statusColor());
    }
    
    QFrame::paintEvent(event);
}

void DeviceCard::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
}

void DeviceCard::onMountClicked()
{
    emit mountRequested(m_device.deviceNode);
}

void DeviceCard::onUnmountClicked()
{
    emit unmountRequested(m_device.deviceNode);
}

void DeviceCard::onEjectClicked()
{
    emit ejectRequested(m_device.deviceNode);
}

void DeviceCard::onRehashClicked()
{
    emit rehashRequested(m_device.deviceNode);
}

void DeviceCard::updatePulse()
{
    m_pulsePhase = (m_pulsePhase + 1) % 100;
    
    // Sine wave for smooth pulsing
    double phase = m_pulsePhase / 100.0 * 2 * M_PI;
    double intensity = (sin(phase) + 1) / 2;  // 0 to 1
    
    m_glowEffect->setBlurRadius(5 + intensity * 15);
    m_glowEffect->setColor(statusColor());
    
    // Also pulse the status indicator opacity
    if (m_statusIndicator) {
        int alpha = static_cast<int>(155 + intensity * 100);
        QColor color = statusColor();
        color.setAlpha(alpha);
        m_statusIndicator->setStyleSheet(QString(R"(
            QLabel#StatusIndicator {
                background-color: %1;
                border-radius: 6px;
                min-width: 12px;
                min-height: 12px;
            }
        )").arg(color.name(QColor::HexArgb)));
    }
}

void DeviceCard::updateDisplay()
{
    // Update name
    m_nameLabel->setText(m_device.displayName());
    
    // Update device path
    m_devicePathLabel->setText(m_device.deviceNode);
    
    // Update info fields
    m_sizeLabel->setText(formatSize(m_device.sizeBytes));
    m_fsTypeLabel->setText(m_device.fsType.isEmpty() ? "Unknown" : m_device.fsType.toUpper());
    m_mountPointLabel->setText(m_device.mountPoint.isEmpty() ? "Not mounted" : m_device.mountPoint);
    m_serialLabel->setText(m_device.serial.isEmpty() ? "N/A" : m_device.serial);
    
    // Update icon based on filesystem type
    QString icon = "💾";
    if (m_device.fsType.contains("ntfs", Qt::CaseInsensitive)) {
        icon = "🪟";  // Windows
    } else if (m_device.fsType.contains("ext", Qt::CaseInsensitive)) {
        icon = "🐧";  // Linux
    } else if (m_device.fsType.contains("fat", Qt::CaseInsensitive) ||
               m_device.fsType.contains("exfat", Qt::CaseInsensitive)) {
        icon = "📁";  // Generic
    }
    m_iconLabel->setText(icon);
    
    updateStatusIndicator();
    updateActionButtons();
}

void DeviceCard::updateStatusIndicator()
{
    QColor color = statusColor();
    QString statusText = verificationStatusToString(m_status);
    
    m_statusIndicator->setStyleSheet(FSStyle.statusIndicatorStyleSheet(
        [this]() {
            switch (m_status) {
                case VerificationStatus::Verified:  return StyleManager::ColorRole::Verified;
                case VerificationStatus::Modified:  return StyleManager::ColorRole::Modified;
                case VerificationStatus::NewDevice: return StyleManager::ColorRole::Unknown;
                case VerificationStatus::Hashing:   return StyleManager::ColorRole::Hashing;
                case VerificationStatus::Error:     return StyleManager::ColorRole::Error;
                default:                            return StyleManager::ColorRole::TextMuted;
            }
        }()
    ));
    
    m_statusLabel->setText(statusText);
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(color.name()));
}

void DeviceCard::updateActionButtons()
{
    bool isMounted = m_device.isMounted;
    
    m_mountBtn->setVisible(!isMounted);
    m_unmountBtn->setVisible(isMounted);
    m_openBtn->setVisible(isMounted);
    m_openBtn->setEnabled(isMounted && !m_device.mountPoint.isEmpty());
    
    // Disable rehash while hashing
    m_rehashBtn->setEnabled(m_status != VerificationStatus::Hashing);
    
    // Update rehash button appearance based on status
    if (m_status == VerificationStatus::Modified) {
        m_rehashBtn->setText("↻ Verify");
        m_rehashBtn->setStyleSheet(FSStyle.dangerButtonStyleSheet());
    } else {
        m_rehashBtn->setText("↻ Rehash");
        m_rehashBtn->setStyleSheet(FSStyle.primaryButtonStyleSheet());
    }
}

QColor DeviceCard::statusColor() const
{
    switch (m_status) {
        case VerificationStatus::Verified:
            return FSColor(Verified);
        case VerificationStatus::Modified:
            return FSColor(Modified);
        case VerificationStatus::NewDevice:
            return FSColor(Unknown);
        case VerificationStatus::Hashing:
            return FSColor(Hashing);
        case VerificationStatus::Error:
            return FSColor(Error);
        case VerificationStatus::Pending:
        case VerificationStatus::Unknown:
        default:
            return FSColor(TextMuted);
    }
}

QString DeviceCard::formatSize(uint64_t bytes) const
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }
    
    if (unitIndex == 0) {
        return QString("%1 B").arg(bytes);
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIndex]);
}

void DeviceCard::applyGlowEffect(QWidget* widget, const QColor& color, int blur)
{
    auto* effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(blur);
    effect->setColor(color);
    effect->setOffset(0, 0);
    widget->setGraphicsEffect(effect);
}

} // namespace FlashSentry