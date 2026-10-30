#include "../src/nickelmenu.cc"
#include "../src/action.h"
#include "../src/util.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QEventLoop>
#include <QMouseEvent>
#include <QTemporaryFile>
#include <QTimer>
#include <cassert>
#include <cstring>

extern "C" void nh_log(const char *, ...) {}
extern "C" bool nh_delete_file(const char *) { std::abort(); }
extern "C" bool nh_delete_dir(const char *) { std::abort(); }

static MainNavView *expected_view;
static QWidget *expected_parent;
static unsigned constructor_calls;
static const void *n365_device = reinterpret_cast<const void *>(uintptr_t(0x4000));
static bool original_dropbox_support;
static bool original_gdrive_support;
static bool test_model_support(const void *device) { return device == n365_device; }
static bool test_original_dropbox_support(const void *) { return original_dropbox_support; }
static bool test_original_gdrive_support(const void *) { return original_gdrive_support; }
#if NM_QT_MAJOR >= 6
static bool test_feature_disabled(const char *) { return false; }
#endif
static MainNavView *original_constructor(MainNavView *view, QWidget *parent) {
    assert(view == expected_view && parent == expected_parent);
    ++constructor_calls;
    return reinterpret_cast<MainNavView *>(uintptr_t(0x3000));
}

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", "/usr/lib/plugins/platforms");
    QApplication app(argc, argv);
    unsigned child_creations = 0;
    bool type_mismatch = false;
    const QString child_name = QStringLiteral("nmTestController");
    auto create_child = [&]() -> QObject * { ++child_creations; return new QObject; };
    QObject *child = nm_application_owned_child(&app, child_name, "QObject", create_child, &type_mismatch);
    assert(child && child->parent() == &app && child->objectName() == child_name && !type_mismatch);
    assert(nm_application_owned_child(&app, child_name, "QObject", create_child, &type_mismatch) == child);
    assert(child_creations == 1 && !type_mismatch);
    delete child;
    QObject *replacement = nm_application_owned_child(&app, child_name, "QObject", create_child, &type_mismatch);
    assert(replacement && replacement->parent() == &app && child_creations == 2);
    assert(!nm_application_owned_child(&app, child_name, "QWidget", create_child, &type_mismatch));
    assert(type_mismatch && child_creations == 2);
    delete replacement;

    auto tabOverride = nm_parse_tab_override("action=cmd_spawn\nargument=quiet:exec /mnt/onboard/.adds/koreader/koreader.sh\nlabel=KOReader\n");
    assert(tabOverride && tabOverride->action.act == NM_ACTION(cmd_spawn));
    assert(tabOverride->action.arg == tabOverride->argument.data());
    assert(tabOverride->displayLabel == QLatin1String("KOReader"));
    auto noLabelOverride = nm_parse_tab_override("# default label\naction=kfmon_list\nargument=\n");
    assert(noLabelOverride && noLabelOverride->action.act == NM_ACTION(kfmon_list));
    assert(noLabelOverride->displayLabel.isEmpty());
    assert(!nm_parse_tab_override("action=cmd_spawn\naction=cmd_spawn\nargument=x\n"));
    assert(!nm_parse_tab_override("action=not_an_action\nargument=x\n"));
    assert(!nm_parse_tab_override("action=skip\nargument=1\n"));
    assert(!nm_parse_tab_override("action=cmd_spawn\n"));
    assert(!nm_parse_tab_override("unknown=x\naction=cmd_spawn\nargument=x\n"));
    QByteArray nulArgument("action=dbg_toast\nargument=x");
    nulArgument.append('\0');
    nulArgument += "y\n";
    assert(!nm_parse_tab_override(nulArgument));

