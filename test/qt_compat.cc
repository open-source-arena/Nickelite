#include "../src/nickelmenu.cc"
#include "../src/action.h"
#include "../src/util.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <cassert>
#include <cstring>

extern "C" void nh_log(const char *, ...) {}
extern "C" bool nh_delete_file(const char *) { std::abort(); }
extern "C" bool nh_delete_dir(const char *) { std::abort(); }

static MainNavView *expected_view;
static QWidget *expected_parent;
static unsigned constructor_calls;
static MainNavView *original_constructor(MainNavView *view, QWidget *parent) {
    assert(view == expected_view && parent == expected_parent);
    ++constructor_calls;
    return reinterpret_cast<MainNavView *>(uintptr_t(0x3000));
}

int main() {
    for (const char *provider : {"dropbox", "gdrive"}) {
        const QString arg = QStringLiteral("library:") + provider;
        assert(!nm_action_nickel_open(arg.toUtf8().constData()));
        const char *error = nm_err_peek();
        const char *expected = "Dropbox and Google Drive actions are disabled on Qt 6 firmware after a reported device panic";
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
}
