#pragma once

#include <QByteArray>
#include <QString>
#include <functional>

namespace FlashSpartan {

/** HTTP fetch for ISO verification (replaceable in tests). */
class IsoHttpClient {
public:
    using Handler = std::function<QByteArray(const QString& url, QString* errorOut, int timeoutMs)>;

    static QByteArray get(const QString& url, QString* errorOut = nullptr, int timeoutMs = 90000);
    static void setHandler(Handler handler);
    static void reset();

private:
    static Handler& handlerRef();
};

} // namespace FlashSpartan
