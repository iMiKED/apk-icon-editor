#include <QtTest>
#include <QAbstractItemModelTester>
#include <QCheckBox>
#include <QTemporaryDir>
#include <QProcess>
#include <QTreeView>
#include <QTranslator>
#include <QFontDatabase>
#include "decorationdelegate.h"
#include "apkfile.h"
#include "adaptiveicon.h"
#include "iconsproxy.h"
#include "adaptivereplacementdialog.h"

class TestDecorationDelegate : public DecorationDelegate
{
public:
    using DecorationDelegate::DecorationDelegate;
    using DecorationDelegate::initStyleOption;
};

class TestIconVariants : public QObject
{
    Q_OBJECT
    static void write(const QString &path, const QByteArray &bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QFile::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }
    static QByteArray read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QFile::ReadOnly)) return QByteArray();
        return file.readAll();
    }
    static QPixmap solid(QColor color, int size = 192)
    {
        QPixmap pixmap(size, size);
        pixmap.fill(color);
        return pixmap;
    }
    static void fixture(const QString &root, bool adaptive = true, bool shared = false)
    {
        write(root + "/AndroidManifest.xml", "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"test.icons\"><application android:icon=\"@mipmap/launcher\"/></manifest>");
        for (const QString &density : { QString("mdpi"), QString("xxxhdpi") }) {
            const QString path = root + "/res/mipmap-" + density + "/launcher.png";
            QDir().mkpath(QFileInfo(path).absolutePath());
            QVERIFY(solid(Qt::green, density == "mdpi" ? 48 : 192).save(path));
        }
        if (adaptive) {
            write(root + "/res/mipmap-anydpi-v26/launcher.xml",
                  "<adaptive-icon xmlns:android=\"http://schemas.android.com/apk/res/android\"><background android:drawable=\"@android:color/white\"/><foreground android:drawable=\""
                  + QByteArray(shared ? "@mipmap/launcher" : "@drawable/foreground") + "\"/></adaptive-icon>");
            write(root + "/res/drawable/foreground.xml", "<vector xmlns:android=\"http://schemas.android.com/apk/res/android\" android:width=\"108dp\" android:height=\"108dp\" android:viewportWidth=\"108\" android:viewportHeight=\"108\"><path android:fillColor=\"#ffff0000\" android:pathData=\"M0,0 L108,0 L108,108 L0,108 Z\"/></vector>");
        }
    }
    static QList<Icon *> adaptiveIcons(IconsModel &model)
    {
        QList<Icon *> result;
        for (int row = 0; row < model.rowCount(); ++row) {
            Icon *icon = static_cast<Icon *>(model.index(row, 0).internalPointer());
            if (icon->isAdaptiveIcon()) result << icon;
        }
        return result;
    }