#if NM_QT_MAJOR >= 6
    QWidget tabButton;
    int tapCalls = 0;
    int longPressCalls = 0;
    NmMainTabGesture tabGesture(&tabButton, [&tapCalls] { ++tapCalls; }, [&longPressCalls] { ++longPressCalls; }, 40);
    tabButton.show();
    QMouseEvent tapPress(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier, Qt::MouseEventNotSynthesized);
    QMouseEvent tapRelease(QEvent::MouseButtonRelease, QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier, Qt::MouseEventNotSynthesized);
    QApplication::sendEvent(&tabButton, &tapPress);
    QApplication::sendEvent(&tabButton, &tapRelease);
    tabGesture.handleTap();
    assert(tapCalls == 1 && longPressCalls == 0);

    QMouseEvent holdPress(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier, Qt::MouseEventNotSynthesized);
    QApplication::sendEvent(&tabButton, &holdPress);
    QEventLoop holdLoop;
    QTimer::singleShot(60, &holdLoop, &QEventLoop::quit);
    holdLoop.exec();
    assert(longPressCalls == 1);
    QMouseEvent holdRelease(QEvent::MouseButtonRelease, QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier, Qt::MouseEventNotSynthesized);
    QApplication::sendEvent(&tabButton, &holdRelease);
    tabGesture.handleTap();
    assert(tapCalls == 1 && longPressCalls == 1);

    QMouseEvent movePress(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier, Qt::MouseEventNotSynthesized);
    QApplication::sendEvent(&tabButton, &movePress);
    QMouseEvent move(QEvent::MouseMove, QPointF(50, 10), QPointF(50, 10), QPointF(50, 10),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier, Qt::MouseEventNotSynthesized);
    QApplication::sendEvent(&tabButton, &move);
    QEventLoop movedLoop;
    QTimer::singleShot(60, &movedLoop, &QEventLoop::quit);
    movedLoop.exec();
    assert(longPressCalls == 1);

    const auto malformedOverride = nm_parse_tab_override("action=missing\nargument=x\n");
    assert(!malformedOverride);
    int fallbackMenuCalls = 0;
    NmMainTabGesture fallbackGesture(&tabButton, [&fallbackMenuCalls] { ++fallbackMenuCalls; }, [] {}, 40);
    fallbackGesture.handleTap();
    assert(fallbackMenuCalls == 1);

    nm_err_set(nullptr);
    assert(!nm_config_parse(nullptr) && !nm_err_peek());
    size_t emptyCount = 1;
    auto **emptyItems = nm_config_get_menu(nullptr, &emptyCount);
    assert(emptyItems && emptyCount == 0);
    free(emptyItems);

    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(nickel_open), "library:dropbox"), "dropbox"));
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(nickel_open), "library:gdrive"), "gdrive"));
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(nickel_open), "library:authors"), "navigation"));
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(nickel_setting), "toggle : dark_mode"), "dark_mode"));
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(nickel_setting), "enable:screenshots"), "screenshots"));
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(nickel_misc), "force_usb_connection"), "usb"));
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(nickel_misc), "rescan_books"), "book_scan"));
#if NM_QT_MAJOR >= 6
    char tabActionArg[] = "quiet:exec /mnt/onboard/.adds/koreader/koreader.sh";
    nm_menu_action_t tabAction = {tabActionArg, true, false, NM_ACTION(cmd_spawn), nullptr};
    const char *blockedFeature = nullptr;
    assert(nm_action_permission_blocked(&tabAction, false, test_feature_disabled, &blockedFeature));
    assert(!std::strcmp(blockedFeature, "commands"));
    assert(!nm_action_permission_blocked(&tabAction, true, test_feature_disabled, &blockedFeature));
