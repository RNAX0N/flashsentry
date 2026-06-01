#include "WinUsbEnumerator.h"

#ifdef Q_OS_WIN

#include <qt_windows.h>

#include <QRegularExpression>
#include <QSet>

#include <cfgmgr32.h>
#include <devpropdef.h>
#include <devpkey.h>
#include <setupapi.h>

namespace FlashSpartan::WinUsbEnumerator {

namespace {

QString devPropString(DEVINST devInst, const DEVPROPKEY* key)
{
    DEVPROPTYPE propType = 0;
    ULONG propSize = 0;
    if (CM_Get_DevNode_PropertyW(devInst, key, &propType, nullptr, &propSize, 0) != CR_BUFFER_SMALL) {
        return {};
    }
    QByteArray buffer;
    buffer.resize(static_cast<int>(propSize));
    if (CM_Get_DevNode_PropertyW(devInst, key, &propType,
                                 reinterpret_cast<PBYTE>(buffer.data()), &propSize, 0)
        != CR_SUCCESS) {
        return {};
    }
    if (propType != DEVPROP_TYPE_STRING) {
        return {};
    }
    return QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.constData())).trimmed();
}

QString categoryForInstanceId(const QString& instanceId, const QString& className,
                              const QString& friendlyName)
{
    const QString upper = instanceId.toUpper();
    if (upper.startsWith(QStringLiteral("USBSTOR\\"))) {
        return QStringLiteral("USB storage (disk)");
    }
    if (upper.contains(QStringLiteral("ROOT_HUB")) || upper.contains(QStringLiteral("ROOTHUB"))) {
        return QStringLiteral("USB hub");
    }

    const QString classLower = className.toLower();
    if (classLower.contains(QStringLiteral("hid"))) {
        if (friendlyName.contains(QStringLiteral("fido"), Qt::CaseInsensitive)
            || friendlyName.contains(QStringLiteral("security key"), Qt::CaseInsensitive)
            || friendlyName.contains(QStringLiteral("yubi"), Qt::CaseInsensitive)) {
            return QStringLiteral("Security key (HID)");
        }
        return QStringLiteral("HID device");
    }
    if (classLower.contains(QStringLiteral("disk")) || classLower.contains(QStringLiteral("volume"))) {
        return QStringLiteral("USB storage");
    }
    if (classLower.contains(QStringLiteral("battery"))
        || classLower.contains(QStringLiteral("portable"))) {
        return QStringLiteral("USB power / battery");
    }
    if (classLower.contains(QStringLiteral("bluetooth"))) {
        return QStringLiteral("Bluetooth (USB)");
    }
    if (classLower.contains(QStringLiteral("media")) || classLower.contains(QStringLiteral("wpd"))) {
        return QStringLiteral("Portable device");
    }
    if (classLower.contains(QStringLiteral("net"))) {
        return QStringLiteral("USB network");
    }
    if (classLower.contains(QStringLiteral("image")) || classLower.contains(QStringLiteral("camera"))) {
        return QStringLiteral("USB camera");
    }
    if (classLower.contains(QStringLiteral("usb")) || upper.startsWith(QStringLiteral("USB\\"))) {
        return QStringLiteral("USB device");
    }
    return QStringLiteral("USB attachment");
}

void parseVidPid(const QString& instanceId, QString& vendorId, QString& productId)
{
    static const QRegularExpression re(
        QStringLiteral(R"(VID_([0-9A-F]{4})&PID_([0-9A-F]{4}))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(instanceId);
    if (match.hasMatch()) {
        vendorId = match.captured(1).toLower();
        productId = match.captured(2).toLower();
    }
}

std::optional<UsbHostDeviceInfo> deviceFromInstanceId(const QString& instanceId)
{
    if (instanceId.isEmpty()) {
        return std::nullopt;
    }

    DEVINST devInst = 0;
    if (CM_Locate_DevNodeW(&devInst, reinterpret_cast<DEVINSTID_W>(const_cast<wchar_t*>(
                                                    reinterpret_cast<const wchar_t*>(instanceId.utf16()))),
                           CM_LOCATE_DEVNODE_NORMAL)
        != CR_SUCCESS) {
        return std::nullopt;
    }

    UsbHostDeviceInfo info;
    info.instanceId = instanceId;
    info.manufacturer = devPropString(devInst, &DEVPKEY_Device_Manufacturer);
    const QString friendly = devPropString(devInst, &DEVPKEY_NAME);
    const QString desc = devPropString(devInst, &DEVPKEY_Device_DeviceDesc);
    info.displayName = friendly.isEmpty() ? desc : friendly;
    if (info.displayName.isEmpty()) {
        info.displayName = instanceId.section(QLatin1Char('\\'), 0, 0);
    }

    const QString className = devPropString(devInst, &DEVPKEY_Device_Class);
    info.category = categoryForInstanceId(instanceId, className, info.displayName);
    parseVidPid(instanceId, info.vendorId, info.productId);

    if (info.manufacturer.isEmpty() && !info.vendorId.isEmpty()) {
        info.manufacturer = QStringLiteral("VID %1").arg(info.vendorId);
    }
    return info;
}

bool isUsbInstanceId(const QString& instanceId)
{
    const QString upper = instanceId.toUpper();
    if (upper.startsWith(QStringLiteral("USBSTOR\\"))) {
        return false;
    }
    return upper.startsWith(QStringLiteral("USB\\"));
}

} // namespace

QList<UsbHostDeviceInfo> enumeratePresentUsbDevices()
{
    QList<UsbHostDeviceInfo> devices;
    QSet<QString> seen;

    ULONG listLen = 0;
    if (CM_Get_Device_ID_List_SizeW(&listLen, nullptr, CM_GETIDLIST_FILTER_PRESENT) != CR_SUCCESS
        || listLen == 0) {
        return devices;
    }

    QVector<wchar_t> buffer(static_cast<int>(listLen), L'\0');
    if (CM_Get_Device_ID_ListW(nullptr, buffer.data(), listLen, CM_GETIDLIST_FILTER_PRESENT)
        != CR_SUCCESS) {
        return devices;
    }

    const wchar_t* cursor = buffer.constData();
    while (*cursor) {
        const QString instanceId = QString::fromWCharArray(cursor);
        cursor += instanceId.length() + 1;

        if (!isUsbInstanceId(instanceId) || seen.contains(instanceId)) {
            continue;
        }

        const std::optional<UsbHostDeviceInfo> info = deviceFromInstanceId(instanceId);
        if (!info) {
            continue;
        }

        seen.insert(instanceId);
        devices.append(*info);
    }

    return devices;
}

} // namespace WinUsbEnumerator
} // namespace FlashSpartan

#endif
