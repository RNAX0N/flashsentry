#include "IsoChecksum.h"

#include <QRegularExpression>
#include <QStringList>

namespace FlashSentry {

namespace {

QString normalizeHash(const QString& h)
{
    return h.trimmed().toLower();
}

} // namespace

QString IsoChecksum::parseSha256Content(const QString& content, const QString& isoBaseName,
                                        QString* errorOut)
{
    if (!isoBaseName.isEmpty()) {
        static const QRegularExpression parenRe(
            QStringLiteral("^SHA256 \\((.+?)\\) = ([0-9a-fA-F]{64})$"),
            QRegularExpression::CaseInsensitiveOption);
        for (QString line : content.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QRegularExpressionMatch m = parenRe.match(line.trimmed());
            if (m.hasMatch() && m.captured(1) == isoBaseName) {
                return normalizeHash(m.captured(2));
            }
        }
    }

    for (QString line : content.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int space = line.indexOf(QLatin1Char(' '));
        if (space <= 0) {
            continue;
        }
        QString hash = line.left(space);
        QString name = line.mid(space).trimmed();
        if (name.startsWith(QLatin1Char('*'))) {
            name = name.mid(1);
        }
        if (name.startsWith(QStringLiteral("./"))) {
            name = name.mid(2);
        }
        if (isoBaseName.isEmpty() || name == isoBaseName
            || name.endsWith(QLatin1Char('/') + isoBaseName)) {
            return normalizeHash(hash);
        }
    }

    const QString trimmed = content.trimmed();
    if (trimmed.size() == 64 && trimmed.indexOf(QLatin1Char(' ')) < 0) {
        return normalizeHash(trimmed);
    }

    if (errorOut) {
        *errorOut = isoBaseName.isEmpty()
            ? QStringLiteral("No valid SHA-256 checksum in file")
            : QStringLiteral("ISO not listed in checksum file");
    }
    return {};
}

} // namespace FlashSentry
