#include "../src/nickelmenu.cc"
#include "../src/action.h"
#include "../src/util.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QTemporaryFile>
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
static MainNavView *original_constructor(MainNavView *view, QWidget *parent) {
    assert(view == expected_view && parent == expected_parent);
    ++constructor_calls;
    return reinterpret_cast<MainNavView *>(uintptr_t(0x3000));
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
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

#if NM_QT_MAJOR >= 6
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
    assert(!nm_feature_for_action(NM_ACTION(uninstall), ""));
    assert(!nm_feature_for_action(NM_ACTION(skip), "1"));

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
