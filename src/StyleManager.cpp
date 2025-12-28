#include "StyleManager.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QDebug>

namespace FlashSentry {

// Static theme palettes
const QHash<StyleManager::Theme, QHash<StyleManager::ColorRole, QColor>> StyleManager::s_themePalettes = {
    // CyberDark - Default dark theme with cyan accents
    {Theme::CyberDark, {
        {ColorRole::Background,      QColor(0x0D, 0x0D, 0x0F)},
        {ColorRole::BackgroundAlt,   QColor(0x14, 0x14, 0x18)},
        {ColorRole::BackgroundDark,  QColor(0x08, 0x08, 0x0A)},
        {ColorRole::Surface,         QColor(0x1A, 0x1A, 0x22)},
        {ColorRole::SurfaceHover,    QColor(0x24, 0x24, 0x2E)},
        
        {ColorRole::TextPrimary,     QColor(0xE8, 0xEA, 0xED)},
        {ColorRole::TextSecondary,   QColor(0xA8, 0xAE, 0xB8)},
        {ColorRole::TextMuted,       QColor(0x68, 0x6E, 0x78)},
        {ColorRole::TextDisabled,    QColor(0x48, 0x4E, 0x58)},
        
        {ColorRole::AccentPrimary,   QColor(0x00, 0xD4, 0xFF)},  // Cyan
        {ColorRole::AccentSecondary, QColor(0x00, 0x8B, 0xB8)},
        {ColorRole::AccentGlow,      QColor(0x00, 0xD4, 0xFF, 0x40)},
        
        {ColorRole::Success,         QColor(0x00, 0xE6, 0x76)},
        {ColorRole::Warning,         QColor(0xFF, 0xB8, 0x00)},
        {ColorRole::Error,           QColor(0xFF, 0x3D, 0x5A)},
        {ColorRole::Info,            QColor(0x00, 0xD4, 0xFF)},
        
        {ColorRole::Verified,        QColor(0x00, 0xE6, 0x76)},
        {ColorRole::Modified,        QColor(0xFF, 0x3D, 0x5A)},
        {ColorRole::Unknown,         QColor(0xFF, 0xB8, 0x00)},
        {ColorRole::Hashing,         QColor(0x00, 0xD4, 0xFF)},
        
        {ColorRole::Border,          QColor(0x2A, 0x2A, 0x35)},
        {ColorRole::BorderActive,    QColor(0x00, 0xD4, 0xFF)},
        {ColorRole::BorderGlow,      QColor(0x00, 0xD4, 0xFF, 0x60)},
        
        {ColorRole::GlowPrimary,     QColor(0x00, 0xD4, 0xFF, 0x30)},
        {ColorRole::GlowSecondary,   QColor(0x00, 0x8B, 0xB8, 0x20)},
        {ColorRole::ShadowColor,     QColor(0x00, 0x00, 0x00, 0x80)}
    }},
    
    // NeonPurple - Purple/magenta neon theme
    {Theme::NeonPurple, {
        {ColorRole::Background,      QColor(0x0E, 0x08, 0x14)},
        {ColorRole::BackgroundAlt,   QColor(0x16, 0x0E, 0x1E)},
        {ColorRole::BackgroundDark,  QColor(0x0A, 0x05, 0x0E)},
        {ColorRole::Surface,         QColor(0x1E, 0x14, 0x28)},
        {ColorRole::SurfaceHover,    QColor(0x28, 0x1E, 0x35)},
        
        {ColorRole::TextPrimary,     QColor(0xF0, 0xE8, 0xF8)},
        {ColorRole::TextSecondary,   QColor(0xB8, 0xA8, 0xC8)},
        {ColorRole::TextMuted,       QColor(0x78, 0x68, 0x88)},
        {ColorRole::TextDisabled,    QColor(0x58, 0x48, 0x68)},
        
        {ColorRole::AccentPrimary,   QColor(0xE040, 0x00, 0xFF)},  // Magenta
        {ColorRole::AccentSecondary, QColor(0xA0, 0x00, 0xC8)},
        {ColorRole::AccentGlow,      QColor(0xE0, 0x00, 0xFF, 0x40)},
        
        {ColorRole::Success,         QColor(0x00, 0xFF, 0x88)},
        {ColorRole::Warning,         QColor(0xFF, 0xA0, 0x40)},
        {ColorRole::Error,           QColor(0xFF, 0x40, 0x70)},
        {ColorRole::Info,            QColor(0xE0, 0x00, 0xFF)},
        
        {ColorRole::Verified,        QColor(0x00, 0xFF, 0x88)},
        {ColorRole::Modified,        QColor(0xFF, 0x40, 0x70)},
        {ColorRole::Unknown,         QColor(0xFF, 0xA0, 0x40)},
        {ColorRole::Hashing,         QColor(0xE0, 0x00, 0xFF)},
        
        {ColorRole::Border,          QColor(0x35, 0x28, 0x45)},
        {ColorRole::BorderActive,    QColor(0xE0, 0x00, 0xFF)},
        {ColorRole::BorderGlow,      QColor(0xE0, 0x00, 0xFF, 0x60)},
        
        {ColorRole::GlowPrimary,     QColor(0xE0, 0x00, 0xFF, 0x30)},
        {ColorRole::GlowSecondary,   QColor(0xA0, 0x00, 0xC8, 0x20)},
        {ColorRole::ShadowColor,     QColor(0x00, 0x00, 0x00, 0x80)}
    }},
    
    // MatrixGreen - Classic green-on-black
    {Theme::MatrixGreen, {
        {ColorRole::Background,      QColor(0x0A, 0x0F, 0x0A)},
        {ColorRole::BackgroundAlt,   QColor(0x10, 0x18, 0x10)},
        {ColorRole::BackgroundDark,  QColor(0x05, 0x08, 0x05)},
        {ColorRole::Surface,         QColor(0x14, 0x20, 0x14)},
        {ColorRole::SurfaceHover,    QColor(0x1A, 0x28, 0x1A)},
        
        {ColorRole::TextPrimary,     QColor(0x00, 0xFF, 0x00)},
        {ColorRole::TextSecondary,   QColor(0x00, 0xC0, 0x00)},
        {ColorRole::TextMuted,       QColor(0x00, 0x80, 0x00)},
        {ColorRole::TextDisabled,    QColor(0x00, 0x50, 0x00)},
        
        {ColorRole::AccentPrimary,   QColor(0x00, 0xFF, 0x00)},
        {ColorRole::AccentSecondary, QColor(0x00, 0xB0, 0x00)},
        {ColorRole::AccentGlow,      QColor(0x00, 0xFF, 0x00, 0x40)},
        
        {ColorRole::Success,         QColor(0x00, 0xFF, 0x00)},
        {ColorRole::Warning,         QColor(0xFF, 0xFF, 0x00)},
        {ColorRole::Error,           QColor(0xFF, 0x00, 0x00)},
        {ColorRole::Info,            QColor(0x00, 0xFF, 0x00)},
        
        {ColorRole::Verified,        QColor(0x00, 0xFF, 0x00)},
        {ColorRole::Modified,        QColor(0xFF, 0x00, 0x00)},
        {ColorRole::Unknown,         QColor(0xFF, 0xFF, 0x00)},
        {ColorRole::Hashing,         QColor(0x00, 0xFF, 0x80)},
        
        {ColorRole::Border,          QColor(0x00, 0x40, 0x00)},
        {ColorRole::BorderActive,    QColor(0x00, 0xFF, 0x00)},
        {ColorRole::BorderGlow,      QColor(0x00, 0xFF, 0x00, 0x60)},
        
        {ColorRole::GlowPrimary,     QColor(0x00, 0xFF, 0x00, 0x30)},
        {ColorRole::GlowSecondary,   QColor(0x00, 0xB0, 0x00, 0x20)},
        {ColorRole::ShadowColor,     QColor(0x00, 0x00, 0x00, 0x80)}
    }},
    
    // BladeRunner - Orange/amber tones
    {Theme::BladeRunner, {
        {ColorRole::Background,      QColor(0x12, 0x0C, 0x08)},
        {ColorRole::BackgroundAlt,   QColor(0x1A, 0x12, 0x0C)},
        {ColorRole::BackgroundDark,  QColor(0x0A, 0x06, 0x04)},
        {ColorRole::Surface,         QColor(0x22, 0x18, 0x10)},
        {ColorRole::SurfaceHover,    QColor(0x2C, 0x20, 0x16)},
        
        {ColorRole::TextPrimary,     QColor(0xFF, 0xE8, 0xD0)},
        {ColorRole::TextSecondary,   QColor(0xD0, 0xB8, 0x98)},
        {ColorRole::TextMuted,       QColor(0x90, 0x78, 0x58)},
        {ColorRole::TextDisabled,    QColor(0x60, 0x50, 0x40)},
        
        {ColorRole::AccentPrimary,   QColor(0xFF, 0x80, 0x00)},
        {ColorRole::AccentSecondary, QColor(0xD0, 0x60, 0x00)},
        {ColorRole::AccentGlow,      QColor(0xFF, 0x80, 0x00, 0x40)},
        
        {ColorRole::Success,         QColor(0x80, 0xFF, 0x00)},
        {ColorRole::Warning,         QColor(0xFF, 0xD0, 0x00)},
        {ColorRole::Error,           QColor(0xFF, 0x20, 0x20)},
        {ColorRole::Info,            QColor(0xFF, 0x80, 0x00)},
        
        {ColorRole::Verified,        QColor(0x80, 0xFF, 0x00)},
        {ColorRole::Modified,        QColor(0xFF, 0x20, 0x20)},
        {ColorRole::Unknown,         QColor(0xFF, 0xD0, 0x00)},
        {ColorRole::Hashing,         QColor(0xFF, 0xA0, 0x40)},
        
        {ColorRole::Border,          QColor(0x40, 0x30, 0x20)},
        {ColorRole::BorderActive,    QColor(0xFF, 0x80, 0x00)},
        {ColorRole::BorderGlow,      QColor(0xFF, 0x80, 0x00, 0x60)},
        
        {ColorRole::GlowPrimary,     QColor(0xFF, 0x80, 0x00, 0x30)},
        {ColorRole::GlowSecondary,   QColor(0xD0, 0x60, 0x00, 0x20)},
        {ColorRole::ShadowColor,     QColor(0x00, 0x00, 0x00, 0x80)}
    }},
    
    // GhostWhite - Light theme with blue accents
    {Theme::GhostWhite, {
        {ColorRole::Background,      QColor(0xF8, 0xFA, 0xFC)},
        {ColorRole::BackgroundAlt,   QColor(0xF0, 0xF4, 0xF8)},
        {ColorRole::BackgroundDark,  QColor(0xE8, 0xEC, 0xF0)},
        {ColorRole::Surface,         QColor(0xFF, 0xFF, 0xFF)},
        {ColorRole::SurfaceHover,    QColor(0xF0, 0xF4, 0xF8)},
        
        {ColorRole::TextPrimary,     QColor(0x1A, 0x1A, 0x2E)},
        {ColorRole::TextSecondary,   QColor(0x4A, 0x4A, 0x5E)},
        {ColorRole::TextMuted,       QColor(0x8A, 0x8A, 0x9E)},
        {ColorRole::TextDisabled,    QColor(0xBA, 0xBA, 0xCE)},
        
        {ColorRole::AccentPrimary,   QColor(0x00, 0x6A, 0xFF)},
        {ColorRole::AccentSecondary, QColor(0x00, 0x50, 0xC8)},
        {ColorRole::AccentGlow,      QColor(0x00, 0x6A, 0xFF, 0x20)},
        
        {ColorRole::Success,         QColor(0x00, 0xA8, 0x60)},
        {ColorRole::Warning,         QColor(0xE0, 0x90, 0x00)},
        {ColorRole::Error,           QColor(0xE0, 0x30, 0x50)},
        {ColorRole::Info,            QColor(0x00, 0x6A, 0xFF)},
        
        {ColorRole::Verified,        QColor(0x00, 0xA8, 0x60)},
        {ColorRole::Modified,        QColor(0xE0, 0x30, 0x50)},
        {ColorRole::Unknown,         QColor(0xE0, 0x90, 0x00)},
        {ColorRole::Hashing,         QColor(0x00, 0x6A, 0xFF)},
        
        {ColorRole::Border,          QColor(0xD0, 0xD4, 0xD8)},
        {ColorRole::BorderActive,    QColor(0x00, 0x6A, 0xFF)},
        {ColorRole::BorderGlow,      QColor(0x00, 0x6A, 0xFF, 0x40)},
        
        {ColorRole::GlowPrimary,     QColor(0x00, 0x6A, 0xFF, 0x15)},
        {ColorRole::GlowSecondary,   QColor(0x00, 0x50, 0xC8, 0x10)},
        {ColorRole::ShadowColor,     QColor(0x00, 0x00, 0x00, 0x20)}
    }}
};

StyleManager& StyleManager::instance()
{
    static StyleManager instance;
    return instance;
}

StyleManager::StyleManager()
{
    initialize();
}

void StyleManager::initialize()
{
    loadTheme(Theme::CyberDark);
    
    // Setup default fonts
    m_fonts[FontRole::Default] = QFont("Segoe UI", m_baseFontSize);
    m_fonts[FontRole::Heading1] = QFont("Segoe UI", m_baseFontSize + 8, QFont::Bold);
    m_fonts[FontRole::Heading2] = QFont("Segoe UI", m_baseFontSize + 4, QFont::DemiBold);
    m_fonts[FontRole::Heading3] = QFont("Segoe UI", m_baseFontSize + 2, QFont::DemiBold);
    m_fonts[FontRole::Monospace] = QFont("JetBrains Mono", m_baseFontSize);
    m_fonts[FontRole::Small] = QFont("Segoe UI", m_baseFontSize - 2);
    m_fonts[FontRole::Button] = QFont("Segoe UI", m_baseFontSize, QFont::Medium);
    m_fonts[FontRole::Label] = QFont("Segoe UI", m_baseFontSize - 1);
    
    // Try alternative fonts if primary not available
    if (!m_fonts[FontRole::Monospace].exactMatch()) {
        m_fonts[FontRole::Monospace] = QFont("Consolas", m_baseFontSize);
        if (!m_fonts[FontRole::Monospace].exactMatch()) {
            m_fonts[FontRole::Monospace] = QFont("monospace", m_baseFontSize);
        }
    }
    
    generateStyleSheet();
}

void StyleManager::setTheme(Theme theme)
{
    if (m_currentTheme == theme) return;
    
    loadTheme(theme);
    generateStyleSheet();
    applyToApplication();
    
    emit themeChanged(theme);
}

QString StyleManager::themeName(Theme theme) const
{
    switch (theme) {
        case Theme::CyberDark:   return "Cyber Dark";
        case Theme::NeonPurple:  return "Neon Purple";
        case Theme::MatrixGreen: return "Matrix Green";
        case Theme::BladeRunner: return "Blade Runner";
        case Theme::GhostWhite:  return "Ghost White";
    }
    return "Unknown";
}

QList<StyleManager::Theme> StyleManager::availableThemes() const
{
    return {
        Theme::CyberDark,
        Theme::NeonPurple,
        Theme::MatrixGreen,
        Theme::BladeRunner,
        Theme::GhostWhite
    };
}

QColor StyleManager::color(ColorRole role) const
{
    return m_colors.value(role, QColor(0xFF, 0x00, 0xFF));  // Magenta for missing
}

QString StyleManager::colorCss(ColorRole role) const
{
    QColor c = color(role);
    if (c.alpha() == 255) {
        return QString("rgb(%1, %2, %3)").arg(c.red()).arg(c.green()).arg(c.blue());
    }
    return QString("rgba(%1, %2, %3, %4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(c.alpha() / 255.0, 0, 'f', 2);
}

QColor StyleManager::colorWithAlpha(ColorRole role, int alpha) const
{
    QColor c = color(role);
    c.setAlpha(alpha);
    return c;
}

QFont StyleManager::font(FontRole role) const
{
    return m_fonts.value(role, QFont());
}

void StyleManager::setBaseFontSize(int size)
{
    m_baseFontSize = qBound(8, size, 24);
    initialize();  // Regenerate fonts
}

QString StyleManager::applicationStyleSheet() const
{
    return m_cachedStyleSheet;
}

QString StyleManager::mainWindowStyleSheet() const
{
    return QString(R"(
        QMainWindow {
            background-color: %1;
        }
        QMainWindow::separator {
            background: %2;
            width: 1px;
            height: 1px;
        }
    )")
    .arg(colorCss(ColorRole::Background))
    .arg(colorCss(ColorRole::Border));
}

QString StyleManager::deviceCardStyleSheet() const
{
    return QString(R"(
        QFrame#DeviceCard {
            background-color: %1;
            border: 1px solid %2;
            border-radius: %5px;
            padding: 12px;
        }
        QFrame#DeviceCard:hover {
            background-color: %3;
            border-color: %4;
        }
    )")
    .arg(colorCss(ColorRole::Surface))
    .arg(colorCss(ColorRole::Border))
    .arg(colorCss(ColorRole::SurfaceHover))
    .arg(colorCss(ColorRole::BorderActive))
    .arg(m_borderRadius);
}