#endif
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(kfmon), "watch.png"), "kfmon"));
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(kfmon_id), "watch-id"), "kfmon"));
    assert(!std::strcmp(nm_feature_for_action(NM_ACTION(kfmon_list), ""), "kfmon"));
    assert(!std::strcmp(nm_ui_action_feature(QStringLiteral("kfmon"), "watch.png"), "kfmon"));
    assert(!std::strcmp(nm_ui_action_feature(QStringLiteral("kfmon_id"), "watch-id"), "kfmon"));
    assert(!std::strcmp(nm_ui_action_feature(QStringLiteral("kfmon_generator"), "gui"), "kfmon"));
    assert(!std::strcmp(nm_ui_action_feature(QStringLiteral("kfmon_list"), ""), "kfmon"));
    assert(nm_config_is_managed_kfmon_shortcut(NM_CONFIG_DIR "/zz-nm-settings.conf", "generator:main:kfmon:gui"));
    assert(!nm_config_is_managed_kfmon_shortcut(NM_CONFIG_DIR "/custom.conf", "generator:main:kfmon:gui"));
    assert(!nm_config_is_managed_kfmon_shortcut(NM_CONFIG_DIR "/zz-nm-settings.conf", "generator:reader:kfmon:gui"));
    assert(!nm_action_available("kfmon", false) && !nm_action_available("kfmon_id", false)
        && !nm_action_available("kfmon_list", false));
    assert(nm_action_available("kfmon", true) && nm_action_available("kfmon_id", true)
        && nm_action_available("kfmon_list", true));
    assert(nm_action_available("dbg_msg", false));
    assert(!nm_feature_for_action(NM_ACTION(uninstall), ""));
    assert(!nm_feature_for_action(NM_ACTION(skip), "1"));

    const QRect systemViewArea(0, 0, 1000, 1400);
    assert(nm_system_view_covers(QRect(0, 0, 800, 1120), systemViewArea));
    assert(!nm_system_view_covers(QRect(0, 0, 1000, 1000), systemViewArea));
    assert(nm_system_transition_event(QEvent::ApplicationDeactivate));
    assert(nm_system_transition_event(QEvent::ApplicationStateChange));
    assert(!nm_system_transition_event(QEvent::Show));
    assert(nm_system_view_presented_event(QEvent::Show, false));
    assert(nm_system_view_presented_event(QEvent::ShowToParent, false));
    assert(nm_system_view_presented_event(QEvent::WindowActivate, true));
    assert(!nm_system_view_presented_event(QEvent::WindowActivate, false));
    assert(!nm_system_view_presented_event(QEvent::Hide, true));
    QWidget picker;
    picker.setGeometry(0, 0, 1000, 1400);
    new NmSystemDialogGuard(&picker, true, false);
    picker.show();
    app.processEvents();
    assert(picker.isVisible());
    QWidget systemView;
    systemView.setGeometry(0, 0, 1000, 1400);
    systemView.show();
    assert(!picker.isVisible());
    app.processEvents();
    assert(!picker.isVisible());
    QEventLoop closeLoop;
    QTimer::singleShot(50, &closeLoop, &QEventLoop::quit);
    closeLoop.exec();
    assert(!picker.isVisible());
    systemView.close();
    QTimer transitionSignal;
    transitionSignal.setSingleShot(true);
    transitionSignal.setInterval(0);
    QEventLoop transitionLoop;
    bool transitionDismissed = false;
    assert(nm_connect_transition_signal(&transitionSignal, SIGNAL(timeout()), &transitionLoop, SLOT(quit())));
    transitionSignal.start();
    transitionLoop.exec();
    transitionDismissed = !transitionSignal.isActive();
    assert(transitionDismissed);

    QWidget lifecyclePicker;
    lifecyclePicker.setGeometry(0, 0, 1000, 1400);
    lifecyclePicker.show();
    app.processEvents();
    assert(lifecyclePicker.isVisible());
    QTimer lifecycleSignal;
    lifecycleSignal.setSingleShot(true);
    lifecycleSignal.setInterval(0);
    QTimer closeTimer;
    closeTimer.setSingleShot(true);
    closeTimer.setInterval(0);
    assert(nm_connect_transition_signal(&lifecycleSignal, SIGNAL(timeout()), &lifecyclePicker, SLOT(hide())));
    assert(nm_connect_transition_signal(&lifecycleSignal, SIGNAL(timeout()), &closeTimer, SLOT(start())));
    QObject::connect(&closeTimer, &QTimer::timeout, &lifecyclePicker, [&lifecyclePicker] {
        nm_queue_native_close(&lifecyclePicker);
    });
    lifecycleSignal.start();
    QEventLoop lifecycleLoop;
    QTimer::singleShot(50, &lifecycleLoop, &QEventLoop::quit);
    lifecycleLoop.exec();
    assert(!lifecyclePicker.isVisible());

    bool foundKfmonPermission = false;
    for (const auto &feature : nm_features)
        if (!std::strcmp(feature.key, "kfmon")) {
            assert(!std::strcmp(feature.label, "KFMon actions"));
            foundKfmonPermission = true;
        }
    assert(foundKfmonPermission);
    bool foundKfmonShortcut = false;
    for (const auto &shortcut : nm_shortcuts)
        if (!std::strcmp(shortcut.label, "KFMon launchers")) {
            assert(!std::strcmp(shortcut.action, "kfmon_list"));
            assert(!std::strcmp(shortcut.argument, ""));
            assert(!std::strcmp(shortcut.permission, "kfmon"));
            foundKfmonShortcut = true;
        }
    assert(foundKfmonShortcut);
    assert(nm_choice_label(QStringLiteral("kfmon")) == QStringLiteral("Launch a KFMon file"));
    assert(nm_choice_label(QStringLiteral("kfmon_id")) == QStringLiteral("Launch a KFMon entry"));
    assert(nm_choice_label(QStringLiteral("kfmon_list")) == QStringLiteral("List KFMon launchers"));
    assert(nm_choice_label(QStringLiteral("kfmon_generator")) == QStringLiteral("Generate KFMon launcher rows"));
    assert(nm_ui_item_inputs_valid(QStringLiteral("kfmon_generator"), QString(), QStringLiteral("gui")));
    assert(!nm_ui_item_inputs_valid(QStringLiteral("kfmon"), QString(), QString()));
    assert(!nm_ui_item_inputs_valid(QStringLiteral("kfmon_generator"), QString(), QStringLiteral("gui\n")));
    assert(nm_ui_item_fields(nm_ui_items_path, QStringLiteral("generator:main:kfmon:gui"))
        == QStringList({QStringLiteral("main"), QStringLiteral("KFMon launchers"), QStringLiteral("kfmon_list"), QString()}));
    assert(nm_ui_item_fields(QStringLiteral(".adds/nm/custom.conf"), QStringLiteral("generator:main:kfmon:gui"))
        == QStringList({QStringLiteral("main"), QStringLiteral("KFMon launchers"), QStringLiteral("kfmon_generator"), QStringLiteral("gui")}));
    assert(nm_ui_item_line({QStringLiteral("settings"), QStringLiteral("-1"), QStringLiteral("main"),
        QStringLiteral("KFMon launchers"), QStringLiteral("kfmon_generator"), QStringLiteral("gui")})
        == QStringLiteral("generator:main:kfmon:gui"));
    assert(nm_ui_item_line({QStringLiteral("settings"), QStringLiteral("-1"), QStringLiteral("main"),
        QStringLiteral("KFMon launchers"), QStringLiteral("kfmon_list"), QString()})
        == QStringLiteral("menu_item:main:KFMon launchers:kfmon_list:"));

    Device_isSpaKoboBW = test_model_support;
    Device_supportsDropbox = test_original_dropbox_support;
    Device_supportsGoogleDrive = test_original_gdrive_support;

    nm_exact_n365_firmware = false;
    assert(!_nm_device_supports_dropbox(n365_device));
    assert(!_nm_device_supports_google_drive(n365_device));

    nm_exact_n365_firmware = true;
    const void *other_device = reinterpret_cast<const void *>(uintptr_t(0x5000));
    assert(!_nm_device_supports_dropbox(other_device));
    assert(!_nm_device_supports_google_drive(other_device));
    assert(_nm_device_supports_dropbox(n365_device));
    assert(_nm_device_supports_google_drive(n365_device));

    original_dropbox_support = true;
    original_gdrive_support = true;
    assert(_nm_device_supports_dropbox(other_device));
    assert(_nm_device_supports_google_drive(other_device));
    nm_exact_n365_firmware = false;

