#include "../../core/updater.h"
#include <QtTest>

class TestVersion : public QObject {
    Q_OBJECT

private slots:
    void test();
};

void TestVersion::test()
{
    Updater updater;

    // Objective: ARG1 > ARG2
    QVERIFY(updater.compare("1.1", "1.0"));
    QVERIFY(updater.compare("1.1.0", "1.0"));
    QVERIFY(updater.compare("1.0.1", "1.0"));
    QVERIFY(updater.compare("1.0.0.19", "1.0"));
    QVERIFY(updater.compare("1.0.0.19", "1.0.0.2"));
    QVERIFY(updater.compare("1.0.0.19", "1.0.0.2.777"));

    // Objective: ARG1 == ARG2
    QCOMPARE(updater.compare("1.0", "1.0.0.0"), false);
    QVERIFY(updater.compare("3.0.0", "3.0.0-beta5"));
    QVERIFY(!updater.compare("3.0.0-beta5", "3.0.0"));
    QVERIFY(updater.compare("3.0.0-beta5", "3.0.0-beta4.1"));
    QVERIFY(updater.compare("3.0.0-beta10", "3.0.0-beta9"));
    QVERIFY(updater.compare("3.0.0-rc1", "3.0.0-beta5"));
    QVERIFY(updater.compare("v3.0.1-beta1", "3.0.0"));
    QVERIFY(!updater.compare("v3.0.0+local", "3.0.0"));
    QVERIFY(!updater.compare("", "3.0.0"));
}

QTEST_MAIN(TestVersion)

#include "test_version.moc"