QString StyleManager::buttonStyleSheet() const
{
    return QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: %7px;
            padding: 8px 16px;
            font-weight: 500;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: %4;
            border-color: %5;
        }
        QPushButton:pressed {
            background-color: %6;
        }
        QPushButton:disabled {
            background-color: %1;
            color: %8;
            border-color: %3;
        }
    )")
    .arg(colorCss(ColorRole::Surface))
    .arg(colorCss(ColorRole::TextPrimary))
    .arg(colorCss(ColorRole::Border))
    .arg(colorCss(ColorRole::SurfaceHover))
    .arg(colorCss(ColorRole::BorderActive))
    .arg(colorCss(ColorRole::BackgroundDark))
    .arg(m_borderRadius)
    .arg(colorCss(ColorRole::TextDisabled));
}

QString StyleManager::primaryButtonStyleSheet() const
{
    return QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: %6px;
            padding: 10px 20px;
            font-weight: 600;
            min-height: 22px;
        }
        QPushButton:hover {
            background-color: %3;
        }
        QPushButton:pressed {
            background-color: %4;
        }
        QPushButton:disabled {
            background-color: %5;
            color: %7;
        }
    )")
    .arg(colorCss(ColorRole::AccentPrimary))
    .arg(colorCss(ColorRole::BackgroundDark))
    .arg(colorCss(ColorRole::AccentSecondary))
    .arg(colorCss(ColorRole::AccentSecondary))
    .arg(colorCss(ColorRole::Surface))
    .arg(m_borderRadius)
    .arg(colorCss(ColorRole::TextDisabled));
}

