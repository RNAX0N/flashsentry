#include "IsoHttpClient.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace FlashSentry {

namespace {

QByteArray networkFetch(const QString& url, QString* errorOut, int timeoutMs)
{
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
#ifdef FLASHSENTRY_VERSION
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("FlashSentry/" FLASHSENTRY_VERSION));
#else
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FlashSentry"));
#endif

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Download timed out");
        }
        reply->abort();
        reply->deleteLater();
        return {};
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (errorOut) {
            *errorOut = reply->errorString();
        }
        reply->deleteLater();
        return {};
    }

    const QByteArray data = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (status >= 400) {
        if (errorOut) {
            *errorOut = QStringLiteral("HTTP %1").arg(status);
        }
        return {};
    }
    return data;
}

} // namespace

IsoHttpClient::Handler& IsoHttpClient::handlerRef()
{
    static Handler handler;
    return handler;
}

QByteArray IsoHttpClient::get(const QString& url, QString* errorOut, int timeoutMs)
{
    Handler& custom = handlerRef();
    if (custom) {
        return custom(url, errorOut, timeoutMs);
    }
    return networkFetch(url, errorOut, timeoutMs);
}

void IsoHttpClient::setHandler(Handler handler)
{
    handlerRef() = std::move(handler);
}

void IsoHttpClient::reset()
{
    handlerRef() = nullptr;
}

} // namespace FlashSentry
