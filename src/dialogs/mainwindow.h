///
/// \file
/// This file contains the main window declaration.
///

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QListView>
#include <QStackedWidget>
#include <QTableView>
#include <QComboBox>
#include <QMessageBox>
#include <QActionGroup>
#include <QTranslator>
#include <QCloseEvent>
#include "about.h"
#include "apkmanager.h"
#include "busyindicator.h"
#include "cloud.h"
#include "dialogs.h"
#include "drawarea.h"
#include "effectsdialog.h"
#include "devicemodel.h"
#include "iconsproxy.h"
#include "keymanager.h"
#include "recent.h"
#include "tooldialog.h"
#include "updater.h"

class QMenuBar;

///
/// Main window class.
/// This class describes the main "APK Icon Editor Reborn" window.
///

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:

    // APK:

    /// Opens APK with the specified \c filename.
    /// Displays the "Open APK" dialog if the \c filename is not specified.
    bool apk_open(QString filename = QString());

    /// Saves APK to the specified \c filename.
    /// Displays the "Save APK" dialog if the \c filename is not specified.
    bool apk_save(QString filename = QString());

    /// Opens the directory containing the unpacked APK files.
    void apk_explore();

    /// Closes the current APK.
    void apk_close();

    // Icons:

    /// Loads icon from the file with the given \c filename.
    /// Displays the "Open Icon" dialog if the \c filename is not specified.
    bool icon_open(QString filename = QString());

    /// Saves the current icon to the file with the specified \c filename.
    /// Displays the "Save Icon" dialog if the \c filename is not specified.
    bool icon_save(QString filename = QString());

    /// Resizes the current icon to the specified \c size.
    /// Displays the "Resize Icon" dialog if the \c size is not specified.
    bool icon_resize(QSize size = QSize());

    /// Scales the current icon to the appropriate size.
    bool icon_scale();

    /// Reverts the original APK icon.
    bool icon_revert();

    // Actions:

    /// Displays the icon with the specified \c index in the icon preview widget.
    void setCurrentIcon(const QModelIndex &index);
    void setLanguage(QString lang); ///< Sets the GUI language to \c lang.
    void setTheme(QString theme);   ///< Sets the GUI theme mode.
    bool setPreviewColor();         ///< Displays background color selection dialog.
    void showEffectsDialog();       ///< Displays "Effects" dialog.

    void associate() const;         ///< Sets "APK Icon Editor Reborn" as the default application for \c apk files (Windows only).
    void browseSite() const;        ///< Opens website URL in the default browser.
    void browseBugs() const;        ///< Opens bugs webpage in the default browser.
    void browseTranslate() const;   ///< Opens Crowdin URL in the default browser.
    void browseFaq();               ///< Opens FAQ text document.
    void openLogFile() const;       ///< Opens log file.
    void openLogPath() const;       ///< Opens log directory.

signals:
    /// This signal is emmitted after the Java and Apktool versions check was performed.
    void reqsChecked(QString jre, QString jdk, QString apktool);

private slots:

    // APK:

    /// Handles the packed APK.
    /// \param apk       Object representing the packed APK.
    /// \param isSuccess Contains \c false if the APK is packed with warnings.
    /// \param text      Message text.
    /// \param details   Descriptive message text.
    void apk_packed(Apk::File *apk, bool isSuccess, QString text, QString details);

    /// Handles the unpacked APK.
    /// \param apk Object representing the unpacked APK.
    void apk_unpacked(Apk::File *apk);

    /// Marks the specified \c filename as currently active.
    void setActiveApk(QString filename);

    // Settings:

    void settings_load();  ///< Loads application settings from INI.
    void settings_reset(); ///< Resets application settings to default.

    // Recent:

    void recent_add(QString filename); ///< Adds the specified \c filename to the recent list.
    void recent_update();              ///< Updates the recent menu.
    void recent_clear();               ///< Clears the list of recently opened APK files.

    // Actions:

    void removeIcon();              ///< Removes the current icon file and model index.
    void cloneIcons();              ///< Clones the current icon for to all sizes.
    void applyAppName();            ///< Applies the global application name to all translations.
    void enableUpload(bool enable); ///< Enables or disables upload to cloud services.
    void checkUpdates();            ///< Checks updates and shows the manual check result.

    // Dialogs:

    void donate();       ///< Displays donation dialog.
    void authCloud();    ///< Displays cloud authentication input dialog.

    /// Displays the "New version available" dialog.
    /// \param version Number representing the new version.
    bool newVersion(QString version);
    void updateChecked(QString version, bool updateAvailable, QString error);

    /// Displays a message.
    /// \param title   Message brief title.
    /// \param text    Message brief text.
    /// \param details Message detailed text.
    /// \param type    Message type (information, warning...).
    void message(QString title,
                 QString text,
                 QString details = QString(),
                 QMessageBox::Icon type = QMessageBox::Information);

    /// Displays success message.
    /// \param title   Message brief title.
    /// \param text    Message brief text.
    /// \param details Message detailed text.
    void success(QString title, QString text, QString details = QString());

    /// Displays warning message.
    /// \param title   Message brief title.
    /// \param text    Message brief text.
    /// \param details Message detailed text.
    void warning(QString title, QString text, QString details = QString());

    /// Displays error message.
    /// \param title   Message brief title.
    /// \param text    Message brief text.
    /// \param details Message detailed text.
    void error(QString title, QString text, QString details = QString());

protected:
    bool eventFilter(QObject *object, QEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);
    void closeEvent(QCloseEvent *event);
#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result);
#else
    bool nativeEvent(const QByteArray &eventType, void *message, long *result);
#endif
#endif