QString StyleManager::dangerButtonStyleSheet() const
{
    return QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: 1px solid %1;
            border-radius: %4px;
            padding: 8px 16px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: %2;
            color: %3;
        }
        QPushButton:pressed {
            background-color: %1;
        }
    )")
    .arg(colorCss(ColorRole::Error))
    .arg(colorCss(ColorRole::Error))
    .arg(colorCss(ColorRole::TextPrimary))
    .arg(m_borderRadius);
}

QString StyleManager::listWidgetStyleSheet() const
{
    return QString(R"(
        QListWidget {
            background-color: %1;
            border: 1px solid %2;
            border-radius: %6px;
            padding: 4px;
            outline: none;
        }
        QListWidget::item {
            background-color: transparent;
            color: %3;
            padding: 8px 12px;
            border-radius: 4px;
            margin: 2px 0;
        }
        QListWidget::item:hover {
            background-color: %4;
        }
        QListWidget::item:selected {
            background-color: %5;
            color: %3;
        }
    )")
    .arg(colorCss(ColorRole::BackgroundAlt))
    .arg(colorCss(ColorRole::Border))
    .arg(colorCss(ColorRole::TextPrimary))
    .arg(colorCss(ColorRole::SurfaceHover))
    .arg(colorCss(ColorRole::AccentPrimary) + "40")  // 25% opacity
    .arg(m_borderRadius);
}

