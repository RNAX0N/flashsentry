#include "IsoBaselineService.h"

#include "IsoQuickFingerprint.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace FlashSpartan {

QString IsoBaselineService::relativeImagePath(const QString& mountPoint, const QString& isoPath)
{
    if (isoPath.isEmpty()) {
        return {};
    }
    if (mountPoint.isEmpty()) {
        return QFileInfo(isoPath).fileName();
    }

    QString mount = QDir::cleanPath(mountPoint);
    QString absolute = QDir::cleanPath(isoPath);
    if (!mount.endsWith(QLatin1Char('/'))) {
        mount += QLatin1Char('/');
    }
    if (absolute.startsWith(mount, Qt::CaseInsensitive)) {
        return absolute.mid(mount.size());
    }
    return QFileInfo(isoPath).fileName();
}

QHash<QString, IsoImageBaseline> IsoBaselineService::baselinesByRelativePath(
    const QList<IsoImageBaseline>& baselines)
{
    QHash<QString, IsoImageBaseline> mapped;
    for (const IsoImageBaseline& baseline : baselines) {
        if (!baseline.relativePath.isEmpty()) {
            mapped.insert(baseline.relativePath, baseline);
        }
    }
    return mapped;
}

std::optional<IsoImageBaseline> IsoBaselineService::findBaseline(
    const QList<IsoImageBaseline>& baselines, const QString& relativePath)
{
    for (const IsoImageBaseline& baseline : baselines) {
        if (baseline.relativePath == relativePath) {
            return baseline;
        }
    }
    return std::nullopt;
}

IsoBaselineService::ProcessingOutcome IsoBaselineService::process(
    const QString& mountPoint, const QList<IsoVerifyResult>& results,
    const QList<IsoImageBaseline>& existingBaselines, bool compareBaselines, bool storeBaselines)
{
    ProcessingOutcome outcome;
    outcome.results = results;
    outcome.updatedBaselines = existingBaselines;

    QHash<QString, IsoImageBaseline> baselineMap = baselinesByRelativePath(outcome.updatedBaselines);

    for (IsoVerifyResult& result : outcome.results) {
        if (result.isoPath.isEmpty() || result.computedSha256.isEmpty()) {
            continue;
        }

        const QString relativePath = relativeImagePath(mountPoint, result.isoPath);
        const std::optional<IsoImageBaseline> baseline = findBaseline(existingBaselines, relativePath);
        if (!baseline.has_value() || !compareBaselines) {
            continue;
        }

        result.baselineChecked = true;
        result.storedBaselineSha256 = baseline->sha256;
        result.baselineMatches =
            result.computedSha256.trimmed().toLower() == baseline->sha256.trimmed().toLower();
        if (result.quickFingerprintMismatch) {
            result.baselineMatches = false;
        }
    }

    if (!storeBaselines) {
        return outcome;
    }

    for (const IsoVerifyResult& result : outcome.results) {
        if (result.isoPath.isEmpty() || result.computedSha256.isEmpty()) {
            continue;
        }

        IsoImageBaseline baseline;
        baseline.relativePath = relativeImagePath(mountPoint, result.isoPath);
        baseline.sha256 = result.computedSha256;
        baseline.quickFingerprint = IsoQuickFingerprint::compute(result.isoPath);
        baseline.sizeBytes = QFileInfo(result.isoPath).size();
        baseline.recordedAt = QDateTime::currentDateTimeUtc();
        baseline.publisherVerified =
            result.passed() || (result.hashChecked && result.hashMatches && !result.inconclusive());

        const IsoImageBaseline previous = baselineMap.value(baseline.relativePath);
        if (previous.sha256 != baseline.sha256 || previous.quickFingerprint != baseline.quickFingerprint
            || previous.sizeBytes != baseline.sizeBytes) {
            outcome.baselinesChanged = true;
        }
        baselineMap.insert(baseline.relativePath, baseline);
    }

    if (outcome.baselinesChanged) {
        outcome.updatedBaselines = baselineMap.values();
    }

    return outcome;
}

} // namespace FlashSpartan
