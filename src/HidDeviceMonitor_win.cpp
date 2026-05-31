#include "HidDeviceMonitor.h"

#ifdef Q_OS_WIN

#include <qt_windows.h>

#include <QDateTime>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>

#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpropdef.h>
#include <devpkey.h>

namespace FlashSentry {

namespace {

QString registryProperty(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& devInfo, DWORD property)
{
    DWORD required = 0;
    SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &devInfo, property, nullptr, nullptr, 0,
                                    &required);
    if (required == 0) {
        return {};
    }
    QByteArray buffer;
    buffer.resize(static_cast<int>(required));
    if (!SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet, &devInfo, property, nullptr,
            reinterpret_cast<PBYTE>(buffer.data()), required, &required)) {
        return {};
    }
    return QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.constData())).trimmed();
}

bool appendHidUsageCapabilities(HANDLE hidHandle, QStringList& capabilities)
{
    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(hidHandle, &preparsed) || !preparsed) {
        return false;
    }

    HIDP_CAPS caps{};
    if (HidP_GetCaps(preparsed, &caps) != HIDP_STATUS_SUCCESS) {
        HidD_FreePreparsedData(preparsed);
        return false;
    }

    const USAGE_AND_PAGE top{caps.UsagePage, caps.Usage};
    if (top.UsagePage == 0x01 && top.Usage == 0x06) {
        capabilities.append(QStringLiteral("keyboard"));
    } else if (top.UsagePage == 0x01 && top.Usage == 0x02) {
        capabilities.append(QStringLiteral("mouse"));
    } else if (top.UsagePage == 0x01 && top.Usage == 0x05) {
        capabilities.append(QStringLiteral("gamepad"));
    } else if (top.UsagePage == 0x0D) {
        capabilities.append(QStringLiteral("touchpad"));
    }
    HidD_FreePreparsedData(preparsed);
    return true;
}

using HidStringFn = BOOLEAN(WINAPI*)(HANDLE, PVOID, ULONG);

static BOOLEAN WINAPI hidManufacturer(HANDLE h, PVOID b, ULONG n)
{
    return HidD_GetManufacturerString(h, b, n);
}

static BOOLEAN WINAPI hidProduct(HANDLE h, PVOID b, ULONG n)
{
    return HidD_GetProductString(h, b, n);
}

static BOOLEAN WINAPI hidSerial(HANDLE h, PVOID b, ULONG n)
{
    return HidD_GetSerialNumberString(h, b, n);
}

QString hidString(HANDLE hidHandle, HidStringFn getter)
{
    wchar_t buffer[256] = {};
    if (!getter || !getter(hidHandle, buffer, static_cast<ULONG>(sizeof(buffer)))) {
        return {};
    }
    return QString::fromWCharArray(buffer).trimmed();
}