QString StyleManager::progressBarStyleSheet() const
{
    return QString(R"(
        QProgressBar {
            background-color: %1;
            border: none;
            border-radius: 4px;
            height: 8px;
            text-align: center;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %2, stop:1 %3);
            border-radius: 4px;
        }
    )")
    .arg(colorCss(ColorRole::BackgroundDark))
    .arg(colorCss(ColorRole::AccentSecondary))
    .arg(colorCss(ColorRole::AccentPrimary));
}

QString StyleManager::inputFieldStyleSheet() const
{
    return QString(R"(
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: %6px;
            padding: 8px 12px;
            selection-background-color: %5;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border-color: %4;
        }
        QLineEdit:disabled, QTextEdit:disabled {
            background-color: %7;
            color: %8;
        }
    )")
    .arg(colorCss(ColorRole::BackgroundDark))
    .arg(colorCss(ColorRole::TextPrimary))
    .arg(colorCss(ColorRole::Border))
    .arg(colorCss(ColorRole::AccentPrimary))
    .arg(colorCss(ColorRole::AccentPrimary) + "40")
    .arg(m_borderRadius)
    .arg(colorCss(ColorRole::Surface))
    .arg(colorCss(ColorRole::TextDisabled));
}

QString StyleManager::labelStyleSheet() const
{
    return QString(R"(
        QLabel {
            color: %1;
            background: transparent;
        }
        QLabel#heading {
            color: %2;
            font-weight: 600;
        }
        QLabel#muted {
            color: %3;
        }
    )")
    .arg(colorCss(ColorRole::TextPrimary))
    .arg(colorCss(ColorRole::AccentPrimary))
    .arg(colorCss(ColorRole::TextMuted));
}

