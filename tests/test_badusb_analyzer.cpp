#include <QtTest>

#include "BadUsbAnalyzer.h"

using namespace FlashSpartan;

class TestBadUsbAnalyzer : public QObject {
    Q_OBJECT

private slots:
    void newKeyboardAnomaly();
    void compositeKeyboardStorageIsCritical();
    void trustedKeyboardPasses();
    void interfaceDriftAnomaly();
};

static HidDeviceInfo keyboard()
{
    HidDeviceInfo info;
    info.vendorId = QStringLiteral("046d");
    info.productId = QStringLiteral("c31c");
    info.serial = QStringLiteral("ABC");
    info.product = QStringLiteral("USB Keyboard");
    info.capabilities = {QStringLiteral("keyboard")};
    HidInterfaceInfo iface;
    iface.number = QStringLiteral("00");
    iface.interfaceClass = QStringLiteral("03");
    iface.interfaceSubClass = QStringLiteral("01");
    iface.interfaceProtocol = QStringLiteral("01");
    iface.driver = QStringLiteral("usbhid");
    info.interfaces.append(iface);
    return info;
}

void TestBadUsbAnalyzer::newKeyboardAnomaly()
{
    AppSettings settings;
    const auto result = BadUsbAnalyzer::analyzeConnect(keyboard(), std::nullopt, {}, 1, settings);
    QVERIFY(result.anomalous);
    QCOMPARE(result.ruleId, QStringLiteral("new-keyboard"));
}

void TestBadUsbAnalyzer::compositeKeyboardStorageIsCritical()
{
    AppSettings settings;
    const auto result =
        BadUsbAnalyzer::analyzeConnect(keyboard(), std::nullopt, {QStringLiteral("/dev/sdb1")}, 1, settings);
    QVERIFY(result.anomalous);
    QCOMPARE(result.severity, BadUsbSeverity::Critical);
    QCOMPARE(result.ruleId, QStringLiteral("composite-storage-keyboard"));
}

void TestBadUsbAnalyzer::trustedKeyboardPasses()
{
    AppSettings settings;
    BadUsbBaselineEntry baseline;
    baseline.stableId = keyboard().stableId();
    baseline.device = keyboard();
    baseline.trusted = true;
    const auto result = BadUsbAnalyzer::analyzeConnect(keyboard(), baseline, {}, 1, settings);
    QVERIFY(!result.anomalous);
}

void TestBadUsbAnalyzer::interfaceDriftAnomaly()
{
    AppSettings settings;
    BadUsbBaselineEntry baseline;
    baseline.stableId = keyboard().stableId();
    baseline.device = keyboard();
    baseline.trusted = true;

    HidDeviceInfo changed = keyboard();
    changed.interfaces[0].interfaceProtocol = QStringLiteral("02");
    const auto result = BadUsbAnalyzer::analyzeConnect(changed, baseline, {}, 1, settings);
    QVERIFY(result.anomalous);
    QCOMPARE(result.ruleId, QStringLiteral("interface-drift"));
}

QTEST_MAIN(TestBadUsbAnalyzer)
#include "test_badusb_analyzer.moc"