QString parentUsbInstanceId(const QString& devicePath)
{
    wchar_t instanceId[MAX_DEVICE_ID_LEN] = {};
    DEVPROPTYPE propType = 0;
    ULONG propSize = sizeof(instanceId);
    if (CM_Get_Device_Interface_PropertyW(
            reinterpret_cast<LPCWSTR>(devicePath.utf16()), &DEVPKEY_Device_InstanceId, &propType,
            reinterpret_cast<PBYTE>(instanceId), &propSize, 0)
        != CR_SUCCESS) {
        return {};
    }

    DEVINST devInst = 0;
    if (CM_Locate_DevNodeW(&devInst, instanceId, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) {
        return {};
    }

    for (int depth = 0; depth < 8; ++depth) {
        wchar_t id[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDW(devInst, id, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
            const QString instance = QString::fromWCharArray(id);
            if (instance.startsWith(QStringLiteral("USB\\"), Qt::CaseInsensitive)) {
                return instance;
            }
        }
        DEVINST parent = 0;
        if (CM_Get_Parent(&parent, devInst, 0) != CR_SUCCESS) {
            break;
        }
        devInst = parent;
    }
    return {};
}

void parseVidPidFromPath(const QString& path, QString& vendorId, QString& productId)
{
    static const QRegularExpression re(
        QStringLiteral(R"(vid_([0-9a-f]{4})&pid_([0-9a-f]{4}))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(path);
    if (match.hasMatch()) {
        vendorId = match.captured(1).toLower();
        productId = match.captured(2).toLower();
    }
}

QString busFromInstanceId(const QString& instanceId)
{
    static const QRegularExpression re(QStringLiteral(R"(\\d+$)"));
    const QRegularExpressionMatch match = re.match(instanceId);
    if (match.hasMatch()) {
        return match.captured(0);
    }
    return QStringLiteral("1");
}

std::optional<HidDeviceInfo> readHidInterface(const QString& devicePath)
{
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(devicePath.utf16()), 0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    HidDeviceInfo info;
    info.sysPath = devicePath;
    info.devNode = devicePath;
    info.seenAtUtc = QDateTime::currentDateTimeUtc();

    HIDD_ATTRIBUTES attrs{};
    attrs.Size = sizeof(attrs);
    if (HidD_GetAttributes(handle, &attrs) == FALSE) {
        CloseHandle(handle);
        return std::nullopt;
    }

    info.vendorId = QStringLiteral("%1").arg(attrs.VendorID, 4, 16, QChar('0'));
    info.productId = QStringLiteral("%1").arg(attrs.ProductID, 4, 16, QChar('0'));
    info.manufacturer = hidString(handle, hidManufacturer);
    info.product = hidString(handle, hidProduct);
    info.serial = hidString(handle, hidSerial);

    appendHidUsageCapabilities(handle, info.capabilities);
    CloseHandle(handle);

    if (info.vendorId.isEmpty() || info.productId.isEmpty()) {
        parseVidPidFromPath(devicePath, info.vendorId, info.productId);
    }

    const QString usbInstance = parentUsbInstanceId(devicePath);
    info.usbPath = usbInstance.isEmpty() ? devicePath : usbInstance;
    info.usbBus = busFromInstanceId(usbInstance.isEmpty() ? devicePath : usbInstance);
    info.usbPort = info.usbPath;

    if (info.capabilities.isEmpty()) {
        info.capabilities.append(QStringLiteral("hid"));
    }
    info.capabilities.removeDuplicates();
    return info;
}

} // namespace

void HidDeviceMonitor::scanExistingDevices()
{
    QHash<QString, HidDeviceInfo> detected;

    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr,
                                                  DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return;
    }

    SP_DEVICE_INTERFACE_DATA interfaceData{};
    interfaceData.cbSize = sizeof(interfaceData);
    for (DWORD index = 0;
         SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &hidGuid, index, &interfaceData);
         ++index) {
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, nullptr, 0, &required,
                                       nullptr);
        if (required == 0) {
            continue;
        }

        QByteArray buffer;
        buffer.resize(static_cast<int>(required));
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, detail, required,
                                              nullptr, nullptr)) {
            continue;
        }

        const QString devicePath = QString::fromWCharArray(detail->DevicePath);
        if (devicePath.isEmpty()) {
            continue;
        }

        const std::optional<HidDeviceInfo> info = readHidInterface(devicePath);
        if (!info) {
            continue;
        }
        detected.insert(info->stableId(), *info);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    QList<QString> connected;
    QList<QString> changed;
    QList<QString> removed;
    {
        QMutexLocker locker(&m_devicesMutex);
        for (auto it = detected.constBegin(); it != detected.constEnd(); ++it) {
            if (!m_devices.contains(it.key())) {
                connected.append(it.key());
            } else if (m_devices.value(it.key()).interfaceSignatures()
                       != it.value().interfaceSignatures()) {
                changed.append(it.key());
            }
        }
        for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
            if (!detected.contains(it.key())) {
                removed.append(it.key());
            }
        }
        m_devices = detected;
    }

    for (const QString& id : connected) {
        emit hidConnected(detected.value(id));
    }
    for (const QString& id : changed) {
        emit hidChanged(detected.value(id));
    }
    for (const QString& id : removed) {
        emit hidDisconnected(id);
    }
}

void HidDeviceMonitor::run()
{
    scanExistingDevices();
    {
        QMutexLocker locker(&m_devicesMutex);
        emit initialScanComplete(m_devices.size());
    }

    while (m_running.load()) {
        if (m_rescanRequested.exchange(false)) {
            scanExistingDevices();
        }
        msleep(POLL_TIMEOUT_MS);
        scanExistingDevices();
    }
}

void HidDeviceMonitor::rescan()
{
    m_rescanRequested.store(true);
}

bool HidDeviceMonitor::initializeUdev()
{
    return true;
}

void HidDeviceMonitor::cleanupUdev() {}

void HidDeviceMonitor::processUdevEvent(struct udev_device* /*dev*/) {}

bool HidDeviceMonitor::isUsbHidInput(struct udev_device* /*dev*/) const
{
    return false;
}

HidDeviceInfo HidDeviceMonitor::extractDeviceInfo(struct udev_device* /*dev*/) const
{
    return {};
}

struct udev_device* HidDeviceMonitor::getUsbDeviceParent(struct udev_device* /*dev*/) const
{
    return nullptr;
}

struct udev_device* HidDeviceMonitor::getUsbInterfaceParent(struct udev_device* /*dev*/) const
{
    return nullptr;
}

QString HidDeviceMonitor::getProperty(struct udev_device* /*dev*/, const char* /*key*/) const
{
    return {};
}

QString HidDeviceMonitor::getSysAttr(struct udev_device* /*dev*/, const char* /*key*/) const
{
    return {};
}

} // namespace FlashSentry

#endif