QString StyleManager::scrollAreaStyleSheet() const
{
    return QString(R"(
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background-color: %1;
            width: 10px;
            margin: 0;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background-color: %2;
            min-height: 30px;
            border-radius: 5px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: %3;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background-color: %1;
            height: 10px;
            margin: 0;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal {
            background-color: %2;
            min-width: 30px;
            border-radius: 5px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: %3;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
    )")
    .arg(colorCss(ColorRole::BackgroundDark))
    .arg(colorCss(ColorRole::Border))
    .arg(colorCss(ColorRole::AccentSecondary));
}

QString StyleManager::tooltipStyleSheet() const
{
    return QString(R"(
        QToolTip {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 6px 10px;
        }
    )")
    .arg(colorCss(ColorRole::Surface))
    .arg(colorCss(ColorRole::TextPrimary))
    .arg(colorCss(ColorRole::Border));
}

QString StyleManager::dialogStyleSheet() const
{
    return QString(R"(
        QDialog {
            background-color: %1;
        }
        QDialogButtonBox QPushButton {
            min-width: 80px;
        }
    )")
    .arg(colorCss(ColorRole::Background));
}

QString StyleManager::menuStyleSheet() const
{
    return QString(R"(
        QMenu {
            background-color: %1;
            border: 1px solid %2;
            border-radius: %6px;
            padding: 4px;
        }
        QMenu::item {
            color: %3;
            padding: 8px 24px 8px 12px;
            border-radius: 4px;
            margin: 2px 4px;
        }
        QMenu::item:selected {
            background-color: %4;
        }
        QMenu::separator {
            height: 1px;
            background-color: %2;
            margin: 4px 8px;
        }
        QMenu::icon {
            margin-left: 8px;
        }
        QMenuBar {
            background-color: %1;
            color: %3;
            border-bottom: 1px solid %2;
        }
        QMenuBar::item {
            padding: 8px 12px;
            background: transparent;
        }
        QMenuBar::item:selected {
            background-color: %4;
        }
    )")
    .arg(colorCss(ColorRole::Surface))
    .arg(colorCss(ColorRole::Border))
    .arg(colorCss(ColorRole::TextPrimary))
    .arg(colorCss(ColorRole::SurfaceHover))
    .arg(colorCss(ColorRole::AccentPrimary))
    .arg(m_borderRadius);
}

QString StyleManager::tabWidgetStyleSheet() const
{
    return QString(R"(
        QTabWidget::pane {
            background-color: %1;
            border: 1px solid %2;
            border-radius: %6px;
            top: -1px;
        }
        QTabBar::tab {
            background-color: %3;
            color: %4;
            border: 1px solid %2;
            border-bottom: none;
            border-top-left-radius: %6px;
            border-top-right-radius: %6px;
            padding: 10px 20px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: %1;
            color: %5;
            border-bottom: 2px solid %5;
        }
        QTabBar::tab:hover:!selected {
            background-color: %7;
        }
    )")
    .arg(colorCss(ColorRole::Surface))
    .arg(colorCss(ColorRole::Border))
    .arg(colorCss(ColorRole::BackgroundAlt))
    .arg(colorCss(ColorRole::TextSecondary))
    .arg(colorCss(ColorRole::AccentPrimary))
    .arg(m_borderRadius)
    .arg(colorCss(ColorRole::SurfaceHover));
}

QString StyleManager::statusIndicatorStyleSheet(ColorRole statusColor) const
{
    return QString(R"(
        QLabel#StatusIndicator {
            background-color: %1;
            border-radius: 6px;
            min-width: 12px;
            min-height: 12px;
            max-width: 12px;
            max-height: 12px;
        }
    )")
    .arg(colorCss(statusColor));
}