#endif

    for (const char *provider : {"dropbox", "gdrive"}) {
        const QString arg = QStringLiteral("library:") + provider;
        assert(!nm_action_nickel_open(arg.toUtf8().constData()));
        const char *error = nm_err_peek();
        const char *expected = !std::strcmp(provider, "dropbox")
            ? "could not dlsym MoreController::dropbox"
            : "could not dlsym MoreController::gdrive";
        assert(error && !std::strncmp(error, expected, std::strlen(expected)));
        nm_err_set(nullptr);
    }

    // Missing optional symbols take the production hook's early-exit path.
    // No QWidget is accessed before that check, so opaque tokens suffice.
    expected_view = reinterpret_cast<MainNavView *>(uintptr_t(0x1000));
    expected_parent = reinterpret_cast<QWidget *>(uintptr_t(0x2000));
    MainNavView_MainNavView = original_constructor;
    assert(_nm_menu_hook2(expected_view, expected_parent) == reinterpret_cast<MainNavView *>(uintptr_t(0x3000)));
    assert(constructor_calls == 1);

    const QString selection = QStringLiteral("hello world");
    nm_selmenu_argtransform_data_t data{selection};
    char *result = _nm_selmenu_argtransform(&data, "{1|u|}");
    assert(result && !strcmp(result, "hello_world"));
    free(result);

    const QString empty;
    nm_selmenu_argtransform_data_t empty_data{empty};
    assert(!_nm_selmenu_argtransform(&empty_data, "{1|x|}"));
    assert(nm_err_peek());
    nm_err_set(nullptr);

    QString controls = QStringLiteral("quote\" slash\\");
    for (int i = 0; i < 0x20; ++i) controls += QChar(i);
    nm_selmenu_argtransform_data_t controls_data{controls};
    result = _nm_selmenu_argtransform(&controls_data, "{\"value\":\"{1||\"}\"}");
    assert(result);
    const auto json = QJsonDocument::fromJson(result);
    assert(!json.isNull() && json.object().value(QStringLiteral("value")).toString() == controls);
    free(result);