private:
    void init_core();      ///< Initializes base objects.
    void init_gui();       ///< Initializes windows and widgets.
    void init_languages(); ///< Initializes available languages.
    void init_devices();   ///< Initializes available device presets.
    void init_slots();     ///< Initializes signals/slots.

    void checkReqs();      ///< Checks Java and Apktool versions.
    bool resetApktool();   ///< Removes the Apktool "1.apk" framework file.
    void setInitialSize(); ///< Sets the initial sizes for the window and splitter.
    void setIconsLoading(bool loading); ///< Shows or hides the icon list loading state.
    Qt::Edges framelessResizeEdgesAt(const QPoint &globalPos) const; ///< Returns frameless window resize edges.
    void updateFramelessResizeCursor(const QPoint &globalPos); ///< Updates cursor over frameless resize edges.
    void clearFramelessResizeCursor(); ///< Restores cursor after leaving frameless resize edges.
    bool startFramelessResize(const QPoint &globalPos); ///< Starts system resize for frameless windows.
    bool confirmExit();    ///< Displays the exit confirmation dialog.
    void applyTheme(QString theme); ///< Applies the selected GUI theme.

    /// Uploads the specified file to a cloud service.
    /// \param uploader Cloud uploader object.
    /// \param filename Name of the file to upload.
    void upload(Cloud *uploader, QString filename);

    // Dialogs:

    About *about;
    EffectsDialog *effects;
    ToolDialog *toolDialog;
    KeyManager *keyManager;
    ProgressDialog *loadingDialog;
    ProgressDialog *uploadDialog;

    // MVC:

    QListView *listIcons;
    QStackedWidget *iconsStack;
    QLabel *iconsLoadingLabel;
    BusyIndicator *iconsLoadingIndicator;
    QTableView *tableManifest;
    QTableView *tableTitles;

    DeviceModel deviceModel;
    IconsProxy *iconsProxy;

    // Widgets:

    QSplitter *splitter;
    QWidget *customTitleBar;
    QMenuBar *mainMenuBar;
    DrawArea *drawArea;
    QTabWidget *tabs;
    QWidget *tabIcons;
    QWidget *tabTranslations;
    QWidget *tabProperties;
    QLabel *devicesLabel;
    QComboBox *devices;
    QPushButton *btnApplyAppName;
    QToolButton *btnAddIcon;
    QToolButton *btnRemoveIcon;
    QToolButton *btnOpenIcon;
    QToolButton *btnSaveIcon;
    QToolButton *btnScaleIcon;
    QToolButton *btnResizeIcon;
    QToolButton *btnRevertIcon;
    QToolButton *btnEffectIcon;
    QToolButton *btnCloneIcons;
    QCheckBox *checkDropbox;
    QCheckBox *checkGDrive;
    QCheckBox *checkOneDrive;
    QCheckBox *checkUpload;
    QPushButton *btnPack;

    // Menus:

    QMenu *menuFile;
    QMenu *menuIcon;
    QMenu *menuIconAdd;
    QMenu *menuView;
    QMenu *menuAdaptivePreviewMode;
    QMenu *menuPreviewShape;
    QMenu *menuSett;
    QMenu *menuHelp;
    QMenu *menuRecent;
    QMenu *menuLang;
    QMenu *menuTheme;
    QMenu *menuLogs;
    QToolButton *btnDonate;

    // Actions:

    QAction *actApkOpen;
    QAction *actApkSave;
    QAction *actApkExplore;
    QAction *actApkClose;
    QAction *actExit;
    QAction *actRecentClear;
    QAction *actNoRecent;
    QActionGroup *iconActions;
    QAction *actIconOpen;
    QAction *actIconSave;
    QAction *actIconRemove;
    QAction *actIconScale;
    QAction *actIconResize;
    QAction *actIconRevert;
    QAction *actIconBackground;
    QAction *actIconEffect;
    QAction *actIconClone;
    QAction *actAddIconLdpi;
    QAction *actAddIconMdpi;
    QAction *actAddIconHdpi;
    QAction *actAddIconTvdpi;
    QAction *actAddIconXhdpi;
    QAction *actAddIconXxhdpi;
    QAction *actAddIconXxxhdpi;
    QAction *actAddIconTv;
    QAction *actViewActivities;
    QActionGroup *adaptivePreviewModeActions;
    QAction *actAdaptivePreviewNormal;
    QAction *actAdaptivePreviewThemed;
    QActionGroup *previewShapeActions;
    QAction *actPreviewShapeNone;
    QAction *actPreviewShapeCircle;
    QAction *actPreviewShapeRoundedSquare;
    QAction *actPreviewShapeSquircle;
    QAction *actPacking;
    QAction *actKeys;
    QAction *actTranslate;
    QActionGroup *themeActions;
    QAction *actThemeSystem;
    QAction *actThemeLight;
    QAction *actThemeDark;
    QAction *actAssoc;
    QAction *actReset;
    QAction *actAutoUpdate;
    QAction *actFaq;
    QAction *actWebsite;
    QAction *actReport;
    QAction *actDonate;
    QAction *actLogFile;
    QAction *actLogPath;
    QAction *actUpdate;
    QAction *actAboutQt;
    QAction *actAbout;

    // Base:

    Apk::File *apk;
    ApkManager *apkManager;
    Recent *recent;
    bool manualUpdateCheck;
    Updater *updater;
    Dropbox *dropbox;
    GoogleDrive *gdrive;
    OneDrive *onedrive;

    // Other:

    QTranslator *translator;
    QTranslator *translatorQt;
    QString currentApk;
    QString currentLang;
    QString currentTheme;
    QString currentPath;
    bool framelessResizeCursorActive;
};

#endif // MAINWINDOW_H
