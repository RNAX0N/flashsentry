#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QPalette>
#include <QPropertyAnimation>
#include <QWidget>
#include <memory>

namespace FlashSentry {

/**
 * @brief StyleManager - Centralized theming and styling for the application
 * 
 * Provides a futuristic, cyberpunk-inspired dark theme with neon accents.
 * Supports dynamic theme switching and consistent styling across all widgets.
 */
class StyleManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Available color themes
     */
    enum class Theme {
        CyberDark,      // Default dark theme with cyan accents
        NeonPurple,     // Purple/magenta neon theme
        MatrixGreen,    // Classic green-on-black
        BladeRunner,    // Orange/amber tones
        GhostWhite      // Light theme with blue accents
    };
    Q_ENUM(Theme)

    /**
     * @brief Color roles in the theme
     */
    enum class ColorRole {
        // Base colors
        Background,
        BackgroundAlt,
        BackgroundDark,
        Surface,
        SurfaceHover,
        
        // Text colors
        TextPrimary,
        TextSecondary,
        TextMuted,
        TextDisabled,
        
        // Accent colors
        AccentPrimary,
        AccentSecondary,
        AccentGlow,
        
        // Status colors
        Success,
        Warning,
        Error,
        Info,
        
        // Security-specific
        Verified,
        Modified,
        Unknown,
        Hashing,
        
        // Border colors
        Border,
        BorderActive,
        BorderGlow,
        
        // Special effects
        GlowPrimary,
        GlowSecondary,
        ShadowColor
    };

    /**
     * @brief Font roles
     */
    enum class FontRole {
        Default,
        Heading1,
        Heading2,
        Heading3,
        Monospace,
        Small,
        Button,
        Label
    };

    /**
     * @brief Get singleton instance
     */
    static StyleManager& instance();

    // Prevent copying
    StyleManager(const StyleManager&) = delete;
    StyleManager& operator=(const StyleManager&) = delete;

    /**
     * @brief Initialize the style manager with default theme
     */
    void initialize();

    /**
     * @brief Set the current theme
     * @param theme Theme to apply
     */
    void setTheme(Theme theme);

    /**
     * @brief Get the current theme
     */
    Theme currentTheme() const { return m_currentTheme; }

    /**
     * @brief Get theme name as string
     */
    QString themeName(Theme theme) const;

    /**
     * @brief Get list of available themes
     */
    QList<Theme> availableThemes() const;

    // ========================================================================
    // Color Access
    // ========================================================================

    /**
     * @brief Get a color for the current theme
     * @param role Color role
     * @return Color for the role
     */
    QColor color(ColorRole role) const;

    /**
     * @brief Get a color as CSS string
     */
    QString colorCss(ColorRole role) const;

    /**
     * @brief Get a color with alpha applied
     */
    QColor colorWithAlpha(ColorRole role, int alpha) const;

    // ========================================================================
    // Font Access
    // ========================================================================

    /**
     * @brief Get a font for the current theme
     * @param role Font role
     * @return Font for the role
     */
    QFont font(FontRole role) const;

    /**
     * @brief Set base font size (scales all fonts)
     */
    void setBaseFontSize(int size);

    // ========================================================================
    // Style Sheets
    // ========================================================================

    /**
     * @brief Get the complete application stylesheet
     */
    QString applicationStyleSheet() const;

    /**
     * @brief Get stylesheet for main window
     */
    QString mainWindowStyleSheet() const;

    /**
     * @brief Get stylesheet for device cards
     */
    QString deviceCardStyleSheet() const;

    /**
     * @brief Get stylesheet for buttons
     */
    QString buttonStyleSheet() const;

    /**
     * @brief Get stylesheet for primary (accent) buttons
     */
    QString primaryButtonStyleSheet() const;

    /**
     * @brief Get stylesheet for danger buttons
     */
    QString dangerButtonStyleSheet() const;

    /**
     * @brief Get stylesheet for list widgets
     */
    QString listWidgetStyleSheet() const;

    /**
     * @brief Get stylesheet for progress bars
     */
    QString progressBarStyleSheet() const;

    /**
     * @brief Get stylesheet for input fields
     */
    QString inputFieldStyleSheet() const;

    /**
     * @brief Get stylesheet for labels
     */
    QString labelStyleSheet() const;

    /**
     * @brief Get stylesheet for scroll areas
     */
    QString scrollAreaStyleSheet() const;

    /**
     * @brief Get stylesheet for tooltips
     */
    QString tooltipStyleSheet() const;

    /**
     * @brief Get stylesheet for dialogs
     */
    QString dialogStyleSheet() const;

    /**
     * @brief Get stylesheet for menus
     */
    QString menuStyleSheet() const;

    /**
     * @brief Get stylesheet for tabs
     */
    QString tabWidgetStyleSheet() const;

    /**
     * @brief Get stylesheet for status indicators
     */
    QString statusIndicatorStyleSheet(ColorRole statusColor) const;

    // ========================================================================
    // Animation Helpers
    // ========================================================================

    /**
     * @brief Get glow animation for a widget
     * @param target Widget to animate
     * @param glowColor Glow color
     * @param duration Animation duration in ms
     */
    std::unique_ptr<QPropertyAnimation> createGlowAnimation(
        QWidget* target, 
        const QColor& glowColor,
        int duration = 1000
    ) const;

    /**
     * @brief Apply fade-in animation to widget
     */
    void applyFadeIn(QWidget* widget, int duration = 300) const;

    /**
     * @brief Apply pulse animation to widget
     */
    void applyPulse(QWidget* widget, const QColor& color, int duration = 800) const;

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /**
     * @brief Apply theme to application palette
     */
    void applyToApplication();

    /**
     * @brief Get recommended icon size
     */
    int iconSize() const { return m_iconSize; }

    /**
     * @brief Get recommended spacing
     */
    int spacing() const { return m_spacing; }

    /**
     * @brief Get recommended border radius
     */
    int borderRadius() const { return m_borderRadius; }

    /**
     * @brief Get CSS for box shadow
     */
    QString boxShadowCss(int blur = 20, int spread = 5) const;

    /**
     * @brief Get CSS for glow effect
     */
    QString glowCss(ColorRole color, int blur = 15) const;

    /**
     * @brief Enable or disable animations
     */
    void setAnimationsEnabled(bool enabled) { m_animationsEnabled = enabled; }
    bool animationsEnabled() const { return m_animationsEnabled; }

signals:
    /**
     * @brief Emitted when the theme changes
     */
    void themeChanged(Theme newTheme);

private:
    StyleManager();
    ~StyleManager() = default;

    /**
     * @brief Load color palette for a theme
     */
    void loadTheme(Theme theme);

    /**
     * @brief Generate stylesheet from current colors
     */
    void generateStyleSheet();

    // Current state
    Theme m_currentTheme = Theme::CyberDark;
    QHash<ColorRole, QColor> m_colors;
    QHash<FontRole, QFont> m_fonts;
    QString m_cachedStyleSheet;

    // Configuration
    int m_baseFontSize = 10;
    int m_iconSize = 24;
    int m_spacing = 8;
    int m_borderRadius = 6;
    bool m_animationsEnabled = true;

    // Theme color palettes
    static const QHash<Theme, QHash<ColorRole, QColor>> s_themePalettes;
};

// Convenience macros for accessing style manager
#define FSStyle FlashSentry::StyleManager::instance()
#define FSColor(role) FlashSentry::StyleManager::instance().color(FlashSentry::StyleManager::ColorRole::role)
#define FSFont(role) FlashSentry::StyleManager::instance().font(FlashSentry::StyleManager::FontRole::role)

} // namespace FlashSentry