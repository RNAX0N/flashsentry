#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

namespace FlashSpartan {

struct MountResult {
    QString deviceNode;
    QString mountPoint;
    bool success = false;
    QString errorMessage;
};

struct UnmountResult {
    QString deviceNode;
    bool success = false;
    QString errorMessage;
    bool forcedUnmount = false;
};

struct MountOptions {
    QString filesystem;
    bool readOnly = false;
    bool noExec = true;
    bool noSuid = true;
    bool sync = false;
    QStringList extraOptions;
};

struct UnmountOptions {
    bool force = false;
    bool lazy = false;
};

} // namespace FlashSpartan

Q_DECLARE_METATYPE(FlashSpartan::MountResult)
Q_DECLARE_METATYPE(FlashSpartan::UnmountResult)
