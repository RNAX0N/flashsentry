#include "IsoVerifyReport.h"
#include "IsoVerifyUi.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace FlashSpartan {

static QString csvEscape(QString value)
{
    return value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
}

int IsoVerifyReport::countFailed(const SummaryCounts& counts)
{
    return counts.total - counts.passed - counts.needsSidecar;
}

IsoVerifyReport::SummaryCounts IsoVerifyReport::countSummary(const QList<IsoVerifyResult>& results)
{
    SummaryCounts counts;
    counts.total = results.size();
    for (const IsoVerifyResult& r : results) {
        if (r.inconclusive()) {
            ++counts.needsSidecar;
        } else if (r.passed()) {
            ++counts.passed;
        }
    }
    return counts;
}

QString IsoVerifyReport::summaryLine(const QList<IsoVerifyResult>& results)
{
    const SummaryCounts counts = countSummary(results);
    return IsoVerifyUi::summaryLine(counts.passed, counts.total, counts.needsSidecar);
}

QString IsoVerifyReport::buildPlainText(const QList<IsoVerifyResult>& results)
{
    QString out = summaryLine(results) + QLatin1Char('\n');
    for (const IsoVerifyResult& r : results) {
        out += QLatin1String("\n---\n");
        out += r.reportSummary;
        out += QLatin1Char('\n');
    }
    return out;
}

QString IsoVerifyReport::buildCsv(const QList<IsoVerifyResult>& results)
{
    QString out = QStringLiteral("file,publisher,passed,inconclusive,hash_ok,pgp_valid,error\n");
    for (const IsoVerifyResult& r : results) {
        const QString file = r.isoPath.isEmpty() ? r.layoutNote : QFileInfo(r.isoPath).fileName();
        const QString publisher =
            r.publisherName + QLatin1Char(' ') + r.releaseLabel;
        QString line = QStringLiteral("\"%1\",\"%2\",%3,%4,%5,%6,\"%7\"\n")
                           .arg(csvEscape(file),
                                csvEscape(publisher),
                                r.passed() ? QStringLiteral("yes") : QStringLiteral("no"),
                                r.inconclusive() ? QStringLiteral("yes") : QStringLiteral("no"),
                                r.hashMatches ? QStringLiteral("yes") : QStringLiteral("no"),
                                r.pgpValid ? QStringLiteral("yes") : QStringLiteral("no"),
                                csvEscape(r.errorMessage));
        out += line;
    }
    return out;
}

static QJsonObject isoResultToJson(const IsoVerifyResult& r)
{
    const QString file = r.isoPath.isEmpty() ? r.layoutNote : QFileInfo(r.isoPath).fileName();
    QJsonObject obj;
    obj.insert(QStringLiteral("file"), file);
    obj.insert(QStringLiteral("path"), r.isoPath);
    obj.insert(QStringLiteral("publisher_id"), r.publisherId);
    obj.insert(QStringLiteral("publisher"), r.publisherName);
    obj.insert(QStringLiteral("release"), r.releaseLabel);
    obj.insert(QStringLiteral("passed"), r.passed());
    obj.insert(QStringLiteral("inconclusive"), r.inconclusive());
    obj.insert(QStringLiteral("success"), r.success);
    obj.insert(QStringLiteral("hash_checked"), r.hashChecked);
    obj.insert(QStringLiteral("hash_matches"), r.hashMatches);
    obj.insert(QStringLiteral("expected_sha256"), r.expectedSha256);
    obj.insert(QStringLiteral("computed_sha256"), r.computedSha256);
    obj.insert(QStringLiteral("pgp_checked"), r.pgpChecked);
    obj.insert(QStringLiteral("pgp_valid"), r.pgpValid);
    obj.insert(QStringLiteral("fingerprint_trusted"), r.fingerprintTrusted);
    obj.insert(QStringLiteral("signing_key_fingerprint"), r.signingKeyFingerprint);
    obj.insert(QStringLiteral("source"), static_cast<int>(r.source));
    if (!r.errorMessage.isEmpty()) {
        obj.insert(QStringLiteral("error"), r.errorMessage);
    }
    if (!r.layoutNote.isEmpty() && r.isoPath.isEmpty()) {
        obj.insert(QStringLiteral("layout_note"), r.layoutNote);
    }
    return obj;
}

QString IsoVerifyReport::buildJson(const QList<IsoVerifyResult>& results)
{
    const SummaryCounts counts = countSummary(results);
    QJsonObject root;
    QJsonObject summary;
    summary.insert(QStringLiteral("passed"), counts.passed);
    summary.insert(QStringLiteral("total"), counts.total);
    summary.insert(QStringLiteral("needs_sidecar"), counts.needsSidecar);
    root.insert(QStringLiteral("summary"), summary);
    root.insert(QStringLiteral("summary_line"), summaryLine(results));

    QJsonArray items;
    for (const IsoVerifyResult& r : results) {
        items.append(isoResultToJson(r));
    }
    root.insert(QStringLiteral("results"), items);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString IsoVerifyReport::buildHtml(const QList<IsoVerifyResult>& results)
{
    QString html = QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>FlashSpartan report</title>"
        "<style>body{font-family:sans-serif}table{border-collapse:collapse;width:100%}"
        "td,th{border:1px solid #ccc;padding:6px}tr.fail{background:#fdd}"
        "tr.inconclusive{background:#ffd}</style></head><body>");
    html += QStringLiteral("<h1>FlashSpartan verification</h1><p>%1</p><table><tr>"
                           "<th>File</th><th>Publisher</th><th>SHA-256</th><th>PGP</th><th>Status</th></tr>")
                .arg(summaryLine(results).toHtmlEscaped());
    for (const IsoVerifyResult& r : results) {
        const QString file = r.isoPath.isEmpty() ? r.layoutNote : QFileInfo(r.isoPath).fileName();
        const QString hashCol = IsoVerifyUi::hashColumnText(r);
        const QString pgp = IsoVerifyUi::pgpColumnText(r);
        const QString statusLabel = IsoVerifyUi::outcomeLabel(r);
        const QString rowClass = r.inconclusive() ? QStringLiteral(" class=\"inconclusive\"")
                                 : r.passed()     ? QString()
                                                  : QStringLiteral(" class=\"fail\"");
        html += QStringLiteral("<tr%1><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td></tr>")
                    .arg(rowClass,
                         file.toHtmlEscaped(),
                         (r.publisherName + QLatin1Char(' ') + r.releaseLabel).toHtmlEscaped(),
                         hashCol.toHtmlEscaped(),
                         pgp.toHtmlEscaped(),
                         statusLabel.toHtmlEscaped());
    }
    html += QStringLiteral("</table></body></html>");
    return html;
}

} // namespace FlashSpartan