std::unique_ptr<QPropertyAnimation> StyleManager::createGlowAnimation(
    QWidget* target, 
    const QColor& glowColor,
    int duration) const
{
    if (!target || !m_animationsEnabled) {
        return nullptr;
    }
    
    auto* effect = new QGraphicsDropShadowEffect(target);
    effect->setBlurRadius(0);
    effect->setColor(glowColor);
    effect->setOffset(0, 0);
    target->setGraphicsEffect(effect);
    
    auto animation = std::make_unique<QPropertyAnimation>(effect, "blurRadius");
    animation->setDuration(duration);
    animation->setStartValue(0);
    animation->setKeyValueAt(0.5, 20);
    animation->setEndValue(0);
    animation->setLoopCount(-1);  // Infinite loop
    
    return animation;
}

void StyleManager::applyFadeIn(QWidget* widget, int duration) const
{
    if (!widget || !m_animationsEnabled) {
        return;
    }
    
    auto* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    
    auto* animation = new QPropertyAnimation(effect, "opacity", widget);
    animation->setDuration(duration);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

void StyleManager::applyPulse(QWidget* widget, const QColor& color, int duration) const
{
    if (!widget || !m_animationsEnabled) {
        return;
    }
    
    auto* effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(0);
    effect->setColor(color);
    effect->setOffset(0, 0);
    widget->setGraphicsEffect(effect);
    
    auto* animation = new QPropertyAnimation(effect, "blurRadius", widget);
    animation->setDuration(duration);
    animation->setStartValue(0);
    animation->setKeyValueAt(0.5, 15);
    animation->setEndValue(0);
    animation->setLoopCount(3);
    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

void StyleManager::applyToApplication()
{
    if (auto* app = qApp) {
        app->setStyleSheet(m_cachedStyleSheet);
        
        // Update application palette
        QPalette palette;
        palette.setColor(QPalette::Window, color(ColorRole::Background));
        palette.setColor(QPalette::WindowText, color(ColorRole::TextPrimary));
        palette.setColor(QPalette::Base, color(ColorRole::BackgroundAlt));
        palette.setColor(QPalette::AlternateBase, color(ColorRole::Surface));
        palette.setColor(QPalette::ToolTipBase, color(ColorRole::Surface));
        palette.setColor(QPalette::ToolTipText, color(ColorRole::TextPrimary));
        palette.setColor(QPalette::Text, color(ColorRole::TextPrimary));
        palette.setColor(QPalette::Button, color(ColorRole::Surface));
        palette.setColor(QPalette::ButtonText, color(ColorRole::TextPrimary));
        palette.setColor(QPalette::BrightText, color(ColorRole::AccentPrimary));
        palette.setColor(QPalette::Highlight, color(ColorRole::AccentPrimary));
        palette.setColor(QPalette::HighlightedText, color(ColorRole::BackgroundDark));
        palette.setColor(QPalette::Disabled, QPalette::Text, color(ColorRole::TextDisabled));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, color(ColorRole::TextDisabled));
        
        app->setPalette(palette);
    }
}

QString StyleManager::boxShadowCss(int blur, int spread) const
{
    QColor shadow = color(ColorRole::ShadowColor);
    return QString("box-shadow: 0 4px %1px %2px rgba(%3, %4, %5, %6);")
        .arg(blur).arg(spread)
        .arg(shadow.red()).arg(shadow.green()).arg(shadow.blue())
        .arg(shadow.alpha() / 255.0, 0, 'f', 2);
}

QString StyleManager::glowCss(ColorRole colorRole, int blur) const
{
    QColor glow = color(colorRole);
    return QString("box-shadow: 0 0 %1px %2px rgba(%3, %4, %5, 0.5);")
        .arg(blur).arg(blur / 3)
        .arg(glow.red()).arg(glow.green()).arg(glow.blue());
}

void StyleManager::loadTheme(Theme theme)
{
    m_currentTheme = theme;
    
    auto it = s_themePalettes.find(theme);
    if (it != s_themePalettes.end()) {
        m_colors = *it;
    } else {
        // Fallback to CyberDark
        m_colors = s_themePalettes.value(Theme::CyberDark);
    }
}

void StyleManager::generateStyleSheet()
{
    m_cachedStyleSheet = QString(R"(
        * {
            font-family: "Segoe UI", "SF Pro Display", -apple-system, sans-serif;
            font-size: %1px;
        }
        
        QWidget {
            background-color: %2;
            color: %3;
        }
        
        %4
        %5
        %6
        %7
        %8
        %9
        %10
        %11
        %12
    )")
    .arg(m_baseFontSize)
    .arg(colorCss(ColorRole::Background))
    .arg(colorCss(ColorRole::TextPrimary))
    .arg(buttonStyleSheet())
    .arg(listWidgetStyleSheet())
    .arg(progressBarStyleSheet())
    .arg(inputFieldStyleSheet())
    .arg(scrollAreaStyleSheet())
    .arg(tooltipStyleSheet())
    .arg(menuStyleSheet())
    .arg(tabWidgetStyleSheet())
    .arg(labelStyleSheet());
}

} // namespace FlashSentry