private slots:
    void adaptiveIsDefault()
    {
        QTemporaryDir dir;
        fixture(dir.path());
        Apk::File apk(dir.path());
        const auto icons = adaptiveIcons(apk.iconsModel);
        QCOMPARE(icons.size(), 7);
        QCOMPARE(icons.last()->getPixmap().toImage().pixelColor(96, 96), QColor(Qt::red));
        QCOMPARE(apk.iconsModel.legacyAlternatives(icons.last()).size(), 2);
        IconsProxy proxy;
        Device device;
        proxy.setDevice(&device);
        proxy.setSourceModel(&apk.iconsModel);
        QAbstractItemModelTester tester(&proxy, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(proxy.rowCount(), 1);
        const QModelIndex family = proxy.index(0, 0);
        QCOMPARE(proxy.rowCount(family), 2);
        const QModelIndex adaptive = proxy.index(0, 0, family);
        const QModelIndex legacy = proxy.index(1, 0, family);
        QVERIFY(adaptive.data(IconsProxy::DefaultExpandedRole).toBool());
        QVERIFY(!legacy.data(IconsProxy::DefaultExpandedRole).toBool());
        QCOMPARE(proxy.rowCount(adaptive), 7);
        QCOMPARE(proxy.rowCount(legacy), 2);
        QCOMPARE(qvariant_cast<QPixmap>(family.data(Qt::DecorationRole)).toImage(), icons.last()->getPixmap().toImage());
        QCOMPARE(qvariant_cast<QPixmap>(adaptive.data(Qt::DecorationRole)).toImage(), icons.last()->getPixmap().toImage());
        QCOMPARE(qvariant_cast<QPixmap>(legacy.data(Qt::DecorationRole)).toImage().pixelColor(96, 96), QColor(Qt::green));
        TestDecorationDelegate delegate(QSize(32, 32));
        QStyleOptionViewItem option;
        delegate.initStyleOption(&option, family);
        QCOMPARE(option.decorationSize, QSize(16, 16));
        delegate.initStyleOption(&option, legacy);
        QCOMPARE(option.decorationSize, QSize(16, 16));
        delegate.initStyleOption(&option, proxy.defaultIcon());
        QCOMPARE(option.decorationSize, QSize(32, 32));
        QCOMPARE(proxy.mapToSource(proxy.defaultIcon()).internalPointer(), static_cast<void *>(icons.last()));
        QCOMPARE(icons.first()->width(), 36);
        QVERIFY(!proxy.mapToSource(family).isValid());
        const QString exported = dir.path() + "/export.png";
        QVERIFY(icons.last()->save(exported));
        QCOMPARE(QImage(exported).pixelColor(96, 96), QColor(Qt::red));
    }

    void branchPreviewsTrackChanges()
    {
        QTemporaryDir dir;
        fixture(dir.path());
        Apk::File apk(dir.path());
        IconsProxy proxy;
        proxy.setSourceModel(&apk.iconsModel);
        QAbstractItemModelTester tester(&proxy, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const QModelIndex family = proxy.index(0, 0);
        const QModelIndex adaptive = proxy.index(0, 0, family);
        const QModelIndex legacy = proxy.index(1, 0, family);
        Icon *source = adaptiveIcons(apk.iconsModel).last();
        Icon *legacySource = apk.iconsModel.legacyAlternatives(source).last();
        QSignalSpy changed(&proxy, &IconsProxy::dataChanged);
        const auto headerChanged = [&changed](const QModelIndex &header) {
            for (const auto &arguments : changed) {
                if (arguments.at(0).value<QModelIndex>() == header
                        && arguments.at(2).value<QList<int>>().contains(Qt::DecorationRole)) return true;
            }
            return false;
        };
        QVERIFY(source->replace(solid(Qt::blue)));
        QVERIFY(headerChanged(family));
        QVERIFY(headerChanged(adaptive));
        QVERIFY(!headerChanged(legacy));
        QCOMPARE(qvariant_cast<QPixmap>(family.data(Qt::DecorationRole)).toImage().pixelColor(96, 96), QColor(Qt::blue));
        QCOMPARE(qvariant_cast<QPixmap>(legacy.data(Qt::DecorationRole)).toImage().pixelColor(96, 96), QColor(Qt::green));

        changed.clear();
        QVERIFY(legacySource->replace(solid(Qt::yellow)));
        QVERIFY(headerChanged(legacy));
        QVERIFY(!headerChanged(family));
        QCOMPARE(qvariant_cast<QPixmap>(legacy.data(Qt::DecorationRole)).toImage().pixelColor(96, 96), QColor(Qt::yellow));

        changed.clear();
        QVERIFY(source->revert());
        QVERIFY(headerChanged(family));
        QVERIFY(headerChanged(adaptive));
        QCOMPARE(qvariant_cast<QPixmap>(family.data(Qt::DecorationRole)).toImage().pixelColor(96, 96), QColor(Qt::red));

        for (Icon *icon : adaptiveIcons(apk.iconsModel)) QVERIFY(apk.iconsModel.remove(icon));
        const QModelIndex fallbackFamily = proxy.index(0, 0);
        QCOMPARE(proxy.rowCount(fallbackFamily), 1);
        QCOMPARE(qvariant_cast<QPixmap>(fallbackFamily.data(Qt::DecorationRole)).toImage().pixelColor(96, 96), QColor(Qt::yellow));
    }

    void everyLauncherBranchHasItsOwnPreview()
    {
        QTemporaryDir dir;
        fixture(dir.path());
        Apk::File apk(dir.path());
        const AdaptiveIconDescriptor descriptor = adaptiveIcons(apk.iconsModel).last()->getAdaptiveDescriptor();
        const QList<QColor> colors = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow };
        IconsModel model;
        for (int role = Icon::EntryApplicationIcon; role <= Icon::EntryActivityAliasRoundIcon; ++role) {
            const Icon::Scope scope = role <= Icon::EntryApplicationRoundIcon ? Icon::ScopeApplication : Icon::ScopeActivity;
            model.add(descriptor.xmlPath, solid(colors.at(role)), QStringList(), descriptor,
                      Icon::Xxxhdpi, scope, static_cast<Icon::EntryRole>(role));
        }
        IconsProxy proxy;
        proxy.setSourceModel(&model);
        QCOMPARE(proxy.rowCount(), 2);
        proxy.setShowActivities(true);
        QCOMPARE(proxy.rowCount(), colors.size());
        for (int row = 0; row < proxy.rowCount(); ++row) {
            const QModelIndex family = proxy.index(row, 0);
            const QImage preview = qvariant_cast<QPixmap>(family.data(Qt::DecorationRole)).toImage();
            QCOMPARE(preview.pixelColor(96, 96), colors.at(row));
            QCOMPARE(qvariant_cast<QPixmap>(proxy.index(0, 0, family).data(Qt::DecorationRole)).toImage(), preview);
        }
    }

    void unchangedAndAdaptiveOnlyPreserveLegacy()
    {
        QTemporaryDir dir;
        fixture(dir.path());
        Apk::File apk(dir.path());
        Icon *source = adaptiveIcons(apk.iconsModel).last();
        const auto legacy = apk.iconsModel.legacyAlternatives(source);
        QMap<QString, QByteArray> original;
        for (Icon *icon : legacy) original[icon->getFilename()] = read(icon->getFilename());
        apk.saveIcons();
        for (Icon *icon : legacy) QCOMPARE(read(icon->getFilename()), original.value(icon->getFilename()));
        QVERIFY(source->replace(solid(Qt::blue)));
        apk.iconsModel.updateAdaptiveFamily(source);
        apk.saveIcons();
        for (Icon *icon : legacy) QCOMPARE(read(icon->getFilename()), original.value(icon->getFilename()));
        Apk::File reopened(dir.path());
        QCOMPARE(adaptiveIcons(reopened.iconsModel).last()->getPixmap().toImage().pixelColor(96, 96), QColor(Qt::blue));
    }

    void explicitLegacyUpdate()
    {
        QTemporaryDir dir;
        fixture(dir.path());
        Apk::File apk(dir.path());
        Icon *source = adaptiveIcons(apk.iconsModel).last();
        QVERIFY(source->replace(solid(Qt::blue)));
        apk.iconsModel.updateAdaptiveFamily(source, true);
        apk.saveIcons();
        for (Icon *icon : apk.iconsModel.legacyAlternatives(source)) {
            const QImage image(icon->getFilename());
            QCOMPARE(image.size(), QSize(icon->width(), icon->height()));
            QCOMPARE(image.pixelColor(0, 0), QColor(Qt::blue));
        }
    }

    void sharedForegroundIsDetached()
    {
        QTemporaryDir dir;
        fixture(dir.path(), true, true);
        Apk::File apk(dir.path());
        Icon *source = adaptiveIcons(apk.iconsModel).last();
        QVERIFY(source->getAdaptiveDescriptor().needsXmlPatch());
        const QString legacyPath = dir.path() + "/res/mipmap-xxxhdpi/launcher.png";
        const QByteArray before = read(legacyPath);
        QVERIFY(source->replace(solid(Qt::blue)));
        apk.iconsModel.updateAdaptiveFamily(source);
        apk.saveIcons();
        QCOMPARE(read(legacyPath), before);
        Apk::File reopened(dir.path());
        QCOMPARE(adaptiveIcons(reopened.iconsModel).last()->getPixmap().toImage().pixelColor(96, 96), QColor(Qt::blue));
    }

    void bitmapOnlyListStaysFlat()
    {
        QTemporaryDir dir;
        fixture(dir.path(), false);
        Apk::File apk(dir.path());
        IconsProxy proxy;
        proxy.setSourceModel(&apk.iconsModel);
        QAbstractItemModelTester tester(&proxy, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(proxy.rowCount(), 2);
        QCOMPARE(proxy.rowCount(proxy.index(0, 0)), 0);
        QVERIFY(proxy.mapToSource(proxy.defaultIcon()).isValid());
        QVERIFY(!static_cast<Icon *>(proxy.mapToSource(proxy.defaultIcon()).internalPointer())->isLegacyIcon());
    }

    void unavailableAdaptiveHasExplicitFallback()
    {
        QTemporaryDir dir;
        fixture(dir.path());
        QVERIFY(QFile::remove(dir.path() + "/res/drawable/foreground.xml"));
        Apk::File apk(dir.path());
        QVERIFY(adaptiveIcons(apk.iconsModel).isEmpty());
        IconsProxy proxy;
        proxy.setSourceModel(&apk.iconsModel);
        const QModelIndex legacy = proxy.index(0, 0, proxy.index(0, 0));
        QVERIFY(legacy.data().toString().contains("Adaptive preview unavailable"));
        QVERIFY(legacy.data(IconsProxy::DefaultExpandedRole).toBool());
        QCOMPARE(proxy.rowCount(legacy), 2);
        QCOMPARE(qvariant_cast<QPixmap>(legacy.parent().data(Qt::DecorationRole)).toImage().pixelColor(96, 96), QColor(Qt::green));
        QCOMPARE(qvariant_cast<QPixmap>(legacy.data(Qt::DecorationRole)).toImage().pixelColor(96, 96), QColor(Qt::green));
    }

    void replacementDialogIsOptIn()
    {
        QTemporaryDir dir;
        fixture(dir.path());
        Apk::File apk(dir.path());
        Icon *source = adaptiveIcons(apk.iconsModel).last();
        const QByteArray xml = read(source->getAdaptiveXmlPath());
        AdaptiveReplacementDialog dialog(source, solid(Qt::blue), 7, apk.iconsModel.legacyAlternatives(source));
        QVERIFY(!dialog.updateLegacy());
        auto check = dialog.findChild<QCheckBox *>("updateLegacyCheck");
        QVERIFY(check);
        QVERIFY(check->isEnabled());
        check->setChecked(true);
        QVERIFY(dialog.updateLegacy());
        dialog.reject();
        QCOMPARE(read(source->getAdaptiveXmlPath()), xml);
        QCOMPARE(source->getPixmap().toImage().pixelColor(96, 96), QColor(Qt::red));
        AdaptiveReplacementDialog next(source, solid(Qt::blue), 7, apk.iconsModel.legacyAlternatives(source));
        QVERIFY(!next.updateLegacy());
    }

    void splitLegacyRemainsReadOnly()
    {
        QTemporaryDir dir;
        fixture(dir.path());
        const QString path = dir.path() + "/res/mipmap-mdpi/launcher.png";
        const QByteArray before = read(path);
        LegacyIconDescriptor descriptor;
        descriptor.adaptiveXmlPath = "split/res/mipmap-anydpi/icon.xml";
        descriptor.readOnly = true;
        Icon icon(path, descriptor, Icon::Mdpi);
        QVERIFY(icon.isReadOnly());
        QVERIFY(!icon.replace(solid(Qt::blue)));
        QVERIFY(!icon.resize(192, 192));
        QVERIFY(icon.save());
        QCOMPARE(read(path), before);
        QVERIFY(icon.save(dir.path() + "/export.png"));
    }

    void modelChangesRemainValid()
    {
        QTemporaryDir dir;
        fixture(dir.path(), false);
        IconsModel model;
        QAbstractItemModelTester sourceTester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        IconsProxy proxy;
        proxy.setSourceModel(&model);
        QAbstractItemModelTester tester(&proxy, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.add(dir.path() + "/res/mipmap-xxxhdpi/launcher.png");
        model.add(dir.path() + "/res/mipmap-mdpi/launcher.png");
        QCOMPARE(proxy.rowCount(), 2);
        QCOMPARE(static_cast<Icon *>(proxy.mapToSource(proxy.defaultIcon()).internalPointer())->getType(), Icon::Mdpi);
        model.remove(model.first());
        QCOMPARE(proxy.rowCount(), 1);
        proxy.setSourceModel(nullptr);
        QCOMPARE(proxy.rowCount(), 0);
    }

    void realFutubaPreview()
    {
        const QString path = qEnvironmentVariable("FUTUBA_DECODED");
        if (path.isEmpty()) QSKIP("Set FUTUBA_DECODED to an isolated decoded Futuba APK directory");
        Apk::File apk(path);
        const auto icons = adaptiveIcons(apk.iconsModel);
        QCOMPARE(icons.size(), 14);
        ResourceResolver resolver(path);
        const auto rendered = AdaptiveIcon::resolve(resolver, ResourceRef("@mipmap/ic_launcher"), Icon::Xxxhdpi, QSize(192, 192));
        QVERIFY(rendered.valid);
        Icon *app = nullptr;
        for (Icon *icon : icons) {
            if (icon->getEntryRole() == Icon::EntryApplicationIcon && icon->getType() == Icon::Xxxhdpi) app = icon;
        }
        QVERIFY(app);
        QCOMPARE(app->getPixmap().toImage(), rendered.pixmap.toImage());
        const auto legacy = apk.iconsModel.legacyAlternatives(app);
        QCOMPARE(legacy.size(), 5);
        QVERIFY(app->getPixmap().toImage() != legacy.last()->getPixmap().toImage());
        const QString screenshots = qEnvironmentVariable("ICON_VARIANTS_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            const int fontId = QFontDatabase::addApplicationFont(qEnvironmentVariable("ICON_VARIANTS_FONT"));
            if (fontId >= 0) qApp->setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).first(), 10));
            QTranslator russian;
            russian.load(qEnvironmentVariable("ICON_VARIANTS_RUSSIAN"));
            qApp->installTranslator(&russian);
            Device device;
            IconsProxy proxy;
            proxy.setDevice(&device);
            proxy.setSourceModel(&apk.iconsModel);
            QTreeView tree;
            tree.setHeaderHidden(true);
            tree.setIndentation(16);
            tree.setItemDelegate(new DecorationDelegate(QSize(32, 32), &tree));
            tree.setModel(&proxy);
            tree.resize(420, 520);
            for (int row = 0; row < proxy.rowCount(); ++row) {
                const QModelIndex family = proxy.index(row, 0);
                tree.setExpanded(family, family.data(IconsProxy::DefaultExpandedRole).toBool());
                for (int child = 0; child < proxy.rowCount(family); ++child) {
                    const QModelIndex group = proxy.index(child, 0, family);
                    tree.setExpanded(group, group.data(IconsProxy::DefaultExpandedRole).toBool());
                }
            }
            tree.setCurrentIndex(proxy.defaultIcon());
            tree.show();
            QCoreApplication::processEvents();
            QVERIFY(tree.grab().save(screenshots + "/icons-tree.png"));
            tree.setExpanded(proxy.index(1, 0, proxy.index(0, 0)), true);
            tree.resize(420, 700);
            QCoreApplication::processEvents();
            QVERIFY(tree.grab().save(screenshots + "/icons-tree-legacy.png"));
            AdaptiveReplacementDialog dialog(app, solid(Qt::blue), 7, legacy);
            dialog.show();
            QCoreApplication::processEvents();
            QVERIFY(dialog.grab().save(screenshots + "/replace-dialog.png"));
            qApp->removeTranslator(&russian);
        }
    }

    void realApkRoundTrip()
    {
        const QString input = qEnvironmentVariable("FUTUBA_APK");
        const QString jar = qEnvironmentVariable("APKTOOL_JAR");
        if (input.isEmpty() || jar.isEmpty()) QSKIP("Set FUTUBA_APK and APKTOOL_JAR for the rebuild/reopen test");
        QTemporaryDir dir;
        const QString root = dir.path() + "/decoded";
        const QString frameworks = dir.path() + "/frameworks";
        const auto apktool = [&jar](const QStringList &args) {
            QProcess process;
            process.start("java", QStringList({ "-jar", jar }) + args);
            if (!process.waitForStarted(10000)) return false;
            if (!process.waitForFinished(180000)) {
                process.kill();
                process.waitForFinished();
                return false;
            }
            if (process.exitCode() != 0) qWarning().noquote() << process.readAllStandardError();
            return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        };
        QVERIFY(apktool({ "d", input, "-s", "--res-resolve-mode", "greedy", "-o", root, "-p", frameworks }));
        Apk::File apk(root);
        Icon *source = nullptr;
        for (Icon *icon : adaptiveIcons(apk.iconsModel)) {
            if (icon->getEntryRole() == Icon::EntryApplicationIcon && icon->getType() == Icon::Xxxhdpi) source = icon;
        }
        QVERIFY(source);
        QMap<QString, QByteArray> legacyOriginal;
        for (Icon *icon : apk.iconsModel.legacyAlternatives(source)) {
            legacyOriginal[QDir(root).relativeFilePath(icon->getFilename())] = read(icon->getFilename());
        }
        QVERIFY(source->replace(solid(Qt::blue)));
        apk.iconsModel.updateAdaptiveFamily(source);
        apk.saveIcons();
        const QString packed = dir.path() + "/packed.apk";
        QVERIFY(apktool({ "b", root, "-o", packed, "-p", frameworks }));
        const QString reopenedPath = dir.path() + "/reopened";
        QVERIFY(apktool({ "d", packed, "-s", "--res-resolve-mode", "greedy", "-o", reopenedPath, "-p", frameworks }));
        Apk::File reopened(reopenedPath);
        Icon *reopenedSource = nullptr;
        for (Icon *icon : adaptiveIcons(reopened.iconsModel)) {
            if (icon->getEntryRole() == Icon::EntryApplicationIcon && icon->getType() == Icon::Xxxhdpi) reopenedSource = icon;
        }
        QVERIFY(reopenedSource);
        QCOMPARE(reopenedSource->getPixmap().toImage().pixelColor(96, 96), QColor(Qt::blue));
        QCOMPARE(apk.iconsModel.legacyAlternatives(source).size(), 5);
        for (auto it = legacyOriginal.cbegin(); it != legacyOriginal.cend(); ++it) {
            QCOMPARE(read(reopenedPath + '/' + it.key()), it.value());
        }
        // Exercise the opt-in path on the rebuilt APK, including WebP output.
        QVERIFY(reopenedSource->replace(solid(Qt::red)));
        reopened.iconsModel.updateAdaptiveFamily(reopenedSource, true);
        reopened.saveIcons();
        QVERIFY(apktool({ "b", reopenedPath, "-o", dir.path() + "/legacy-updated.apk", "-p", frameworks }));
        for (Icon *icon : reopened.iconsModel.legacyAlternatives(reopenedSource)) {
            const QColor pixel = QImage(icon->getFilename()).pixelColor(0, 0);
            QVERIFY(pixel.red() > 245 && pixel.green() < 10 && pixel.blue() < 10);
        }
    }
};

QTEST_MAIN(TestIconVariants)
#include "test_iconvariants.moc"