#if NM_QT_MAJOR >= 6
    if (dlopen("libnickel.so.1.0.0", RTLD_LAZY | RTLD_GLOBAL)) {
        const bool supported = !qEnvironmentVariableIsSet("NM_TEST_UNSUPPORTED_FIRMWARE");
        assert(nm_is_exact_target_firmware() == supported);
        if (!supported) return 0;
        void *deviceAddress = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
        Dl_info info{};
        assert(deviceAddress && dladdr(deviceAddress, &info) && info.dli_fname);
        QFile firmware(QString::fromLocal8Bit(info.dli_fname));
        assert(firmware.open(QIODevice::ReadOnly));
        assert(nm_hash_exact_target_libnickel(firmware));
        const nm_cloud_firmware_signature *signature = nullptr;
        for (const auto &candidate : nm_cloud_firmware_signatures)
            if (nm_hash_target_libnickel(firmware, candidate)) signature = &candidate;
        assert(signature);
        const auto enabled = QByteArray::fromHex("00b5012000bd");

        QTemporaryFile patchedFirmware;
        assert(patchedFirmware.open() && firmware.seek(0));
        while (!firmware.atEnd()) {
            const QByteArray chunk = firmware.read(16 * 1024);
            assert(!chunk.isEmpty() && patchedFirmware.write(chunk) == chunk.size());
        }
        assert(patchedFirmware.seek(signature->dropboxOffset));
        assert(patchedFirmware.write(enabled) == enabled.size());
        assert(patchedFirmware.seek(signature->googleDriveOffset));
        assert(patchedFirmware.write(enabled) == enabled.size());
        assert(patchedFirmware.flush() && nm_hash_exact_target_libnickel(patchedFirmware));
        assert(patchedFirmware.seek(0));
        const char changed = static_cast<char>(patchedFirmware.read(1).at(0) ^ 1);
        assert(patchedFirmware.seek(0) && patchedFirmware.write(&changed, 1) == 1);
        assert(patchedFirmware.flush() && !nm_hash_exact_target_libnickel(patchedFirmware));
    }
#endif
}
