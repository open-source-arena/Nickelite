#include <QAction>
#include <QApplication>
#if NM_QT_MAJOR >= 6
#include <QByteArrayView>
#endif
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QLayout>
#include <QMenu>
#include <QMetaProperty>
#include <QPushButton>
#include <QRegularExpression>
#include <QString>
#include <QUrl>
#include <QWidget>
#include <QWidgetAction>
#if NM_QT_MAJOR >= 6
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMap>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPair>
#include <QSaveFile>
#include <QScreen>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSettings>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QTouchEvent>
#include <QVBoxLayout>
#include <QWheelEvent>
#endif

#include <algorithm>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <functional>
#include <memory>

#include <NickelHook.h>
#include "native.h"
#include "icon_data.h"

#include "action.h"
#include "config.h"
#include "nickelmenu.h"
#include "util.h"

#if NM_QT_MAJOR >= 6
static void nm_show_dialog(QString const &title, QString const &body);

struct nm_feature {
    const char *key;
    const char *label;
};

static constexpr nm_feature nm_features[] = {
    {"commands", "Commands and output"},
    {"kfmon", "KFMon launchers"},
    {"settings", "Device settings"},
    {"dark_mode", "Dark mode"},
    {"screenshots", "Screenshots"},
    {"usb", "USB connection"},
    {"browser", "Web browser"},
    {"navigation", "Kobo views"},
    {"dropbox", "Dropbox"},
    {"gdrive", "Google Drive"},
    {"wifi", "Wi-Fi"},
    {"orientation", "Orientation"},
    {"power", "Power controls"},
    {"book_scan", "Book rescan"},
};

struct nm_shortcut {
    const char *label;
    const char *action;
    const char *argument;
    const char *permission;
};

static constexpr nm_shortcut nm_shortcuts[] = {
    {"Dark mode", "nickel_setting", "toggle:dark_mode", "dark_mode"},
    {"Screenshot", "nickel_screenshot", "capture", "screenshots"},
    {"Rotate screen", "nickel_orientation", "swap", "orientation"},
    {"Dropbox", "nickel_open", "library:dropbox", "dropbox"},
    {"Google Drive", "nickel_open", "library:gdrive", "gdrive"},
    {"Wi-Fi", "nickel_wifi", "toggle", "wifi"},
    {"Sleep", "power", "sleep", "power"},
    {"Restart", "power", "reboot", "power"},
    {"Power off", "power", "shutdown", "power"},
};

static const char *nm_feature_for_action(nm_action_fn_t action, const char *argument) {
    if (action == NM_ACTION(cmd_spawn) || action == NM_ACTION(cmd_output)) return "commands";
    if (action == NM_ACTION(kfmon) || action == NM_ACTION(kfmon_id)) return "kfmon";
    if (action == NM_ACTION(nickel_screenshot)) return "screenshots";
    if (action == NM_ACTION(nickel_wifi)) return "wifi";
    if (action == NM_ACTION(nickel_browser)) return "browser";
    if (action == NM_ACTION(nickel_orientation)) return "orientation";
    if (action == NM_ACTION(power)) return "power";
    const QString arg = QString::fromUtf8(argument ? argument : "").trimmed();
    if (action == NM_ACTION(nickel_setting)) {
        const QString setting = arg.section(':', 1).trimmed();
        if (setting == QLatin1String("dark_mode")) return "dark_mode";
        if (setting == QLatin1String("screenshots")) return "screenshots";
        if (setting == QLatin1String("auto_usb_gadget")) return "usb";
        return "settings";
    }
    if (action == NM_ACTION(nickel_open)) {
        if (arg == QLatin1String("library:dropbox")) return "dropbox";
        if (arg == QLatin1String("library:gdrive")) return "gdrive";
        return "navigation";
    }
    if (action == NM_ACTION(nickel_misc)) {
        if (arg == QLatin1String("force_usb_connection") || arg == QLatin1String("rescan_books_full")) return "usb";
        if (arg == QLatin1String("rescan_books")) return "book_scan";
        return "navigation";
    }
    return nullptr; // diagnostics, chaining, and uninstall remain available for recovery
}

static bool nm_feature_enabled(const char *key) {
    QSettings settings(QLatin1String(NM_CONFIG_DIR "/.features.ini"), QSettings::IniFormat);
    return settings.value(QLatin1String(key), true).toBool();
}

static const char *nm_feature_label(const char *key) {
    for (const auto &feature : nm_features)
        if (!std::strcmp(feature.key, key)) return feature.label;
    return key;
}

static bool nm_item_permissions_enabled(const nm_menu_item_t *item) {
    for (const nm_menu_action_t *action = item->action; action; action = action->next) {
        const char *feature = nm_feature_for_action(action->act, action->arg);
        if (feature && !nm_feature_enabled(feature)) return false;
    }
    return true;
}

static void nm_set_menu_widget_enabled(QWidget *widget, bool enabled) {
    widget->setEnabled(enabled);
    if (!enabled) {
        auto *dim = new QGraphicsOpacityEffect(widget);
        dim->setOpacity(0.45);
        widget->setGraphicsEffect(dim);
    }
}

static const QString nm_ui_items_path = QLatin1String(NM_CONFIG_DIR "/zz-nm-settings.conf");

static bool nm_config_valid() {
    nm_config_file_t *files = nm_config_files();
    if (nm_err_peek()) return false;
    nm_config_t *config = nm_config_parse(files);
    nm_config_files_free(files);
    if (!config) return false;
    nm_config_free(config);
    return true;
}

static QStringList nm_menu_fields(const QString &line) {
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QLatin1String("menu_item:"))) return {};
    const int p1 = trimmed.indexOf(':', 10), p2 = trimmed.indexOf(':', p1 + 1), p3 = trimmed.indexOf(':', p2 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0) return {};
    return {trimmed.mid(10, p1 - 10).trimmed(), trimmed.mid(p1 + 1, p2 - p1 - 1).trimmed(),
            trimmed.mid(p2 + 1, p3 - p2 - 1).trimmed(), trimmed.mid(p3 + 1).trimmed()};
}

static QString nm_ui_item_text(const QStringList &fields);

static bool nm_read_ui_items(QListWidget *list, QMap<QString, QByteArray> *snapshots) {
    if (!nm_config_valid()) return false;
    const QDir dir(QLatin1String(NM_CONFIG_DIR));
    for (const QFileInfo &info : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString name = info.fileName();
        if (info.isSymLink() || name.endsWith('~') || name == QLatin1String("desktop.ini")
            || name.compare(QLatin1String("Thumbs.db"), Qt::CaseInsensitive) == 0
            || name.endsWith(QLatin1String(".swp")) || name.endsWith(QLatin1String(".swo"))) continue;
        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QByteArray data = file.readAll();
        snapshots->insert(info.filePath(), data);
        const QStringList lines = QString::fromUtf8(data).split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            const QStringList parsed = nm_menu_fields(lines[i]);
            if (parsed.isEmpty()) continue;
            const QStringList fields{info.filePath(), QString::number(i), parsed[0], parsed[1], parsed[2], parsed[3]};
            auto *item = new QListWidgetItem(nm_ui_item_text(fields), list);
            item->setData(Qt::UserRole, fields);
        }
    }
    return true;
}

static bool nm_write_file(const QString &path, const QByteArray &data) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

static bool nm_write_ui_items(QListWidget *list, const QMap<QString, QByteArray> &snapshots) {
    QMap<QString, QByteArray> changed;
    for (auto it = snapshots.cbegin(); it != snapshots.cend(); ++it) {
        QFile current(it.key());
        if (!current.open(QIODevice::ReadOnly) || current.readAll() != it.value()) return false;
        QStringList lines = QString::fromUtf8(it.value()).split('\n');
        QMap<int, QStringList> blocks;
        for (int line = 0; line < lines.size(); ++line) {
            if (nm_menu_fields(lines[line]).isEmpty()) continue;
            const int start = line;
            QStringList block{lines[line]};
            while (line + 1 < lines.size() && lines[line + 1].trimmed().startsWith(QLatin1String("chain_")))
                block << lines[++line];
            blocks.insert(start, block);
        }
        QStringList ordered;
        for (int n = 0; n < list->count(); ++n) {
            const QStringList fields = list->item(n)->data(Qt::UserRole).toStringList();
            if (fields[0] != it.key()) continue;
            QStringList block = blocks.value(fields[1].toInt());
            if (block.isEmpty()) block << QString();
            block[0] = QLatin1String("menu_item:") + fields.mid(2).join(':');
            ordered << block;
        }
        QStringList result;
        int slot = 0;
        for (int line = 0; line < lines.size(); ++line) {
            if (!blocks.contains(line)) { result << lines[line]; continue; }
            if (slot < ordered.size()) result << ordered[slot++];
            line += blocks[line].size() - 1;
        }
        while (slot < ordered.size()) result << ordered[slot++];
        if (result != lines) changed.insert(it.key(), result.join('\n').toUtf8());
    }
    if (!snapshots.contains(nm_ui_items_path)) {
        QStringList added;
        for (int n = 0; n < list->count(); ++n) {
            const QStringList fields = list->item(n)->data(Qt::UserRole).toStringList();
            if (fields[0] == nm_ui_items_path) added << QLatin1String("menu_item:") + fields.mid(2).join(':');
        }
        if (!added.isEmpty()) changed.insert(nm_ui_items_path, (added.join('\n') + '\n').toUtf8());
    }
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        if (QFile::exists(it.key()) && !snapshots.contains(it.key())) return false;
    QStringList written;
    for (auto it = changed.cbegin(); it != changed.cend(); ++it) {
        if (!nm_write_file(it.key(), it.value())) break;
        written << it.key();
    }
    if (written.size() == changed.size() && nm_config_valid()) return true;
    for (const QString &path : written) {
        if (snapshots.contains(path)) nm_write_file(path, snapshots[path]);
        else QFile::remove(path);
    }
    return false;
}

static void nm_confirm_discard(QWidget *parent, std::function<void()> discard,
                               std::function<void()> stay) {
    auto ctor = reinterpret_cast<void (*)(QDialog *, QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN18ConfirmationDialogC2EP7QWidget"));
    auto setTitle = reinterpret_cast<void (*)(QDialog *, const QString &)>(dlsym(RTLD_DEFAULT, "_ZN18ConfirmationDialog8setTitleERK7QString"));
    auto setText = reinterpret_cast<void (*)(QDialog *, const QString &)>(dlsym(RTLD_DEFAULT, "_ZN18ConfirmationDialog7setTextERK7QString"));
    auto acceptText = reinterpret_cast<void (*)(QDialog *, const QString &)>(dlsym(RTLD_DEFAULT, "_ZN18ConfirmationDialog19setAcceptButtonTextERK7QString"));
    auto rejectText = reinterpret_cast<void (*)(QDialog *, const QString &)>(dlsym(RTLD_DEFAULT, "_ZN18ConfirmationDialog19setRejectButtonTextERK7QString"));
    if (ctor && setTitle && setText && acceptText && rejectText && nh_native_type("ConfirmationDialog").isValid()) {
        auto *dialog = reinterpret_cast<QDialog *>(nm_native_storage("ConfirmationDialog", 0));
        if (dialog) {
            ctor(dialog, parent);
            setTitle(dialog, QStringLiteral("Discard changes?"));
            setText(dialog, QStringLiteral("Your NickelMenu changes have not been applied."));
            acceptText(dialog, QStringLiteral("Discard"));
            rejectText(dialog, QStringLiteral("Keep editing"));
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dialog, &QDialog::accepted, parent, std::move(discard));
            QObject::connect(dialog, &QDialog::rejected, parent, std::move(stay));
            dialog->open();
            return;
        }
    }
    if (QMessageBox::question(parent, QStringLiteral("Discard changes?"),
            QStringLiteral("Your NickelMenu changes have not been applied."),
            QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Discard)
        discard();
    else
        stay();
}

class NmCloseGuard final : public QObject {
public:
    NmCloseGuard(QWidget *view, std::function<bool()> dirty, std::function<void()> stay = {})
        : QObject(view), view(view), dirty(std::move(dirty)), stay(std::move(stay)) {
        setObjectName(QStringLiteral("nmCloseGuard"));
        view->installEventFilter(this);
    }
    void allow() { allowed = true; }
    void setStay(std::function<void()> callback) { stay = std::move(callback); }
protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() != QEvent::Close || allowed || !dirty()) return false;
        event->ignore();
        if (confirming) return true;
        confirming = true;
        nm_confirm_discard(view, [this] { allowed = true; view->close(); },
                           [this] { confirming = false; if (stay) stay(); });
        return true;
    }
private:
    QWidget *view;
    std::function<bool()> dirty;
    std::function<void()> stay;
    bool allowed = false;
    bool confirming = false;
};

class NmSystemDialogGuard final : public QObject {
public:
    explicit NmSystemDialogGuard(QWidget *view) : QObject(view), view(view) {
        qApp->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        auto *widget = qobject_cast<QWidget *>(object);
        if (!widget || widget == view || view->isAncestorOf(widget)
            || (!widget->isWindow() && !widget->isModal())) return false;
        const Qt::WindowType type = widget->windowType();
        if (type == Qt::Popup || type == Qt::ToolTip || type == Qt::Tool || widget->inherits("MainWindow")) return false;
        if (event->type() == QEvent::Show && !dialogs.contains(widget)) {
            dialogs.insert(widget);
            QObject::connect(widget, &QObject::destroyed, this, [this, widget] { resume(widget); });
            view->hide();
        } else if (event->type() == QEvent::Hide || event->type() == QEvent::Close) {
            resume(widget);
        }
        return false;
    }
private:
    void resume(QWidget *widget) {
        if (dialogs.remove(widget) && dialogs.isEmpty() && view) view->show();
    }
    QPointer<QWidget> view;
    QSet<QWidget *> dialogs;
};

static QWidget *nm_native_view(const QString &title, QWidget *parent = nullptr) {
    using NativeDialog = QWidget;
    auto ctor = reinterpret_cast<void (*)(NativeDialog *, QWidget *, bool)>(dlsym(RTLD_DEFAULT, "_ZN8N3DialogC2EP7QWidgetb"));
    auto setTitle = reinterpret_cast<void (*)(NativeDialog *, const QString &)>(dlsym(RTLD_DEFAULT, "_ZN8N3Dialog8setTitleERK7QString"));
    auto fullView = reinterpret_cast<void (*)(NativeDialog *)>(dlsym(RTLD_DEFAULT, "_ZN8N3Dialog18enableFullViewModeEv"));
    auto backButton = reinterpret_cast<void (*)(NativeDialog *, bool)>(dlsym(RTLD_DEFAULT, "_ZN8N3Dialog16enableBackButtonEb"));
    auto disableClose = reinterpret_cast<void (*)(NativeDialog *)>(dlsym(RTLD_DEFAULT, "_ZN8N3Dialog18disableCloseButtonEv"));
    auto setContent = dlsym(RTLD_DEFAULT, "_ZN8N3Dialog10setContentEP7QWidget");
    if (!ctor || !setTitle || !fullView || !backButton || !disableClose || !setContent || !nh_native_type("N3Dialog").isValid()) return nullptr;
    auto *view = reinterpret_cast<NativeDialog *>(nm_native_storage("N3Dialog", 0));
    if (!view) return nullptr;
    ctor(view, parent, false);
    setTitle(view, title);
    fullView(view);
    backButton(view, true);
    disableClose(view);
    view->setAttribute(Qt::WA_DeleteOnClose);
    QRect area = QGuiApplication::primaryScreen()->availableGeometry();
    for (QWidget *widget : QApplication::allWidgets()) {
        if (widget->objectName() == QLatin1String("nmButton") && widget->isVisible()) {
            const int navTop = widget->mapToGlobal(QPoint(0, 0)).y();
            if (navTop > area.top() && navTop < area.bottom()) area.setBottom(navTop - 1);
            break;
        }
    }
    view->setGeometry(area);
    auto *shim = new QPushButton(view);
    shim->hide();
    if (QObject::connect(view, SIGNAL(backTapped()), shim, SIGNAL(pressed())))
        QObject::connect(shim, &QPushButton::pressed, view, [view] { view->close(); });
    return view;
}

static void nm_native_set_content(QWidget *view, QWidget *content) {
    auto setContent = reinterpret_cast<void (*)(QWidget *, QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN8N3Dialog10setContentEP7QWidget"));
    setContent(view, content);
}

static QString nm_choice_label(const QString &key) {
    static constexpr struct { const char *key; const char *label; } labels[] = {
        {"main", "NickelMenu tab"}, {"reader", "While reading"},
        {"browser", "Web browser"}, {"library", "My Books filter"},
        {"selection", "Selected text"}, {"selection_search", "Selection search"},
        {"cmd_spawn", "Run a command"}, {"cmd_output", "Show command output"},
        {"dbg_syslog", "Write to system log"}, {"dbg_error", "Show an error"},
        {"dbg_msg", "Show a message"}, {"dbg_toast", "Show a brief notice"},
        {"kfmon", "Launch a KFMon file"}, {"kfmon_id", "Launch a KFMon entry"},
        {"nickel_setting", "Change a Kobo setting"},
        {"nickel_browser", "Open a web page"}, {"nickel_misc", "Kobo utility"},
        {"nickel_open", "Open a Kobo screen"}, {"nickel_wifi", "Control Wi-Fi"},
        {"nickel_orientation", "Change orientation"},
        {"nickel_screenshot", "Take a screenshot"},
        {"power", "Sleep, restart or power off"},
        {"skip", "Skip chained actions"}, {"uninstall", "Uninstall NickelMenu"},
    };
    for (const auto &entry : labels)
        if (key == QLatin1String(entry.key)) return QString::fromLatin1(entry.label);
    return key;
}

static QString nm_ui_item_text(const QStringList &fields) {
    return fields[3];
}

static void nm_group_ui_items(QListWidget *list) {
    const QSignalBlocker block(list->model());
    QListWidgetItem *selected = list->currentItem();
    if (selected && selected->data(Qt::UserRole).toStringList().value(0).isEmpty()) selected = nullptr;
    QList<QListWidgetItem *> items;
    while (list->count()) {
        auto *item = list->takeItem(0);
        if (item->data(Qt::UserRole).toStringList().value(0).isEmpty()) delete item;
        else items << item;
    }
    static const QStringList targets{QStringLiteral("main"), QStringLiteral("reader"),
        QStringLiteral("browser"), QStringLiteral("library"), QStringLiteral("selection"),
        QStringLiteral("selection_search")};
    std::stable_sort(items.begin(), items.end(), [](QListWidgetItem *a, QListWidgetItem *b) {
        const QString left = a->data(Qt::UserRole).toStringList().value(2);
        const QString right = b->data(Qt::UserRole).toStringList().value(2);
        const int li = targets.indexOf(left), ri = targets.indexOf(right);
        if (li != ri) return (li < 0 ? targets.size() : li) < (ri < 0 ? targets.size() : ri);
        return li < 0 && left < right;
    });
    QString previous;
    for (auto *item : items) {
        const QString target = item->data(Qt::UserRole).toStringList().value(2);
        if (target != previous) {
            auto *heading = new QListWidgetItem(target == QLatin1String("main")
                ? QStringLiteral("NickelMenu") : nm_choice_label(target));
            heading->setData(Qt::UserRole, QStringList{QString(), QStringLiteral("-1"), target,
                QString(), QString(), QString()});
            heading->setFlags(Qt::NoItemFlags);
            heading->setBackground(QColor(QStringLiteral("#dedede")));
            QFont font = list->font();
            font.setPixelSize(23);
            font.setBold(true);
            heading->setFont(font);
            list->addItem(heading);
            auto *bar = new QLabel(heading->text(), list);
            bar->setStyleSheet(QStringLiteral("background: #dedede; color: black; padding: 7px 12px; font-size: 23px; font-weight: bold;"));
            bar->setAttribute(Qt::WA_TransparentForMouseEvents);
            list->setItemWidget(heading, bar);
            previous = target;
        }
        list->addItem(item);
    }
    if (selected && items.contains(selected)) list->setCurrentItem(selected);
}

static bool nm_ui_action_blocked(const QString &name, const QString &argument, const QList<QCheckBox *> &boxes) {
    const QByteArray bytes = argument.toUtf8();
    const char *feature = nullptr;
#define X(action, qt5, qt6) if (name == QLatin1String(#action)) feature = nm_feature_for_action(NM_ACTION(action), bytes.constData());
    NM_ACTIONS
#undef X
    if (!feature) return false;
    for (qsizetype i = 0; i < boxes.size(); ++i)
        if (!std::strcmp(nm_features[i].key, feature)) return !boxes[i]->isChecked();
    return false;
}

static bool nm_ui_item_blocked(const QStringList &fields, const QMap<QString, QByteArray> &snapshots,
                               const QList<QCheckBox *> &boxes) {
    if (fields.size() != 6) return false;
    if (nm_ui_action_blocked(fields[4], fields[5], boxes)) return true;
    if (fields[1] == QLatin1String("-1") || !snapshots.contains(fields[0])) return false;
    const QStringList lines = QString::fromUtf8(snapshots.value(fields[0])).split('\n');
    for (int line = fields[1].toInt() + 1; line < lines.size(); ++line) {
        const QString chain = lines[line].trimmed();
        if (!chain.startsWith(QLatin1String("chain_"))) break;
        const int actionStart = chain.indexOf(':');
        const int argumentStart = chain.indexOf(':', actionStart + 1);
        if (actionStart < 0 || argumentStart < 0) continue;
        if (nm_ui_action_blocked(chain.mid(actionStart + 1, argumentStart - actionStart - 1).trimmed(),
                                 chain.mid(argumentStart + 1).trimmed(), boxes)) return true;
    }
    return false;
}

static QListWidgetItem *nm_managed_shortcut(QListWidget *list, const nm_shortcut &shortcut) {
    for (int i = 0; i < list->count(); ++i) {
        auto *item = list->item(i);
        const QStringList fields = item->data(Qt::UserRole).toStringList();
        if (fields.size() == 6 && fields[0] == nm_ui_items_path && fields[2] == QLatin1String("main")
            && fields[3] == QLatin1String(shortcut.label) && fields[4] == QLatin1String(shortcut.action)
            && fields[5] == QLatin1String(shortcut.argument)) return item;
    }
    return nullptr;
}

class NmPagedFrame final : public QObject {
public:
    NmPagedFrame(QWidget *parent, QLabel *indicator, int rowHeight)
        : QObject(parent), indicator(indicator), rowHeight(rowHeight) {
        frame = new QFrame(parent);
        frame->setObjectName(QStringLiteral("nmPagedFrame"));
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto ctor = reinterpret_cast<void (*)(QScrollBar *, QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN14TouchScrollBarC2EP7QWidget"));
        auto setGutterTap = reinterpret_cast<void (*)(QScrollBar *, bool)>(dlsym(RTLD_DEFAULT, "_ZN14TouchScrollBar19setGutterTapEnabledEb"));
        if (ctor && nh_native_type("TouchScrollBar").isValid()) {
            bar = reinterpret_cast<QScrollBar *>(nm_native_storage("TouchScrollBar", 0));
            if (bar) ctor(bar, frame);
        }
        if (!bar) bar = new QScrollBar(Qt::Vertical, frame);
        bar->setOrientation(Qt::Vertical);
        bar->setFixedWidth(24);
        bar->setFocusPolicy(Qt::NoFocus);
        if (setGutterTap && bar->inherits("TouchScrollBar")) setGutterTap(bar, false);
        layout->addWidget(bar);
        QObject::connect(bar, &QScrollBar::valueChanged, this, [this] { display(); });
        frame->installEventFilter(this);
        qApp->installEventFilter(this);
    }

    QWidget *widget() const { return frame; }
    void setRows(const QList<QWidget *> &value) {
        rows = value;
        setContent(rows.isEmpty() ? nullptr : rows.first()->parentWidget());
    }
    void setList(QListWidget *value) {
        list = value;
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setContent(list);
        QObject::connect(list->model(), &QAbstractItemModel::rowsInserted, this, [this] { refresh(); });
        QObject::connect(list->model(), &QAbstractItemModel::rowsRemoved, this, [this] { refresh(); });
    }
    void refresh() {
        const int count = list ? list->count() : rows.size();
        perPage = qMax(1, (frame->height() - 8) / rowHeight);
        listPages.clear();
        if (list) {
            for (int first = 0; first < count;) {
                int last = qMin(first + perPage, count);
                if (last < count && last > first + 1
                    && list->item(last - 1)->data(Qt::UserRole).toStringList().value(0).isEmpty()) --last;
                listPages << qMakePair(first, last);
                first = last;
            }
        }
        const int pages = list ? qMax(1, listPages.size()) : qMax(1, (count + perPage - 1) / perPage);
        bar->setRange(0, pages - 1);
        bar->setPageStep(1);
        bar->setVisible(pages > 1);
        indicator->setText(QStringLiteral("Page %1 of %2").arg(bar->value() + 1).arg(pages));
        indicator->setVisible(pages > 1);
        display();
    }
    void reveal(int row) {
        refresh();
        if (list) {
            for (int page = 0; page < listPages.size(); ++page)
                if (row >= listPages[page].first && row < listPages[page].second) { bar->setValue(page); return; }
        } else bar->setValue(row / perPage);
    }
protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if (object == frame && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
            QTimer::singleShot(0, this, [this] { refresh(); });
            return false;
        }
        auto *widget = qobject_cast<QWidget *>(object);
        if (!content || !widget || (widget != content && !content->isAncestorOf(widget))) return false;
        if (event->type() == QEvent::Wheel) {
            bar->setValue(bar->value() + (static_cast<QWheelEvent *>(event)->angleDelta().y() < 0 ? 1 : -1));
            return true;
        }
        int y = 0;
        if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd) {
            const auto points = static_cast<QTouchEvent *>(event)->points();
            if (points.isEmpty()) return false;
            y = qRound(points.first().globalPosition().y());
        } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove
                   || event->type() == QEvent::MouseButtonRelease) {
            y = qRound(static_cast<QMouseEvent *>(event)->globalPosition().y());
        } else return false;
        if (event->type() == QEvent::TouchBegin || event->type() == QEvent::MouseButtonPress) {
            startY = y;
            swiped = false;
            return false;
        }
        if (swiped) {
            if (event->type() == QEvent::TouchEnd || event->type() == QEvent::MouseButtonRelease) {
                swiped = false;
                startY = INT_MIN;
            }
            return true;
        }
        if (startY != INT_MIN && qAbs(y - startY) > 70) {
            bar->setValue(bar->value() + (y < startY ? 1 : -1));
            swiped = event->type() != QEvent::TouchEnd && event->type() != QEvent::MouseButtonRelease;
            if (!swiped) startY = INT_MIN;
            return true;
        }
        if (event->type() == QEvent::TouchEnd || event->type() == QEvent::MouseButtonRelease) startY = INT_MIN;
        return false;
    }
private:
    void setContent(QWidget *value) {
        content = value;
        if (content) {
            content->setParent(frame);
            frame->layout()->removeWidget(bar);
            static_cast<QHBoxLayout *>(frame->layout())->addWidget(content, 1);
            static_cast<QHBoxLayout *>(frame->layout())->addWidget(bar);
        }
        refresh();
    }
    void display() {
        const auto range = list && !listPages.isEmpty()
            ? listPages[qMin(bar->value(), listPages.size() - 1)]
            : qMakePair(bar->value() * perPage, (bar->value() + 1) * perPage);
        const int first = range.first;
        const int last = range.second;
        if (list) {
            if (list->currentRow() >= 0 && (list->currentRow() < first || list->currentRow() >= last))
                list->setCurrentRow(-1);
            for (int i = 0; i < list->count(); ++i) list->item(i)->setHidden(i < first || i >= last);
        } else {
            for (int i = 0; i < rows.size(); ++i) {
                const bool onPage = i >= first && i < last;
                rows[i]->setVisible(onPage);
                if (onPage)
                    for (QWidget *child : rows[i]->findChildren<QWidget *>())
                        if (child->inherits("ToggleSwitch")) child->show();
            }
        }
        indicator->setText(QStringLiteral("Page %1 of %2").arg(bar->value() + 1).arg(bar->maximum() + 1));
    }
    QWidget *frame = nullptr;
    QWidget *content = nullptr;
    QLabel *indicator = nullptr;
    QScrollBar *bar = nullptr;
    QListWidget *list = nullptr;
    QList<QWidget *> rows;
    QList<QPair<int, int>> listPages;
    int rowHeight;
    int perPage = 1;
    int startY = INT_MIN;
    bool swiped = false;
};

struct nm_switch_row {
    QWidget *row;
    QCheckBox *state;
};

static nm_switch_row nm_native_switch(QWidget *parent, const QString &text) {
    auto ctor = reinterpret_cast<void (*)(QWidget *, QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN12ToggleSwitchC2EP7QWidget"));
    auto setState = reinterpret_cast<void (*)(QWidget *, bool, bool)>(dlsym(RTLD_DEFAULT, "_ZN12ToggleSwitch8setStateEbb"));
    if (!ctor || !setState || !nh_native_type("ToggleSwitch").isValid()) {
        auto *fallback = new QCheckBox(text, parent);
        return {fallback, fallback};
    }
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *label = new QLabel(text, row);
    QFont labelFont = parent->font();
    labelFont.setPixelSize(28);
    label->setFont(labelFont);
    layout->addWidget(label, 1);
    auto *toggle = reinterpret_cast<QWidget *>(nm_native_storage("ToggleSwitch", 0));
    if (!toggle) {
        delete row;
        auto *fallback = new QCheckBox(text, parent);
        return {fallback, fallback};
    }
    ctor(toggle, row);
    toggle->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(toggle);
    auto *state = new QCheckBox(row);
    state->hide();
    if (!QObject::connect(toggle, SIGNAL(toggled(bool)), state, SLOT(setChecked(bool)))) {
        delete row;
        auto *fallback = new QCheckBox(text, parent);
        return {fallback, fallback};
    }
    QObject::connect(state, &QCheckBox::toggled, row, [toggle, setState](bool checked) {
        setState(toggle, checked, true);
    });
    setState(toggle, false, true);
    row->setMinimumHeight(70);
    return {row, state};
}

static QLineEdit *nm_native_line(QWidget *parent) {
    auto ctor = reinterpret_cast<void (*)(QLineEdit *, QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN13TouchLineEditC2EP7QWidget"));
    if (!ctor || !nh_native_type("TouchLineEdit").isValid()) return new QLineEdit(parent);
    auto *line = reinterpret_cast<QLineEdit *>(nm_native_storage("TouchLineEdit", 0));
    if (!line) return new QLineEdit(parent);
    ctor(line, parent);
    return line;
}

struct nm_choice {
    QWidget *widget;
    QStringList items;
    bool native;
    QString currentText() const {
        if (!native) return static_cast<QComboBox *>(widget)->currentData().toString();
        auto currentIndex = reinterpret_cast<int (*)(QWidget *)>(dlsym(RTLD_DEFAULT, "_ZNK13TouchDropDown12currentIndexEv"));
        return items.value(currentIndex(widget));
    }
};

static nm_choice nm_native_choice(QWidget *parent, QStringList items, const QString &selected) {
    auto ctor = reinterpret_cast<void (*)(QWidget *, QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN13TouchDropDownC2EP7QWidget"));
    auto addItem = reinterpret_cast<void (*)(QWidget *, const QString &, const QVariant &, const QLocale &, bool)>(dlsym(RTLD_DEFAULT, "_ZN24MultiSelectTouchDropDown7addItemERK7QStringRK8QVariantRK7QLocaleb"));
    auto setCurrentIndex = reinterpret_cast<void (*)(QWidget *, int)>(dlsym(RTLD_DEFAULT, "_ZN13TouchDropDown15setCurrentIndexEi"));
    auto currentIndex = dlsym(RTLD_DEFAULT, "_ZNK13TouchDropDown12currentIndexEv");
    if (!ctor || !addItem || !setCurrentIndex || !currentIndex || !nh_native_type("TouchDropDown").isValid()) {
        auto *combo = new QComboBox(parent);
        for (const QString &key : items) combo->addItem(nm_choice_label(key), key);
        combo->setCurrentIndex(qMax(0, items.indexOf(selected)));
        return {combo, items, false};
    }
    auto *choice = reinterpret_cast<QWidget *>(nm_native_storage("TouchDropDown", 0));
    if (!choice) {
        auto *combo = new QComboBox(parent);
        for (const QString &key : items) combo->addItem(nm_choice_label(key), key);
        combo->setCurrentIndex(qMax(0, items.indexOf(selected)));
        return {combo, items, false};
    }
    ctor(choice, parent);
    for (const QString &key : items) addItem(choice, nm_choice_label(key), QVariant(key), QLocale(), false);
    setCurrentIndex(choice, qMax(0, items.indexOf(selected)));
    return {choice, items, true};
}

static void nm_native_keyboard(QWidget *view, const QList<QLineEdit *> &lines) {
    auto *frame = view->findChild<QWidget *>(QStringLiteral("keyboardFrame"));
    auto createKeyboard = reinterpret_cast<void *(*)(QWidget *, int, const QLocale &)>(dlsym(RTLD_DEFAULT, "_ZN13KeyboardFrame14createKeyboardE14KeyboardScriptRK7QLocale"));
    auto receiverCtor = reinterpret_cast<void (*)(QObject *, QLineEdit *, bool)>(dlsym(RTLD_DEFAULT, "_ZN16KeyboardReceiverC2EP9QLineEditb"));
    auto setReceiver = reinterpret_cast<void (*)(void *, QObject *, bool)>(dlsym(RTLD_DEFAULT, "_ZN24SearchKeyboardController11setReceiverEP16KeyboardReceiverb"));
    auto showKeyboard = reinterpret_cast<void (*)(QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN8N3Dialog12showKeyboardEv"));
    if (!frame || !createKeyboard || !receiverCtor || !setReceiver || !showKeyboard || !nh_native_type("KeyboardReceiver").isValid()) return;
    void *controller = createKeyboard(frame, 0, QLocale());
    if (!controller) return;
    for (QLineEdit *line : lines) {
        auto *receiver = reinterpret_cast<QObject *>(nm_native_storage("KeyboardReceiver", 0));
        if (!receiver) continue;
        receiverCtor(receiver, line, false);
        receiver->setParent(view);
        auto *shim = new QPushButton(view);
        shim->hide();
        if (QObject::connect(line, SIGNAL(tapped()), shim, SIGNAL(pressed())))
            QObject::connect(shim, &QPushButton::pressed, view, [view, line, receiver, controller, setReceiver, showKeyboard] {
                setReceiver(controller, receiver, true);
                line->setFocus();
                showKeyboard(view);
            });
    }
}

static void nm_native_style_button(QPushButton *button) {
    button->setMinimumHeight(64);
    button->setFocusPolicy(Qt::NoFocus);
    QFont font = button->font();
    font.setPixelSize(30);
    button->setFont(font);
    button->setStyleSheet(QStringLiteral("QPushButton { background: #dedede; border: 0; padding: 10px 18px; } QPushButton:pressed { background: black; color: white; }"));
}

static void nm_edit_ui_item(QListWidget *list, QListWidgetItem *existing, bool duplicate = false) {
    QWidget *editor = nm_native_view(existing && !duplicate ? QStringLiteral("Edit menu item") : QStringLiteral("Add menu item"), list->window());
    if (!editor) {
        nm_show_dialog(QStringLiteral("NickelMenu Settings"), QStringLiteral("Kobo's native editor is unavailable on this firmware."));
        return;
    }
    editor->setObjectName(QStringLiteral("nmEditor"));
    auto *content = new QWidget(editor);
    QFont uiFont = content->font();
    uiFont.setPixelSize(28);
    content->setFont(uiFont);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(22, 16, 22, 18);
    layout->setSpacing(20);
    auto *form = new QFormLayout;
    form->setSpacing(18);
    const QStringList oldFields = existing ? existing->data(Qt::UserRole).toStringList() : QStringList{};
    auto location = nm_native_choice(content,
        {QStringLiteral("main"), QStringLiteral("reader"), QStringLiteral("browser"),
         QStringLiteral("library"), QStringLiteral("selection"), QStringLiteral("selection_search")},
        existing ? oldFields[2] : QStringLiteral("main"));
    auto *label = nm_native_line(content);
    QStringList actions;
#define X(name, qt5, qt6) if (qt6) actions << QStringLiteral(#name);
    NM_ACTIONS
#undef X
    if (existing && !actions.contains(oldFields[4])) actions << oldFields[4];
    auto action = nm_native_choice(content, actions, existing ? oldFields[4] : QStringLiteral("dbg_msg"));
    auto *argument = nm_native_line(content);
    argument->setPlaceholderText(QStringLiteral("Optional action argument"));
    if (existing) {
        label->setText(oldFields[3]);
        argument->setText(oldFields[5]);
    }
    const QString initialLocation = existing ? oldFields[2] : QStringLiteral("main");
    const QString initialAction = existing ? oldFields[4] : QStringLiteral("dbg_msg");
    const QString initialLabel = label->text();
    const QString initialArgument = argument->text();
    auto *guard = new NmCloseGuard(editor, [location, action, label, argument,
                                           initialLocation, initialAction, initialLabel, initialArgument] {
        return location.currentText() != initialLocation || action.currentText() != initialAction
            || label->text() != initialLabel || argument->text() != initialArgument;
    });
    for (auto *widget : {location.widget, static_cast<QWidget *>(label),
                         action.widget, static_cast<QWidget *>(argument)}) {
        widget->setMinimumHeight(64);
        widget->setFont(uiFont);
    }
    location.widget->setFocusPolicy(Qt::NoFocus);
    action.widget->setFocusPolicy(Qt::NoFocus);
    form->addRow(QStringLiteral("Menu"), location.widget);
    form->addRow(QStringLiteral("Label"), label);
    form->addRow(QStringLiteral("Action"), action.widget);
    form->addRow(QStringLiteral("Argument"), argument);
    for (auto *widget : {location.widget, static_cast<QWidget *>(label),
                         action.widget, static_cast<QWidget *>(argument)})
        if (auto *caption = form->labelForField(widget)) caption->setFont(uiFont);
    layout->addLayout(form);
    layout->addStretch();
    auto *save = new QPushButton(QStringLiteral("Done"), content);
    nm_native_style_button(save);
    layout->addWidget(save);
    nm_native_set_content(editor, content);
    QObject::connect(save, &QPushButton::clicked, editor, [editor, list, existing, duplicate, location, label, action, argument, guard] {
        if (label->text().trimmed().isEmpty() || label->text().contains(':') || label->text().contains('\n')
            || label->text().contains('\r') || argument->text().contains('\n') || argument->text().contains('\r')) {
            nm_show_dialog(QStringLiteral("Menu item"), QStringLiteral("Enter a label without colons or line breaks, and a one-line argument."));
            return;
        }
        const QStringList previous = existing && !duplicate ? existing->data(Qt::UserRole).toStringList()
                                              : QStringList{nm_ui_items_path, QStringLiteral("-1")};
        const QStringList fields{previous[0], previous[1], location.currentText(), label->text().trimmed(), action.currentText(), argument->text()};
        auto *item = existing && !duplicate ? existing : new QListWidgetItem;
        item->setData(Qt::UserRole, fields);
        item->setText(nm_ui_item_text(fields));
        if (!existing || duplicate) { list->addItem(item); list->setCurrentItem(item); }
        guard->allow();
        editor->close();
    });
    editor->show();
    nm_native_keyboard(editor, {label, argument});
    auto hideKeyboard = reinterpret_cast<void (*)(QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN8N3Dialog12hideKeyboardEv"));
    if (hideKeyboard) hideKeyboard(editor);
}

static void nm_show_feature_settings() {
    QWidget *view = nm_native_view(QStringLiteral("NickelMenu Settings"));
    if (!view) {
        nm_show_dialog(QStringLiteral("NickelMenu Settings"), QStringLiteral("Kobo's native settings view is unavailable on this firmware."));
        return;
    }
    new NmSystemDialogGuard(view);
    auto *content = new QWidget(view);
    QFont uiFont = content->font();
    uiFont.setPixelSize(28);
    content->setFont(uiFont);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(22, 8, 22, 18);
    layout->setSpacing(6);
    auto *tabs = new QTabWidget(content);
    tabs->tabBar()->setFocusPolicy(Qt::NoFocus);
    tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 0; border-top: 1px solid #aaa; }"
        "QTabBar::tab { border: 0; background: white; min-height: 58px; min-width: 150px; padding: 4px 8px; font-size: 25px; }"
        "QTabBar::tab:selected { border-bottom: 4px solid black; font-weight: bold; }"
          "QLabel#nmSectionHeading { background: #dedede; padding: 7px 12px; font-size: 23px; }"
          "QFrame#nmPagedFrame { border: 0; }"
          "QListWidget { border: 0; font-size: 30px; } QListWidget::item { min-height: 76px; border-bottom: 1px solid #aaa; }"
        "QListWidget::item:selected { background: black; color: white; border: 0; }"));
    layout->addWidget(tabs);
    auto *permissionsPage = new QWidget(tabs);
    auto *permissionsLayout = new QVBoxLayout(permissionsPage);
    permissionsLayout->setContentsMargins(0, 0, 0, 0);
    auto *permissionsHeading = new QLabel(QStringLiteral("Manage the availability of different Menu items"), permissionsPage);
    permissionsHeading->setObjectName(QStringLiteral("nmSectionHeading"));
    permissionsLayout->addWidget(permissionsHeading);
    auto *permissionsPageNumber = new QLabel(permissionsPage);
    permissionsPageNumber->setFont(uiFont);
    permissionsLayout->addWidget(permissionsPageNumber, 0, Qt::AlignRight);
    auto *permissionsPager = new NmPagedFrame(permissionsPage, permissionsPageNumber, 60);
    permissionsLayout->addWidget(permissionsPager->widget(), 1);
    auto *featureList = new QWidget(permissionsPage);
    auto *rows = new QVBoxLayout(featureList);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    QList<QCheckBox *> boxes;
    QList<QWidget *> permissionRows;
    for (const auto &feature : nm_features) {
        const auto switchRow = nm_native_switch(featureList, QLatin1String(feature.label));
        auto *box = switchRow.state;
        box->setChecked(nm_feature_enabled(feature.key));
        switchRow.row->setMinimumHeight(60);
        box->setFocusPolicy(Qt::NoFocus);
        QFont font = switchRow.row->font();
        font.setPixelSize(26);
        switchRow.row->setFont(font);
        rows->addWidget(switchRow.row);
        permissionRows << switchRow.row;
        boxes.append(box);
    }
    rows->addStretch();
    permissionsPager->setRows(permissionRows);
    tabs->addTab(permissionsPage, QStringLiteral("Permissions"));
    auto *menuPage = new QWidget(tabs);
    auto *menuLayout = new QVBoxLayout(menuPage);
    menuLayout->setContentsMargins(0, 0, 0, 0);
    auto *menuHeading = new QLabel(QStringLiteral("Manage menu items on different menus"), menuPage);
    menuHeading->setObjectName(QStringLiteral("nmSectionHeading"));
    menuLayout->addWidget(menuHeading);
    auto *menuPageNumber = new QLabel(menuPage);
    menuPageNumber->setFont(uiFont);
    menuLayout->addWidget(menuPageNumber, 0, Qt::AlignRight);
    auto *menuItems = new QListWidget(menuPage);
    menuItems->setFocusPolicy(Qt::NoFocus);
    menuItems->setFont(uiFont);
    menuItems->setSpacing(0);
    menuItems->setFrameShape(QFrame::NoFrame);
    menuItems->setStyleSheet(QStringLiteral("QListWidget { border-top: 1px solid #aaa; }"));
    auto *menuPager = new NmPagedFrame(menuPage, menuPageNumber, 76);
    menuPager->setList(menuItems);
    menuLayout->addWidget(menuPager->widget(), 1);
    auto *blockedLegend = new QLabel(QStringLiteral("* Disabled because a permission is off."), menuPage);
    QFont legendFont = uiFont;
    legendFont.setPixelSize(22);
    blockedLegend->setFont(legendFont);
    blockedLegend->setStyleSheet(QStringLiteral("color: #666;"));
    menuLayout->addWidget(blockedLegend);
    auto snapshots = QMap<QString, QByteArray>{};
    const bool editable = nm_read_ui_items(menuItems, &snapshots);
    nm_group_ui_items(menuItems);
    menuPager->refresh();
    auto regroupPending = std::make_shared<bool>(false);
    auto refreshAfterRegroup = std::make_shared<std::function<void()>>();
    auto scheduleRegroup = [menuItems, menuPager, regroupPending, refreshAfterRegroup] {
        if (*regroupPending) return;
        *regroupPending = true;
        QTimer::singleShot(0, menuItems, [menuItems, menuPager, regroupPending, refreshAfterRegroup] {
            *regroupPending = false;
            nm_group_ui_items(menuItems);
            menuPager->refresh();
            if (menuItems->currentRow() >= 0) menuPager->reveal(menuItems->currentRow());
            if (*refreshAfterRegroup) (*refreshAfterRegroup)();
        });
    };
    QObject::connect(menuItems->model(), &QAbstractItemModel::rowsInserted, view, scheduleRegroup);
    QObject::connect(menuItems->model(), &QAbstractItemModel::rowsRemoved, view, scheduleRegroup);
    QObject::connect(menuItems->model(), &QAbstractItemModel::dataChanged, view, scheduleRegroup);
    auto refreshMarkers = [menuItems, boxes, snapshots] {
        const QSignalBlocker block(menuItems->model());
        for (int i = 0; i < menuItems->count(); ++i) {
            auto *item = menuItems->item(i);
            const QStringList fields = item->data(Qt::UserRole).toStringList();
            if (fields.size() != 6 || fields[0].isEmpty()) continue;
            const QString text = nm_ui_item_text(fields)
                + (nm_ui_item_blocked(fields, snapshots, boxes) ? QStringLiteral(" *") : QString());
            if (item->text() != text) item->setText(text);
        }
    };
    for (QCheckBox *box : boxes)
        QObject::connect(box, &QCheckBox::toggled, view, refreshMarkers);
    QObject::connect(menuItems->model(), &QAbstractItemModel::rowsInserted, view, refreshMarkers);
    QObject::connect(menuItems->model(), &QAbstractItemModel::dataChanged, view, refreshMarkers);
    refreshMarkers();
    QObject::connect(menuItems->model(), &QAbstractItemModel::rowsInserted, view,
        [menuPager, menuItems] { menuPager->reveal(menuItems->count() - 1); });
    auto *quickPage = new QWidget(tabs);
    auto *quickLayout = new QVBoxLayout(quickPage);
    quickLayout->setContentsMargins(0, 0, 0, 0);
    auto *quickHeading = new QLabel(QStringLiteral("Manage items shown on main NickelMenu menu"), quickPage);
    quickHeading->setObjectName(QStringLiteral("nmSectionHeading"));
    quickLayout->addWidget(quickHeading);
    auto *quickPageNumber = new QLabel(quickPage);
    quickPageNumber->setFont(uiFont);
    quickLayout->addWidget(quickPageNumber, 0, Qt::AlignRight);
    auto *quickPager = new NmPagedFrame(quickPage, quickPageNumber, 78);
    quickLayout->addWidget(quickPager->widget(), 1);
    auto *quickList = new QWidget(quickPage);
    auto *quickRows = new QVBoxLayout(quickList);
    quickRows->setContentsMargins(0, 0, 0, 0);
    quickRows->setSpacing(0);
    QList<QCheckBox *> quickBoxes;
    QList<QWidget *> shortcutRows;
    for (const auto &shortcut : nm_shortcuts) {
        const auto switchRow = nm_native_switch(quickList, QLatin1String(shortcut.label));
        auto *box = switchRow.state;
        box->setChecked(nm_managed_shortcut(menuItems, shortcut));
        switchRow.row->setEnabled(editable);
        box->setFocusPolicy(Qt::NoFocus);
        switchRow.row->setMinimumHeight(70);
        switchRow.row->setFont(uiFont);
        quickRows->addWidget(switchRow.row);
        shortcutRows << switchRow.row;
        quickBoxes << box;
        QObject::connect(box, &QCheckBox::toggled, view, [menuItems, boxes, shortcut](bool checked) {
            if (checked && !nm_managed_shortcut(menuItems, shortcut)) {
                for (qsizetype i = 0; i < boxes.size(); ++i)
                    if (!std::strcmp(nm_features[i].key, shortcut.permission)) boxes[i]->setChecked(true);
                const QStringList fields{nm_ui_items_path, QStringLiteral("-1"), QStringLiteral("main"),
                    QLatin1String(shortcut.label), QLatin1String(shortcut.action), QLatin1String(shortcut.argument)};
                auto *item = new QListWidgetItem(nm_ui_item_text(fields));
                item->setData(Qt::UserRole, fields);
                menuItems->addItem(item);
            } else if (!checked) {
                while (auto *item = nm_managed_shortcut(menuItems, shortcut))
                    delete menuItems->takeItem(menuItems->row(item));
            }
        });
    }
    quickRows->addStretch();
    quickPager->setRows(shortcutRows);
    tabs->insertTab(0, quickPage, QStringLiteral("Shortcuts"));
    auto refreshQuick = [menuItems, quickBoxes] {
        for (qsizetype i = 0; i < quickBoxes.size(); ++i) {
            const QSignalBlocker block(quickBoxes[i]);
            quickBoxes[i]->setChecked(nm_managed_shortcut(menuItems, nm_shortcuts[i]));
        }
    };
    QObject::connect(menuItems->model(), &QAbstractItemModel::rowsInserted, view, refreshQuick);
    QObject::connect(menuItems->model(), &QAbstractItemModel::rowsRemoved, view, refreshQuick);
    QObject::connect(menuItems->model(), &QAbstractItemModel::dataChanged, view, refreshQuick);
    auto *menuButtons = new QGridLayout;
    menuButtons->setHorizontalSpacing(6);
    auto *add = new QPushButton(QStringLiteral("Add menu item"), menuPage);
    auto *edit = new QPushButton(QStringLiteral("Edit selected"), menuPage);
    auto *duplicate = new QPushButton(QStringLiteral("Duplicate selected"), menuPage);
    auto *remove = new QPushButton(QStringLiteral("Remove selected"), menuPage);
    auto *moveUp = new QPushButton(QStringLiteral("Move up"), menuPage);
    auto *moveDown = new QPushButton(QStringLiteral("Move down"), menuPage);
    for (auto *button : {add, edit, duplicate, remove, moveUp, moveDown}) {
        nm_native_style_button(button);
        button->setMinimumHeight(56);
        QFont font = button->font();
        font.setPixelSize(25);
        button->setFont(font);
        button->setEnabled(editable);
    }
    edit->setEnabled(false);
    duplicate->setEnabled(false);
    remove->setEnabled(false);
    moveUp->setEnabled(false);
    moveDown->setEnabled(false);
    menuButtons->addWidget(moveUp, 0, 0);
    menuButtons->addWidget(add, 0, 1);
    menuButtons->addWidget(edit, 0, 2);
    menuButtons->addWidget(moveDown, 1, 0);
    menuButtons->addWidget(duplicate, 1, 1);
    menuButtons->addWidget(remove, 1, 2);
    menuLayout->addLayout(menuButtons);
    QObject::connect(menuItems, &QListWidget::currentItemChanged, view,
        [menuItems, edit, duplicate, remove, moveUp, moveDown, editable](QListWidgetItem *item) {
            const QString source = item ? item->data(Qt::UserRole).toStringList().value(0) : QString();
            const bool actualItem = editable && !source.isEmpty();
            edit->setEnabled(actualItem);
            duplicate->setEnabled(actualItem);
            remove->setEnabled(actualItem);
            const int row = item ? menuItems->row(item) : -1;
            const QString target = item ? item->data(Qt::UserRole).toStringList().value(2) : QString();
            moveUp->setEnabled(actualItem && row > 0
                && menuItems->item(row - 1)->data(Qt::UserRole).toStringList().value(0) == source
                && menuItems->item(row - 1)->data(Qt::UserRole).toStringList().value(2) == target);
            moveDown->setEnabled(actualItem && row >= 0 && row + 1 < menuItems->count()
                && menuItems->item(row + 1)->data(Qt::UserRole).toStringList().value(0) == source
                && menuItems->item(row + 1)->data(Qt::UserRole).toStringList().value(2) == target);
        });
    tabs->insertTab(1, menuPage, QStringLiteral("Menu items"));
    tabs->setCurrentIndex(0);
    QObject::connect(add, &QPushButton::clicked, view, [menuItems] { nm_edit_ui_item(menuItems, nullptr); });
    QObject::connect(edit, &QPushButton::clicked, view, [menuItems] {
        if (auto *item = menuItems->currentItem()) nm_edit_ui_item(menuItems, item);
    });
    QObject::connect(duplicate, &QPushButton::clicked, view, [menuItems] {
        if (auto *item = menuItems->currentItem()) nm_edit_ui_item(menuItems, item, true);
    });
    QObject::connect(remove, &QPushButton::clicked, view, [menuItems] {
        delete menuItems->takeItem(menuItems->currentRow());
    });
    auto moveItem = [menuItems, menuPager](int delta) {
        const int row = menuItems->currentRow();
        const int target = row + delta;
        if (row < 0 || target < 0 || target >= menuItems->count()) return;
        const QStringList fields = menuItems->item(row)->data(Qt::UserRole).toStringList();
        const QStringList neighbor = menuItems->item(target)->data(Qt::UserRole).toStringList();
        if (fields.value(0).isEmpty() || fields.value(0) != neighbor.value(0)
            || fields.value(2) != neighbor.value(2)) return;
        const QSignalBlocker block(menuItems->model());
        auto *item = menuItems->takeItem(row);
        menuItems->insertItem(target, item);
        menuPager->reveal(target);
        menuItems->setCurrentItem(item);
    };
    QObject::connect(moveUp, &QPushButton::clicked, view, [moveItem] { moveItem(-1); });
    QObject::connect(moveDown, &QPushButton::clicked, view, [moveItem] { moveItem(1); });
    if (!editable) nm_show_dialog(QStringLiteral("Menu items"), QStringLiteral("A config file could not be read or validated. Menu items are protected from changes."));
    QList<bool> initialFeatures;
    for (QCheckBox *box : boxes) initialFeatures << box->isChecked();
    QList<QStringList> initialItems;
    for (int i = 0; i < menuItems->count(); ++i)
        initialItems << menuItems->item(i)->data(Qt::UserRole).toStringList();
    auto isDirty = [boxes, menuItems, initialFeatures, initialItems] {
        for (qsizetype i = 0; i < boxes.size(); ++i)
            if (boxes[i]->isChecked() != initialFeatures[i]) return true;
        if (menuItems->count() != initialItems.size()) return true;
        for (int i = 0; i < menuItems->count(); ++i)
            if (menuItems->item(i)->data(Qt::UserRole).toStringList() != initialItems[i]) return true;
        return false;
    };
    auto *guard = new NmCloseGuard(view, isDirty);
    auto *save = new QPushButton(QStringLiteral("Apply changes"), content);
    auto *cancel = new QPushButton(QStringLiteral("Cancel"), content);
    nm_native_style_button(save);
    nm_native_style_button(cancel);
    auto *saveRow = new QGridLayout;
    saveRow->setHorizontalSpacing(6);
    saveRow->setColumnStretch(0, 1);
    saveRow->setColumnStretch(1, 1);
    saveRow->addWidget(save, 0, 0, 1, 2);
    saveRow->addWidget(cancel, 0, 2);
    layout->addLayout(saveRow);
    auto refreshDirty = [isDirty, save, cancel, saveRow] {
        const bool dirty = isDirty();
        save->setEnabled(dirty);
        cancel->setVisible(dirty);
        saveRow->setColumnStretch(2, dirty ? 1 : 0);
    };
    *refreshAfterRegroup = refreshDirty;
    for (QCheckBox *box : boxes)
        QObject::connect(box, &QCheckBox::toggled, view, refreshDirty);
    QObject::connect(menuItems->model(), &QAbstractItemModel::rowsInserted, view, refreshDirty);
    QObject::connect(menuItems->model(), &QAbstractItemModel::rowsRemoved, view, refreshDirty);
    QObject::connect(menuItems->model(), &QAbstractItemModel::dataChanged, view, refreshDirty);
    QObject::connect(moveUp, &QPushButton::clicked, view, refreshDirty);
    QObject::connect(moveDown, &QPushButton::clicked, view, refreshDirty);
    refreshDirty();
    nm_native_set_content(view, content);
    QObject::connect(cancel, &QPushButton::clicked, view, [view] { view->close(); });
    QObject::connect(save, &QPushButton::clicked, view, [view, boxes, menuItems, editable, snapshots, guard] {
        if (editable && !nm_write_ui_items(menuItems, snapshots)) {
            nm_show_dialog(QStringLiteral("NickelMenu Settings"), QStringLiteral("Could not save menu items."));
            return;
        }
        QSettings settings(QLatin1String(NM_CONFIG_DIR "/.features.ini"), QSettings::IniFormat);
        for (qsizetype i = 0; i < boxes.size(); ++i)
            settings.setValue(QLatin1String(nm_features[i].key), boxes[i]->isChecked());
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            nm_show_dialog(QStringLiteral("NickelMenu Settings"), QStringLiteral("Could not save feature settings."));
            return;
        }
        nm_global_config_reload();
        guard->allow();
        view->close();
    });
    auto getActive = reinterpret_cast<QWidget *(*)(QWidget *)>(dlsym(RTLD_DEFAULT, "_ZN11MainNavView18getActiveNavButtonEv"));
    auto setActive = reinterpret_cast<void (*)(QWidget *, bool)>(dlsym(RTLD_DEFAULT, "_ZN13MainNavButton9setActiveEb"));
    if (getActive && setActive) {
        QWidget *nav = nullptr;
        QWidget *menuTab = nullptr;
        for (QWidget *widget : QApplication::allWidgets()) {
            if (widget->inherits("MainNavView")) nav = widget;
            if (widget->objectName() == QLatin1String("nmButton")) menuTab = widget;
        }
        if (nav && menuTab) {
            QPointer<QWidget> previous = getActive(nav);
            QPointer<QWidget> tab = menuTab;
            auto restore = std::make_shared<QPointer<QWidget>>(previous);
            if (previous && previous != tab) setActive(previous, false);
            setActive(menuTab, true);
            guard->setStay([previous, tab, restore, setActive] {
                if (*restore && *restore != tab) setActive(*restore, false);
                if (tab) setActive(tab, true);
                *restore = previous;
            });
            for (QWidget *button : QApplication::allWidgets()) {
                if (!button->inherits("MainNavButton") || button == menuTab) continue;
                auto *shim = new QPushButton(view);
                shim->hide();
                if (QObject::connect(button, SIGNAL(tapped()), shim, SIGNAL(pressed())))
                    QObject::connect(shim, &QPushButton::pressed, view, [view, button, previous, tab, restore, setActive] {
                        *restore = button;
                        setActive(button, false);
                        if (tab) setActive(tab, true);
                        if (auto *child = view->findChild<QWidget *>(QStringLiteral("nmEditor"), Qt::FindDirectChildrenOnly);
                                child && child->isVisible()) {
                            auto pending = std::make_shared<bool>(true);
                            if (auto *childGuard = static_cast<NmCloseGuard *>(child->findChild<QObject *>(QStringLiteral("nmCloseGuard"))))
                                childGuard->setStay([previous, tab, restore, setActive, pending] {
                                    *pending = false;
                                    if (*restore && *restore != tab) setActive(*restore, false);
                                    if (tab) setActive(tab, true);
                                    *restore = previous;
                                });
                            QObject::connect(child, &QObject::destroyed, view, [view, pending] {
                                if (*pending) view->close();
                            });
                            child->close();
                            return;
                        }
                        view->close();
                    });
            }
            QObject::connect(view, &QObject::destroyed, [restore, tab, setActive] {
                if (tab) setActive(tab, false);
                if (*restore) setActive(*restore, true);
            });
        }
    }
    view->show();
}

#endif

typedef QWidget MenuTextItem; // it's actually a subclass, but we don't need its functionality directly, so we'll stay on the safe side
typedef void MainWindowController;

// AbstractNickelMenuController::createMenuTextItem creates a menu item
// (allocated on the heap). The last parameter has been empty everywhere I've
// seen it. The checked option does not have any effect unless checkable is true
// (see the search menu for an example).
static MenuTextItem* (*AbstractNickelMenuController_createMenuTextItem)(void*, QMenu* menu, QString const& text, bool checkable, bool checked, QString const& /*I think it's for QObject::setObjectName*/);

// AbstractNickelMenuController::createAction finishes adding the action to the
// menu. The actionWidget should be a MenuTextItem (allocated on the heap). If
// disabled is true, the item will appear greyed out. Note that for the
// separator, even in the main menu (removed in FW 15505), it'll inherit the
// color of a Reader Menu separator (#666), rather than the usual dim (#BBB) or
// solid (#000) ones seen in the stock main menu.
//
// The code is basically:
//     act = new QWidgetAction(this, arg2);
//     act->setEnabled(arg4);
//     if (arg3)
//         connect(act, &QAction::triggered, &QWidget::hide, arg1);
//     arg1->addAction(act);
//     if (arg5)
//         arg1->addSeparator();
static QAction* (*AbstractNickelMenuController_createAction)(void*, QMenu* menu, QWidget* actionWidget, bool closeOnTap, bool disabled, bool separatorAfter);

// ConfirmationDialogFactory::showOKDialog shows an dialog box with an OK
// button, and should only be called from the main thread (or a signal handler).
// Note that ConfirmationDialog uses Qt::RichText for the body.
static void (*ConfirmationDialogFactory_showOKDialog)(QString const& title, QString const& body);

MainWindowController *(*MainWindowController_sharedInstance)();

// MainWindowController::toast shows a message as an overlay, and should only be
// called from the main thread (or a signal handler).
void (*MainWindowController_toast)(MainWindowController*, QString const& primary, QString const& secondary, int milliseconds);

// *MenuSeparator::*MenuSeparator initializes a main menu separator which can be
// added to the menu with QWidget::addAction. The menu should be its parent. It
// initializes a QAction.
static void (*LightMenuSeparator_LightMenuSeparator)(void*, QWidget* parent);
static void (*BoldMenuSeparator_BoldMenuSeparator)(void*, QWidget* parent);

// New bottom tab bar which replaced the main menu on 15505+.
typedef QWidget MainNavButton;
typedef QWidget MainNavView;
MainNavView *(*MainNavView_MainNavView)(MainNavView*, QWidget* parent);
void (*MainNavButton_MainNavButton)(MainNavButton*, QWidget* parent);
void (*MainNavButton_setPixmap)(MainNavButton*, QString const& pixmapName);
void (*MainNavButton_setActivePixmap)(MainNavButton*, QString const& pixmapName);
void (*MainNavButton_setText)(MainNavButton*, QString const& text);
void (*MainNavButton_tapped)(MainNavButton*); // signal

// Creating menus from scratch (for the menu above on 15505+).
typedef QMenu TouchMenu;
typedef TouchMenu NickelTouchMenu;
typedef int DecorationPosition;
void (*NickelTouchMenu_NickelTouchMenu)(NickelTouchMenu*, QWidget* parent, DecorationPosition position);
void (*MenuTextItem_MenuTextItem)(MenuTextItem*, QWidget* parent, bool checkable, bool italic);
void (*MenuTextItem_setText)(MenuTextItem*, QString const& text);
void (*MenuTextItem_registerForTapGestures)(MenuTextItem*);

// Selection menu stuff (14622+).
typedef void SelectionMenuController; // note: items are re-initialized every time the menu is opened
typedef QWidget SelectionMenuView;
typedef void WebSearchMixinBase;
void (*SelectionMenuController_lookupWikipedia)(SelectionMenuController*);
void (*SelectionMenuController_setupSearchOptions)(SelectionMenuController*, bool);
SelectionMenuView *(*SelectionMenuController_menuView)(SelectionMenuController*);
void (*SelectionMenuController_close)(SelectionMenuController*);
void (*SelectionMenuController_clearSelection)(SelectionMenuController*);
QString (*PageImageReader_selectedText)(QWidget*);
void (*SelectionMenuController_onSearchInBook) (SelectionMenuController*);
void (*SelectionMenuController_lookupWeb)(SelectionMenuController*); // 14622-18838
void (*SelectionMenuController_lookupGoogle)(SelectionMenuController*); // 19086+ (replaces lookupWeb, alternative 1)
void (*SelectionMenuController_lookupBaidu)(SelectionMenuController*); // 19086+ (replaces lookupWeb, alternative 2)
void (*SelectionMenuController_addMenuItem)(SelectionMenuController*, SelectionMenuView* smv, MenuTextItem* mti, const char *slot); // note: the MenuTextItem is created by SelectionMenuController_createMenuTextItem, with smv as the parent (the first QWidget* argument)
void (*SelectionMenuView_addMenuItem)(SelectionMenuView*, MenuTextItem *mti); // note: this adds the separator and the item (it doesn't connect signals or things like that)
void (*WebSearchMixinBase_doWikipediaSearch)(WebSearchMixinBase *, QString const& selection, QString const& locale);

#if NM_QT_MAJOR >= 6
static bool (*Device_supportsDropbox)(const void *) = nullptr;
static bool (*Device_supportsGoogleDrive)(const void *) = nullptr;
static bool (*Device_isSpaKoboBW)(const void *) = nullptr;
static bool nm_exact_n365_firmware = false;

struct nm_cloud_firmware_signature {
    qint64 dropboxOffset;
    qint64 googleDriveOffset;
    const char *sha256;
};

static constexpr nm_cloud_firmware_signature nm_cloud_firmware_signatures[] = {
    {0x00f5b9b4, 0x00f5b9e4, "b22059eb126d355312f69abb9d1cafc72f0cf54ed4ffe5607eadda2d65670df7"}, // 6.0.274403
    {0x00f5e58c, 0x00f5e5bc, "82c877261652bec2a7d601b1ac3007f48bd6f3b39dd1950b19d0869cb29a7a44"}, // 6.0.279225
};

static bool nm_hash_target_libnickel(QFile &library, const nm_cloud_firmware_signature &signature) {
    const qint64 dropboxOffset = signature.dropboxOffset;
    const qint64 googleDriveOffset = signature.googleDriveOffset;
    constexpr qint64 methodSize = 6;
    static const auto original = QByteArray::fromHex("10b501210446");
    static const auto enabled = QByteArray::fromHex("00b5012000bd");
    if (library.size() <= googleDriveOffset + methodSize) return false;

    if (!library.seek(dropboxOffset)) return false;
    const auto dropboxBytes = library.read(methodSize);
    if (!library.seek(googleDriveOffset)) return false;
    const auto googleDriveBytes = library.read(methodSize);
    const bool patched = dropboxBytes == enabled && googleDriveBytes == enabled;
    if (!patched && (dropboxBytes != original || googleDriveBytes != original)) return false;
    if (!library.seek(0)) return false;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buffer[16 * 1024];
    qint64 position = 0;
    while (true) {
        const qint64 count = library.read(buffer, sizeof(buffer));
        if (count < 0) return false;
        if (!count) break;
        if (patched) {
            const qint64 ranges[] = {dropboxOffset, googleDriveOffset};
            for (const qint64 start : ranges) {
                const qint64 from = qMax(position, start);
                const qint64 to = qMin(position + count, start + methodSize);
                if (from < to)
                    std::memcpy(buffer + (from - position), original.constData() + (from - start), to - from);
            }
        }
        hash.addData(QByteArrayView(buffer, count));
        position += count;
    }
    return hash.result().toHex() == signature.sha256;
}

static bool nm_hash_exact_target_libnickel(QFile &library) {
    for (const auto &signature : nm_cloud_firmware_signatures)
        if (nm_hash_target_libnickel(library, signature)) return true;
    return false;
}

static bool nm_is_exact_target_firmware() {
    void *address = dlsym(RTLD_DEFAULT, "_ZN6Device16getCurrentDeviceEv");
    Dl_info info{};
    if (!address || !dladdr(address, &info) || !info.dli_fname) return false;
    QFile library(QString::fromLocal8Bit(info.dli_fname));
    return library.open(QIODevice::ReadOnly) && nm_hash_exact_target_libnickel(library);
}

extern "C" __attribute__((visibility("default"))) bool _nm_device_supports_dropbox(const void *device) {
    if (nm_exact_n365_firmware && Device_isSpaKoboBW && Device_isSpaKoboBW(device)) return true;
    return Device_supportsDropbox && Device_supportsDropbox(device);
}

extern "C" __attribute__((visibility("default"))) bool _nm_device_supports_google_drive(const void *device) {
    if (nm_exact_n365_firmware && Device_isSpaKoboBW && Device_isSpaKoboBW(device)) return true;
    return Device_supportsGoogleDrive && Device_supportsGoogleDrive(device);
}
#endif

static struct nh_info NickelMenu = (struct nh_info){
    .name            = "NickelMenu",
    .desc            = "Integrated launcher for Nickel.",
    .uninstall_flag  = NM_CONFIG_DIR "/uninstall",
#ifdef NM_UNINSTALL_CONFIGDIR
    .uninstall_xflag = NM_CONFIG_DIR,
#else
    .uninstall_xflag = NULL,
#endif
    .failsafe_delay  = 3,
};

static struct nh_hook NickelMenuHook[] = {
#if NM_QT_MAJOR >= 6
    {.sym = "_ZNK6Device15supportsDropboxEv", .sym_new = "_nm_device_supports_dropbox", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(Device_supportsDropbox), .desc = "N365 Qt6 cloud capability compatibility", .optional = true},
    {.sym = "_ZNK6Device19supportsGoogleDriveEv", .sym_new = "_nm_device_supports_google_drive", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(Device_supportsGoogleDrive), .desc = "N365 Qt6 cloud capability compatibility", .optional = true},
#endif
    {.sym = "_ZN23SelectionMenuController18setupSearchOptionsEb", .sym_new = "_nm_selection_search", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(SelectionMenuController_setupSearchOptions), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN23SelectionMenuController18setupSearchOptionsEb
    // menu injection
    {.sym = "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", .sym_new = "_nm_menu_hook", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(AbstractNickelMenuController_createMenuTextItem), .desc = nullptr, .optional = true}, //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_

    // bottom nav main menu button injection (15505+)
    {.sym = "_ZN11MainNavViewC1EP7QWidget", .sym_new = "_nm_menu_hook2", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(MainNavView_MainNavView), .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN11MainNavViewC1EP7QWidget

    // selection menu injection
    {.sym = "_ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc", .sym_new = "_nm_menu_hook3", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(SelectionMenuController_addMenuItem),  .desc = "selection menu injection",                     .optional = true}, //libnickel 4.20.14622 * _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc
    {.sym = "_ZN18WebSearchMixinBase17doWikipediaSearchERK7QStringS2_",                         .sym_new = "_nm_menu_hook4", .lib = "libnickel.so.1.0.0", .out = nh_symoutptr(WebSearchMixinBase_doWikipediaSearch), .desc = "selection menu injection (wikipedia handler)", .optional = true}, //libnickel 4.20.14622 * _ZN18WebSearchMixinBase17doWikipediaSearchERK7QStringS2_

    // null
    {0},
};

static struct nh_dlsym NickelMenuDlsym[] = {
#if NM_QT_MAJOR >= 6
    {.name = "_ZNK6Device11isSpaKoboBWEv", .out = nh_symoutptr(Device_isSpaKoboBW), .desc = "N365 Qt6 cloud capability compatibility", .optional = true},
#endif
    {.name = "_ZN23SelectionMenuController14clearSelectionEv", .out = nh_symoutptr(SelectionMenuController_clearSelection), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN23SelectionMenuController14clearSelectionEv
    {.name = "_ZN23SelectionMenuController5closeEv", .out = nh_symoutptr(SelectionMenuController_close), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN23SelectionMenuController5closeEv
    {.name = "_ZN15PageImageReader12selectedTextEv", .out = nh_symoutptr(PageImageReader_selectedText), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN15PageImageReader12selectedTextEv
    {.name = "_ZN23SelectionMenuController8menuViewEv", .out = nh_symoutptr(SelectionMenuController_menuView), .desc = nullptr, .optional = true}, //libnickel 6.0 * _ZN23SelectionMenuController8menuViewEv
    // menu injection
    {.name = "_ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_", .out = nh_symoutptr(AbstractNickelMenuController_createMenuTextItem), .desc = nullptr, .optional = true}, //libnickel 4.6 * _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_
    {.name = "_ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb",                .out = nh_symoutptr(AbstractNickelMenuController_createAction), .desc = nullptr, .optional = true},       //libnickel 4.6 * _ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb
    {.name = "_ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_",                   .out = nh_symoutptr(ConfirmationDialogFactory_showOKDialog), .desc = nullptr, .optional = true},          //libnickel 4.6 * _ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_
    {.name = "_ZN20MainWindowController14sharedInstanceEv",                                  .out = nh_symoutptr(MainWindowController_sharedInstance), .desc = nullptr, .optional = true},             //libnickel 4.6 * _ZN20MainWindowController14sharedInstanceEv
    {.name = "_ZN20MainWindowController5toastERK7QStringS2_i",                               .out = nh_symoutptr(MainWindowController_toast), .desc = nullptr, .optional = true},                      //libnickel 4.6 * _ZN20MainWindowController5toastERK7QStringS2_i
    {.name = "_ZN18LightMenuSeparatorC2EP7QWidget",                                          .out = nh_symoutptr(LightMenuSeparator_LightMenuSeparator), .desc = nullptr, .optional = true},           //libnickel 4.6 * _ZN18LightMenuSeparatorC2EP7QWidget
    {.name = "_ZN17BoldMenuSeparatorC1EP7QWidget",                                           .out = nh_symoutptr(BoldMenuSeparator_BoldMenuSeparator), .desc = nullptr, .optional = true},             //libnickel 4.6 * _ZN17BoldMenuSeparatorC1EP7QWidget

    // bottom nav main menu button injection (15505+)
    {.name = "_ZN13MainNavButtonC1EP7QWidget",                       .out = nh_symoutptr(MainNavButton_MainNavButton),         .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButtonC1EP7QWidget
    {.name = "_ZN13MainNavButton9setPixmapERK7QString",              .out = nh_symoutptr(MainNavButton_setPixmap),             .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton9setPixmapERK7QString
    {.name = "_ZN13MainNavButton15setActivePixmapERK7QString",       .out = nh_symoutptr(MainNavButton_setActivePixmap),       .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton15setActivePixmapERK7QString
    {.name = "_ZN13MainNavButton7setTextERK7QString",                .out = nh_symoutptr(MainNavButton_setText),               .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton7setTextERK7QString
    {.name = "_ZN13MainNavButton6tappedEv",                          .out = nh_symoutptr(MainNavButton_tapped),                .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN13MainNavButton6tappedEv
    {.name = "_ZN15NickelTouchMenuC2EP7QWidget18DecorationPosition", .out = nh_symoutptr(NickelTouchMenu_NickelTouchMenu),     .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN15NickelTouchMenuC2EP7QWidget18DecorationPosition
    {.name = "_ZN12MenuTextItemC1EP7QWidgetbb",                      .out = nh_symoutptr(MenuTextItem_MenuTextItem),           .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItemC1EP7QWidgetbb
    {.name = "_ZN12MenuTextItem7setTextERK7QString",                 .out = nh_symoutptr(MenuTextItem_setText),                .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItem7setTextERK7QString
    {.name = "_ZN12MenuTextItem22registerForTapGesturesEv",          .out = nh_symoutptr(MenuTextItem_registerForTapGestures), .desc = "bottom nav main menu button injection (15505+)", .optional = true}, //libnickel 4.23.15505 * _ZN12MenuTextItem22registerForTapGesturesEv

    // selection menu injection (14622+)
    {.name = "_ZN17SelectionMenuView11addMenuItemEP12MenuTextItem", .out = nh_symoutptr(SelectionMenuView_addMenuItem),           .desc = "selection menu injection (14622+)",        .optional = true}, //libnickel 4.20.14622 * _ZN17SelectionMenuView11addMenuItemEP12MenuTextItem
    {.name = "_ZN23SelectionMenuController15lookupWikipediaEv",     .out = nh_symoutptr(SelectionMenuController_lookupWikipedia), .desc = "selection menu injection (14622+)",        .optional = true}, //libnickel 4.20.14622 * _ZN23SelectionMenuController15lookupWikipediaEv _ZN23SelectionMenuController15lookupWikipediaERK7QString
    {.name = "_ZN23SelectionMenuController9lookupWebEv",            .out = nh_symoutptr(SelectionMenuController_lookupWeb),       .desc = "selection menu injection (14622-18838)",   .optional = true}, //libnickel 4.20.14622 4.30.18838 _ZN23SelectionMenuController9lookupWebEv
    {.name = "_ZN23SelectionMenuController12lookupGoogleEv",        .out = nh_symoutptr(SelectionMenuController_lookupGoogle),    .desc = "selection menu injection (19086+, alt 1)", .optional = true}, //libnickel 4.31.19086 * _ZN23SelectionMenuController12lookupGoogleEv _ZN23SelectionMenuController12lookupGoogleERK7QString _ZN23SelectionMenuController9lookupWebEv
    {.name = "_ZN23SelectionMenuController11lookupBaiduEv",         .out = nh_symoutptr(SelectionMenuController_lookupBaidu),     .desc = "selection menu injection (19086+, alt 2)", .optional = true}, //libnickel 4.31.19086 * _ZN23SelectionMenuController11lookupBaiduEv _ZN23SelectionMenuController11lookupBaiduERK7QString

    // selection menu injection (parental controls enabled)
    {.name = "_ZN23SelectionMenuController14onSearchInBookEv",     .out = nh_symoutptr(SelectionMenuController_onSearchInBook), .desc = "selection menu injection (14601+, parental controls enabled)", .optional = true}, //libnickel 4.20.14601 * _ZN23SelectionMenuController14onSearchInBookEv

    // null
    {0},
};

static int nm_init();

NickelHook(
    .init  = &nm_init,
    .info  = &NickelMenu,
    .hook  = NickelMenuHook,
    .dlsym = NickelMenuDlsym,
)

// AbstractNickelMenuController_createAction_before wraps
// AbstractNickelMenuController::createAction to use the correct separator for
// the menu location and to match the behaviour of QMenu::insertAction instead
// of QMenu::addAction. It also adds the property nm_action=true to the action
// and separator.
QAction *AbstractNickelMenuController_createAction_before(QAction *before, nm_menu_location_t loc, bool last_in_group, void *_this, QMenu *menu, QWidget *widget, bool close, bool enabled, bool separator);

// nm_argtranform_t transforms an action's argument and returns a new malloc'd
// string. On error, it should return NULL and set nm_err.
typedef char *(*nm_argtransform_t)(void *data, const char *arg);

// nm_menu_item_do runs a nm_menu_item_t and must be called from the thread of a
// signal handler. argtransform and argtransform_data are optional.
static void nm_menu_item_do(nm_menu_item_t *it, nm_argtransform_t argtransform, void *argtransform_data);

// _nm_menu_inject handles the QMenu::aboutToShow signal and injects menu items.
static void _nm_menu_inject(void *nmc, QMenu *menu, nm_menu_location_t loc, int at);

static void nm_show_dialog(QString const &title, QString const &body) {
    if (ConfirmationDialogFactory_showOKDialog)
        ConfirmationDialogFactory_showOKDialog(title, body);
    else
        NM_LOG("dialog unavailable: %s: %s", qPrintable(title), qPrintable(body));
}

static int nm_init() {
#if NM_QT_MAJOR >= 6
    nm_exact_n365_firmware = nm_is_exact_target_firmware();
    NM_LOG("cloud capability compatibility: %s", nm_exact_n365_firmware
        ? "target firmware recognized; N365 model check remains active"
        : "target firmware not recognized; native model gates unchanged");
#endif
    // Each menu and output feature checks its own dependencies before use.
    for (auto *symbol = NickelMenuDlsym; symbol->name; ++symbol) {
        auto *code = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(*symbol->out) & ~uintptr_t(1));
        if (*symbol->out && !nh_native_range(code, 2, PF_R | PF_X)) {
            NM_LOG("symbol outside executable memory: %s", symbol->name);
            *symbol->out = nullptr;
        }
    }
    #ifdef NM_UNINSTALL_CONFIGDIR
    NM_LOG("feature: NM_UNINSTALL_CONFIGDIR: true");
    #else
    NM_LOG("feature: NM_UNINSTALL_CONFIGDIR: false");
    #endif

    NM_LOG("updating config");

    int rev = nm_global_config_update();
    if (nm_err_peek())
        NM_LOG("... warning: error parsing config, will show a menu item with the error: %s", nm_err());

    size_t ntmp = SIZE_MAX;
    if (rev == -1) {
        NM_LOG("... info: no config file changes detected for initial config update (it should always return an error or update), stopping (this is a bug; err should have been returned instead)");
    } else if (!nm_global_config_items(&ntmp)) {
        NM_LOG("... warning: no menu items returned by nm_global_config_items, ignoring for now (this is a bug; it should always have a menu item whether the default, an error, or the actual config)");
    } else if (ntmp == SIZE_MAX) {
        NM_LOG("... warning: no size returned by nm_global_config_items, ignoring for now (this is a bug)");
    } else if (!ntmp) {
        NM_LOG("... warning: size returned by nm_global_config_items is 0, ignoring for now (this is a bug; it should always have a menu item whether the default, an error, or the actual config)");
    }

    return 0;
}

extern "C" __attribute__((visibility("default"))) MenuTextItem* _nm_menu_hook(void* _this, QMenu* menu, QString const& label, bool checkable, bool checked, QString const& thingy) {
    NM_LOG("AbstractNickelMenuController::createMenuTextItem(%p, `%s`, %d, %d, `%s`)", menu, qPrintable(label), checkable, checked, qPrintable(thingy));

    QString trmm = QCoreApplication::translate("StatusBarMenuController", "Settings");
    QString trrm = QCoreApplication::translate("DictionaryActionProxy", "Dictionary");
    QString trbm = QCoreApplication::translate("N3BrowserSettingsMenuController", "Keyboard");
    QString trlm = QCoreApplication::translate("LibraryViewMenuController", "Manage downloads");
    NM_LOG("Comparing against '%s', '%s', '%s', '%s'", qPrintable(trmm), qPrintable(trrm), qPrintable(trbm), qPrintable(trlm));

    nm_menu_location_t loc = {};
    if (label == trmm && !checkable) {
        NM_LOG("Intercepting main menu (label=Settings, checkable=false)...");
        loc = NM_MENU_LOCATION(main);
    } else if (label == trrm && !checkable) {
        NM_LOG("Intercepting reader menu (label=Dictionary, checkable=false)...");
        loc = NM_MENU_LOCATION(reader);
    } else if (label == trbm && !checkable) {
        NM_LOG("Intercepting browser menu (label=Keyboard, checkable=false)...");
        loc = NM_MENU_LOCATION(browser);
    } else if (label == trlm && !checkable) {
        NM_LOG("Intercepting library menu (label=Manage downloads, checkable=false)...");  // this is actually two menus: in "My Books", and in "My Articles"
        loc = NM_MENU_LOCATION(library);
    }

    if (loc && AbstractNickelMenuController_createAction)
        QObject::connect(menu, &QMenu::aboutToShow, std::bind(_nm_menu_inject, _this, menu, loc, menu->actions().count()));

    return AbstractNickelMenuController_createMenuTextItem(_this, menu, label, checkable, checked, thingy);
}

QString nm_menu_pixmap(const char *custom, const char *custom_temp_out, const char *fallback) {
    if (!custom || !custom_temp_out)
        return QString(fallback);

    QImage a;
    if (!a.load(QString(custom))) {
        NM_LOG("nm_menu_pixmap: error loading '%s', falling back to '%s': %s", custom, fallback, QFile::exists(custom) ? "failed to load image" : "image does not exist");
        return QString(fallback);
    }

    // The stock tab buttons come without a fallback to size against, which
    // left their custom icons unapplied. Every tab icon has the size of the
    // More icon, so size against that when no fallback is given.
    const char *reference = fallback ? fallback : ":/images/home/main_nav_more.png";
    QPixmap b;
    if (!b.load(QString(reference))) {
        NM_LOG("nm_menu_pixmap: error loading default pixmap '%s': %s", reference, QFile::exists(reference) ? "failed to load image" : "image does not exist");
        return QString(fallback);
    }

    QImage c = a.scaled(b.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    NM_LOG("nm_menu_pixmap: resized '%s' to match '%s' (%dx%d): %dx%d", custom, reference, b.size().width(), b.size().height(), c.size().width(), c.size().height());

    if (!c.save(QString(custom_temp_out), "PNG")) {
        NM_LOG("nm_menu_pixmap: error saving resized pixmap to '%s'", custom_temp_out);
        return QString(fallback);
    }

    return QString(custom_temp_out);
}

static const char *nm_main_menu_config(int index, const char *option) {
    char buf[strlen("menu_main_15505_9_icon_active") + 1];
    if (snprintf(buf, sizeof(buf), "menu_main_15505_%d_%s", index, option) >= static_cast<ssize_t>(sizeof(buf))) {
        NM_LOG("Failed to create main menu config key for index %d, option %s", index, option);
        return nullptr;
    }
    return nm_global_config_experimental(buf);
}

static bool nm_write_default_icon(const char *path, int resourceIndex) {
    const nh_resource &resource = nh_embedded_resources[resourceIndex];
    const QImage icon = QImage::fromData(resource.data, static_cast<int>(resource.size), "PNG");
    return !icon.isNull() && icon.save(QString::fromLatin1(path), "PNG");
}

static void main_nav_button_configure(MainNavButton *btn, const char *label, const char *icon, const char *icon_fallback, const char *icon_active, const char *icon_active_fallback) {
    if (label && MainNavButton_setText) {
        MainNavButton_setText(btn, label);
    }

    if ((icon || icon_fallback) && MainNavButton_setPixmap) {
        QString pixmap = nm_menu_pixmap(
            icon,
            "/tmp/nm_menu.png",
            icon_fallback
        );
        if (!pixmap.isEmpty()) {
            MainNavButton_setPixmap(btn, pixmap);
        }
    }

    if ((icon_active || icon_active_fallback) && MainNavButton_setActivePixmap) {
        QString pixmap = nm_menu_pixmap(
            icon_active,
            "/tmp/nm_menu.png",
            icon_active_fallback
        );
        if (!pixmap.isEmpty()) {
            MainNavButton_setActivePixmap(btn, pixmap);
        }
    }

    if (icon || icon_fallback || icon_active || icon_active_fallback) {
        QFile::remove("/tmp/nm_menu.png");
    }
}

static void nm_main_nav_inject(MainNavView *_this, QWidget *parent) {
    if (!MainNavButton_MainNavButton || !MainNavButton_setText || !MainNavButton_tapped || !NickelTouchMenu_NickelTouchMenu || !MenuTextItem_MenuTextItem || !MenuTextItem_setText || !MenuTextItem_registerForTapGestures) {
        NM_LOG("Could not find required MainNavButton symbols, cannot add tab button for NickelMenu main menu.");
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (!nh_native_type("MainNavButton").isValid() || !nh_native_type("NickelTouchMenu").isValid()
            || !nh_native_type("MenuTextItem").isValid()) {
        NM_LOG("native menu type unavailable, cannot add NickelMenu tab");
        return;
    }

#endif

    QHBoxLayout *bl = _this->findChild<QHBoxLayout*>();
    if (!bl) {
        NM_LOG("Could not find QHBoxLayout(should contain MainNavButtons and be contained in the QVBoxLayout of MainNavView) in MainNavView, cannot add tab button for NickelMenu main menu.");
        return;
    }

    NM_LOG("Default main menu has %d buttons", bl->count());
    for (int i = 0; i < bl->count(); ++i) {
        QWidget *widget = bl->itemAt(i)->widget();
        if (!widget || !widget->inherits("MainNavButton")) continue;
        NM_LOG("Main menu button %d = %s", i, widget->objectName().toUtf8().constData());

        MainNavButton *btn = widget;
        if (!btn) {
            NM_LOG("qobject_cast<MainNavButton*> failed on button %d", i);
            continue;
        }

        const char *label = nm_main_menu_config(i, "label");
        const char *icon = nm_main_menu_config(i, "icon");
        const char *icon_active = nm_main_menu_config(i, "icon_active");

        const char *enabled = nm_main_menu_config(i, "enabled");
        main_nav_button_configure(btn, label, icon, nullptr, icon_active, nullptr);
        if (enabled) {
            if (strcmp("0", enabled) == 0) {
                NM_LOG("Main menu button %d disabled", i);
                widget->hide();
            } else if (strcmp("1", enabled) == 0) {
                NM_LOG("Main menu button %d explicitly enabled", i);
                widget->show();
            }
        }
    }

    const char *enabled = nm_global_config_experimental("menu_main_15505_enabled");
    if (enabled && strcmp("0", enabled) == 0) {
        NM_LOG("Main menu NickelMenu button disabled");
        return;
    }

    NM_LOG("Adding main menu button in tab bar for firmware 4.23.15505+.");
    MainNavButton *btn = reinterpret_cast<MainNavButton*>(nm_native_storage("MainNavButton", 256));
    if (!btn) {
        NM_LOG("Failed to allocate memory for MainNavButton, cannot add tab button for NickelMenu main menu.");
        return;
    }

    MainNavButton_MainNavButton(btn, parent);
    const char *inactiveIcon = "/tmp/nm_menu_inactive_icon.png";
    const char *activeIcon = "/tmp/nm_menu_active_icon.png";
    const bool hasInactiveIcon = nm_write_default_icon(inactiveIcon, 0);
    const bool hasActiveIcon = nm_write_default_icon(activeIcon, 1);
    main_nav_button_configure(btn,
        nm_global_config_experimental("menu_main_15505_label") ?: "NickelMenu",
        nm_global_config_experimental("menu_main_15505_icon") ?: (hasInactiveIcon ? inactiveIcon : nullptr),
        ":/images/home/main_nav_more.png",
        nm_global_config_experimental("menu_main_15505_icon_active") ?: (hasActiveIcon ? activeIcon : nullptr),
        ":/images/home/main_nav_more_active.png"
    );
    btn->setObjectName("nmButton");

    QPushButton *sh = new QPushButton(_this); // HACK: we use a QPushButton as an adaptor so we can connect an old-style signal with the new-style connect without needing a custom QObject
    if (!QWidget::connect(btn, SIGNAL(tapped()), sh, SIGNAL(pressed()))) {
        NM_LOG("Failed to connect SIGNAL(tapped()) on TouchLabel to SIGNAL(pressed()) on the QPushButton shim, cannot add tab button for NickelMenu main menu.");
        return;
    }
    sh->setVisible(false);

    QWidget::connect(sh, &QPushButton::pressed, [btn] {
        if (!NickelTouchMenu_NickelTouchMenu || !MenuTextItem_MenuTextItem || !MenuTextItem_setText || !MenuTextItem_registerForTapGestures) {
            NM_LOG("could not find required NickelTouchMenu and MenuTextItem symbols for generating menu");
            nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Could not find required NickelTouchMenu and MenuTextItem symbols for generating menu (this is a bug)."));
            return;
        }

        NM_LOG("checking for config updates");
        int rev = nm_global_config_update();
        NM_LOG("revision = %d", rev);

        NM_LOG("building menu");

        size_t items_n;
        nm_menu_item_t **items = nm_global_config_items(&items_n);

        if (!items) {
            NM_LOG("failed to get menu items");
            nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Failed to get menu items (this might be a bug)."));
            return;
        }

        NickelTouchMenu *menu = reinterpret_cast<NickelTouchMenu*>(nm_native_storage("NickelTouchMenu", 512));
        if (!menu) {
            NM_LOG("failed to allocate memory for menu");
            nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Failed to allocate memory for menu."));
            return;
        }

        NickelTouchMenu_NickelTouchMenu(menu, nullptr, 3);

        const size_t builtIn = NM_QT_MAJOR >= 6 ? 1 : 0;
        for (size_t i = 0; i < items_n + builtIn; i++) {
            const bool featureSettings = builtIn && i == items_n;
            nm_menu_item_t *it = featureSettings ? nullptr : items[i];
            if (!featureSettings && it->loc != NM_MENU_LOCATION(main))
                continue;

            const char *label = featureSettings ? "NickelMenu Settings" : it->lbl;
            NM_LOG("adding item '%s'...", label);

            // based on _ZN23SelectionMenuController18createMenuTextItemEP7QWidgetRK7QString
            // (also see _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_, which seems to do the gestures itself instead of calling registerForTapGestures)

            MenuTextItem *mti = reinterpret_cast<MenuTextItem*>(nm_native_storage("MenuTextItem", 256));
            if (!mti) {
                NM_LOG("failed to allocate memory for config item");
                menu->deleteLater();
                nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Failed to allocate memory for menu item."));
                return;
            }

            MenuTextItem_MenuTextItem(mti, menu, false, true);
            MenuTextItem_setText(mti, QString::fromUtf8(label));
            MenuTextItem_registerForTapGestures(mti); // this only makes the MenuTextItem::tapped signal connect so it highlights on tap, doesn't apply to the QAction::triggered below (which needs another GestureReceiver somewhere)
            bool enabled = true;
#if NM_QT_MAJOR >= 6
            if (!featureSettings) enabled = nm_item_permissions_enabled(it);
            nm_set_menu_widget_enabled(mti, enabled);
#endif

            // based on _ZN22AbstractMenuController12createActionEP5QMenuP7QWidgetbbb

            QWidgetAction *ac = new QWidgetAction(menu);
            ac->setDefaultWidget(mti);
            ac->setEnabled(enabled);

            menu->addAction(ac);

            QWidget::connect(ac, &QAction::triggered, menu, &QMenu::hide);

            if (i != items_n + builtIn - 1)
                menu->addSeparator();

            // shim so we don't need to deal with GestureReceiver directly like _ZN28AbstractNickelMenuController18createMenuTextItemEP5QMenuRK7QStringbbS4_ does
            // (similar to _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc)

            if (!QWidget::connect(mti, SIGNAL(tapped(bool)), ac, SIGNAL(triggered()))) {
                NM_LOG("could not handle touch events for menu item (connection of SIGNAL(tapped(bool)) on MenuTextItem to SIGNAL(triggered()) on QWidgetAction failed)");
                menu->deleteLater();
                nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Could not attach touch event handlers to menu item (this is a bug)."));
                return;
            }

            // event handler

            QObject::connect(ac, &QAction::triggered, [it, featureSettings, ac](bool) {
                if (!ac->isEnabled()) return;
                if (featureSettings) {
#if NM_QT_MAJOR >= 6
                    QTimer::singleShot(250, [] { nm_show_feature_settings(); });
#endif
                    return;
                }
                NM_LOG("item '%s' pressed...", it->lbl);
                nm_menu_item_do(it, NULL, NULL);
                NM_LOG("done");
            }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
        }

        NM_LOG("showing menu");

        QWidget::connect(menu, &QMenu::aboutToHide, menu, &QWidget::deleteLater);

        menu->ensurePolished();
        menu->popup(btn->mapToGlobal(btn->geometry().topRight() - QPoint(0, menu->sizeHint().height())));
    });

    bl->addWidget(btn, 1);
    _this->ensurePolished();

    NM_LOG("Added button.");
}

extern "C" __attribute__((visibility("default"))) MainNavView *_nm_menu_hook2(MainNavView *_this, QWidget *parent) {
    NM_LOG("MainNavView::MainNavView(%p, %p)", _this, parent);
    // ARM C++ constructors return this; preserve it even when injection stops early.
    MainNavView *result = MainNavView_MainNavView(_this, parent);
    nm_main_nav_inject(_this, parent);
    return result;
}

// _nm_menu_hook4_item gets/sets the current menu item. It must only be called
// by one thread at a time. The item will be cleared after it has been used.
nm_menu_item_t *_nm_menu_hook4_item(nm_menu_item_t *it) {
    static nm_menu_item_t *its = NULL;
    if (it)
        return (its = it);
    if (!its)
        return NULL;
    nm_menu_item_t *tmp = its;
    its = NULL;
    return tmp;
}

typedef struct {
    QString const& selection;
} nm_selmenu_argtransform_data_t;
char *_nm_selmenu_argtransform(void *data, const char *arg);
static void nm_selection_inject(SelectionMenuController *, SelectionMenuView *, nm_menu_location_t);

extern "C" __attribute__((visibility("default"))) void _nm_selection_search(SelectionMenuController *_this, bool full) {
    SelectionMenuController_setupSearchOptions(_this, full);
    // Firmware 6 adds web search rows directly, bypassing the controller hook.
    if (!SelectionMenuController_lookupWikipedia && SelectionMenuController_menuView)
        nm_selection_inject(_this, SelectionMenuController_menuView(_this), NM_MENU_LOCATION(selection_search));
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook3(SelectionMenuController *_this, SelectionMenuView* smv, MenuTextItem* mti, const char *slot) {
    NM_LOG("hook3: %p %p %p %s", _this, smv, mti, slot);
    SelectionMenuController_addMenuItem(_this, smv, mti, slot);

    // this is important for another reason other than positioning: it only displays if Volume::canSearch()
    nm_menu_location_t loc;
    if (!strcmp(slot, "1showSearchOptions()")) //libnickel 4.20.14622 * _ZN23SelectionMenuController17showSearchOptionsEv
        loc = NM_MENU_LOCATION(selection);
    else if (!strcmp(slot, "1onSearchInBook()")) //libnickel 4.20.14601 * _ZN23SelectionMenuController14onSearchInBookEv
        loc = NM_MENU_LOCATION(selection); // this one is shown instead if parental controls are enabled
    else if (!strcmp(slot, "2lookupWeb()")) //libnickel 4.20.14622 4.30.18838 _ZN23SelectionMenuController9lookupWebEv
        loc = NM_MENU_LOCATION(selection_search);
    else if (!strcmp(slot, "2lookupGoogle()")) //libnickel 4.31.19086 * _ZN23SelectionMenuController12lookupGoogleEv _ZN23SelectionMenuController12lookupGoogleERK7QString _ZN23SelectionMenuController9lookupWebEv
        loc = NM_MENU_LOCATION(selection_search);
    else if (!strcmp(slot, "2lookupBaidu()")) //libnickel 4.31.19086 * _ZN23SelectionMenuController11lookupBaiduEv _ZN23SelectionMenuController11lookupBaiduERK7QString
        loc = NM_MENU_LOCATION(selection_search);
    else
        return;

    nm_selection_inject(_this, smv, loc);
}

static void nm_selection_inject(SelectionMenuController *_this, SelectionMenuView *smv, nm_menu_location_t loc) {
    const bool modern = !SelectionMenuController_lookupWikipedia;
    if (!smv || !SelectionMenuView_addMenuItem || !MenuTextItem_MenuTextItem || !MenuTextItem_setText || !MenuTextItem_registerForTapGestures
            || (modern ? (!PageImageReader_selectedText || !SelectionMenuController_close || !SelectionMenuController_clearSelection)
                       : !WebSearchMixinBase_doWikipediaSearch)) {
        NM_LOG("selection menu dependencies unavailable");
        return;
    }

    NM_LOG("checking for config updates");
    int rev = nm_global_config_update();
    NM_LOG("revision = %d", rev);

    NM_LOG("adding items");

    size_t items_n;
    nm_menu_item_t **items = nm_global_config_items(&items_n);

    if (!items) {
        NM_LOG("failed to get menu items");
        nm_show_dialog(QLatin1String("NickelMenu"), QLatin1String("Failed to get menu items (this might be a bug)."));
        return;
    }

    for (size_t i = 0; i < items_n; i++) {
        nm_menu_item_t *it = items[i];
        if (it->loc != loc)
            continue;

        NM_LOG("adding item '%s'...", it->lbl);

        // based on _ZN23SelectionMenuController18createMenuTextItemEP7QWidgetRK7QString

        MenuTextItem *mti = reinterpret_cast<MenuTextItem*>(nm_native_storage("MenuTextItem", 256));
        if (!mti) {
            NM_LOG("failed to allocate memory for config item");
            return;
        }

        MenuTextItem_MenuTextItem(mti, smv, false, true);
        MenuTextItem_setText(mti, QString::fromUtf8(it->lbl));
        MenuTextItem_registerForTapGestures(mti);
#if NM_QT_MAJOR >= 6
        nm_set_menu_widget_enabled(mti, nm_item_permissions_enabled(it));
#endif

        // based on _ZN23SelectionMenuController18setupSearchOptionsEb and _ZN23SelectionMenuController11addMenuItemEP17SelectionMenuViewP12MenuTextItemPKc, plus some custom stuff for better compatibility

        QPushButton *sh = new QPushButton(smv); // HACK: we use a QPushButton as an adaptor so we can connect an old-style signal with the new-style connect without needing a custom QObject
        if (!QWidget::connect(mti, SIGNAL(tapped(bool)), sh, SIGNAL(pressed()))) {
            NM_LOG("Failed to connect SIGNAL(tapped(bool)) on MenuTextItem to SIGNAL(pressed()) on the QPushButton shim, cannot add custom selection menu item.");
            return;
        }
        sh->setVisible(false);

        QObject::connect(sh, &QPushButton::pressed, [_this, it]() {
#if NM_QT_MAJOR >= 6
            if (!nm_item_permissions_enabled(it)) return;
#endif
            NM_LOG("item '%s' pressed...", it->lbl);
            if (!SelectionMenuController_lookupWikipedia) {
                // Firmware 6 keeps the live selection in the reader, while the
                // current Bookmark can still have empty text. Use the same
                // accessor as its native search menu, without private offsets.
                QWidget *reader = nullptr;
                for (auto *widget : QApplication::allWidgets()) {
                    if (!widget->isVisible() || !widget->inherits("PageImageReader")) continue;
                    if (reader) {
                        nm_show_dialog(QStringLiteral("NickelMenu"), QStringLiteral("More than one active reader."));
                        return;
                    }
                    reader = widget;
                }
                if (!reader || !PageImageReader_selectedText) {
                    nm_show_dialog(QStringLiteral("NickelMenu"), QStringLiteral("Selected text is unavailable."));
                    return;
                }
                const QString selection = PageImageReader_selectedText(reader);
                SelectionMenuController_close(_this);
                SelectionMenuController_clearSelection(_this);
                nm_selmenu_argtransform_data_t data{selection};
                nm_menu_item_do(it, _nm_selmenu_argtransform, &data);
                return;
            }
            _nm_menu_hook4_item(it); // this is safe since it is a pointer captured by value
            NM_LOG("triggering lookupWikipedia() slot");
            SelectionMenuController_lookupWikipedia(_this);
        });

        SelectionMenuView_addMenuItem(smv, mti);
    }
}

char *_nm_selmenu_argtransform(void *data, const char *arg) {
    nm_selmenu_argtransform_data_t *d = (nm_selmenu_argtransform_data_t*)(data);

    QString src = QString::fromUtf8(arg), res;
    QRegularExpression re = QRegularExpression("\\{([1])\\|([aAfnsSuwx]*)\\|([\"$%]*)\\}");

    for (QString x = src; x.length() > 0;) {
        QRegularExpressionMatch m = re.match(x);

        if (!m.hasMatch()) {
            res += x;
            x = x.mid(x.length());
            continue;
        }

        QString tmp;

        for (int k = 0; k < m.capturedLength(1); k++) {
            switch (m.captured(1).at(k).toLatin1()) {
            case '1': tmp = d->selection; break;
            }
        }

        for (int k = 0; k < m.capturedLength(2); k++) {
            switch (m.captured(2).at(k).toLatin1()) {
            case 'a': tmp = tmp.toLower(); break;
            case 'A': tmp = tmp.toUpper(); break;
            case 'f': tmp = tmp.split(QRegularExpression("\\s")).first(); break;
            case 'n': tmp = tmp.remove(QRegularExpression("[^0-9a-zA-Z]")); break;
            case 's': tmp = tmp.trimmed(); break;
            case 'S': tmp = tmp.simplified(); break;
            case 'x': if (tmp.length() == 0) { nm_err_set("argtransform: empty substitution result for %s", qPrintable(m.captured(0))); return NULL; }; break;
            case 'w': tmp = tmp.remove(QRegularExpression("\\s")); break;
            case 'u': tmp = tmp.replace(QRegularExpression("\\s"), "_"); break;
            }
        }

        for (int k = 0; k < m.capturedLength(3); k++) {
            switch (m.captured(3).at(k).toLatin1()) {
            case '"':
                // Escape existing backslashes before adding escapes for other characters.
                tmp = tmp
                    .replace("\\", "\\\\")
                    .replace("\"", "\\\"")
                    .replace("\n", "\\n")
                    .replace("\b", "\\b")
                    .replace("\t", "\\t")
                    .replace("\f", "\\f")
                    .replace("\r", "\\r");
                // JSON also requires escapes for the remaining control characters.
                for (int c = 0; c < 0x20; ++c)
                    tmp.replace(QChar(c), QStringLiteral("\\u%1").arg(c, 4, 16, QLatin1Char('0')));
                break;
            case '$':
                tmp = tmp.replace("'", "'\"'\"'");
                break;
            case '%':
                tmp = QUrl::toPercentEncoding(tmp);
                break;
            }
        }

        res += x.left(m.capturedStart());
        res += tmp;
        x = x.mid(m.capturedEnd());
    }

    char *x = strdup(res.toUtf8().data());
    if (!x)
        nm_err_set("argtransform: could not allocate memory: %m");
    return x;
}

extern "C" __attribute__((visibility("default"))) void _nm_menu_hook4(WebSearchMixinBase *_this, QString const& selection, QString const& locale) {
    NM_LOG("hook4: %p %s %s", _this, qPrintable(selection), qPrintable(locale));

    nm_menu_item_t *it = _nm_menu_hook4_item(NULL);
    if (!it) {
        NM_LOG("No current menu item, continuing with default wikipedia search.");
        WebSearchMixinBase_doWikipediaSearch(_this, selection, locale);
        return;
    }

    NM_LOG("continuing execution of item %p (%s)", it, it->lbl);
    nm_selmenu_argtransform_data_t data = (nm_selmenu_argtransform_data_t){
        .selection = selection,
    };
    nm_menu_item_do(it, _nm_selmenu_argtransform, (void*)(&data)); // this is safe since data will not be used after this returns
    NM_LOG("done");
}

void _nm_menu_inject(void *nmc, QMenu *menu, nm_menu_location_t loc, int at) {
    NM_LOG("inject %d @ %d", loc, at);
    if (!AbstractNickelMenuController_createMenuTextItem || !AbstractNickelMenuController_createAction)
        return;

    int rev_o = menu->property("nm_config_rev").toInt();

    NM_LOG("checking for config updates (current revision: %d)", rev_o);
    int rev_n = nm_global_config_update(); // if there was an error it will be returned as a menu item anyways (and updated will be true)
    NM_LOG("new revision = %d%s", rev_n, rev_n == rev_o ? "" : " (changed)");

    NM_LOG("checking for existing items added by nm");

    for (auto action : menu->actions()) {
        if (action->property("nm_action") == true) {
            if (rev_o == rev_n)
                return; // already added items, menu is up to date
            menu->removeAction(action);
            delete action;
        }
    }

    NM_LOG("getting insertion point");

    auto actions = menu->actions();
    auto before = at < actions.count()
        ? actions.at(at)
        : nullptr;

    if (before == nullptr)
        NM_LOG("it seems the original item to add new ones before was never actually added to the menu (number of items when the action was created is %d, current is %d), appending to end instead", at, actions.count());

    NM_LOG("injecting new items");

    size_t items_n;
    nm_menu_item_t **items = nm_global_config_items(&items_n);

    if (!items) {
        NM_LOG("items is NULL (either the config hasn't been parsed yet or there was a memory allocation error), not adding");
        return;
    }

    // if it segfaults in createMenuTextItem, it's likely because
    // AbstractNickelMenuController is invalid, which shouldn't happen while the
    // menu which we added the signal from still can be shown... (but
    // theoretically, it's possible)

    for (size_t i = 0; i < items_n; i++) {
        nm_menu_item_t *it = items[i];
        if (it->loc != loc)
            continue;

        NM_LOG("adding item '%s'...", it->lbl);

        MenuTextItem* item = AbstractNickelMenuController_createMenuTextItem(nmc, menu, QString::fromUtf8(it->lbl), false, false, "");
        if (!item) continue;
        bool enabled = true;
#if NM_QT_MAJOR >= 6
        enabled = nm_item_permissions_enabled(it);
#endif
        QAction* action = AbstractNickelMenuController_createAction_before(before, loc, i == items_n-1, nmc, menu, item, true, enabled, true);

        if (!action) { item->deleteLater(); continue; }
        action->setEnabled(enabled);
#if NM_QT_MAJOR >= 6
        nm_set_menu_widget_enabled(item, enabled);
#endif
        QObject::connect(action, &QAction::triggered, [it, action](bool){
            if (!action->isEnabled()) return;
            NM_LOG("item '%s' pressed...", it->lbl);
            nm_menu_item_do(it, NULL, NULL);
            NM_LOG("done");
        }); // note: we're capturing by value, i.e. the pointer to the global variable, rather then the stack variable, so this is safe
    }

    NM_LOG("updating config revision property");
    menu->setProperty("nm_config_rev", rev_n);
}

void nm_menu_item_do(nm_menu_item_t *it, nm_argtransform_t argtransform, void *argtransform_data) {
    const char *err = NULL;
    bool success = true;
    int skip = 0;

    for (nm_menu_action_t *cur = it->action; cur; cur = cur->next) {
        NM_LOG("action %p with argument %s : ", cur->act, cur->arg);
        NM_LOG("...success=%d ; on_success=%d on_failure=%d skip=%d", success, cur->on_success, cur->on_failure, skip);

        if (skip != 0) {
            NM_LOG("...skipping action due to skip flag (remaining=%d)", skip);
            if (skip > 0)
                skip--;
            continue;
        } else if (!((success && cur->on_success) || (!success && cur->on_failure))) {
            NM_LOG("...skipping action due to condition flags");
            continue;
        }

        nm_action_result_t *res = NULL;
        char *transformed = nullptr;
        const char *arg = cur->arg;
        if (argtransform) {
            NM_LOG("...applying argtransform");
            transformed = (*argtransform)(argtransform_data, cur->arg);
            arg = transformed;
            if (arg) NM_LOG("...applied argtransform: %s", arg);
        }
        if (arg) {
#if NM_QT_MAJOR >= 6
            const char *feature = nm_feature_for_action(cur->act, arg);
            if (feature && !nm_feature_enabled(feature))
                nm_err_set("%s is disabled in NickelMenu Settings", nm_feature_label(feature));
            else
#endif
                res = cur->act(arg);
        }
        free(transformed);
        err = nm_err();

        if (err == NULL && res && res->type == NM_ACTION_RESULT_TYPE_SKIP) {
            NM_LOG("...not updating success flag (value=%d) for skip result", success);
        } else if (!(success = err == NULL)) {
            NM_LOG("...error: '%s'", err);
            continue;
        } else if (!res) {
            NM_LOG("...warning: you should have returned a result with type silent, not null, upon success");
            continue;
        }

        NM_LOG("...result: type=%d msg='%s', handling...", res->type, res->msg);

        MainWindowController *mwc;
        switch (res->type) {
        case NM_ACTION_RESULT_TYPE_SILENT:
            break;
        case NM_ACTION_RESULT_TYPE_MSG:
            nm_show_dialog(QString::fromUtf8(it->lbl), QString::fromUtf8(res->msg));
            break;
        case NM_ACTION_RESULT_TYPE_TOAST:
            if (!MainWindowController_sharedInstance || !MainWindowController_toast) {
                NM_LOG("toast unavailable: %s", res->msg);
                break;
            }
            mwc = MainWindowController_sharedInstance();
            if (!mwc) {
                NM_LOG("toast: could not get shared main window controller pointer");
                break;
            }
            MainWindowController_toast(mwc, QString::fromUtf8(res->msg), QStringLiteral(""), 1500);
            break;
        case NM_ACTION_RESULT_TYPE_SKIP:
            skip = res->skip;
            break;
        }

        if (skip == -1)
            NM_LOG("...skipping remaining actions");
        else if (skip != 0)
            NM_LOG("...skipping next %d actions", skip);

        nm_action_result_free(res);
    }

    if (err) {
        NM_LOG("last action returned error %s", err);
        nm_show_dialog(QString::fromUtf8(it->lbl), QString::fromUtf8(err));
    }
}

QAction *AbstractNickelMenuController_createAction_before(QAction *before, nm_menu_location_t loc, bool last_in_group, void *_this, QMenu *menu, QWidget *widget, bool close, bool enabled, bool separator) {
    int n = menu->actions().count();
    QAction* action = AbstractNickelMenuController_createAction(_this, menu, widget, /*close*/false, enabled, /*separator*/false);

    if (!action) return nullptr;
    action->setProperty("nm_action", true);

    if (!menu->actions().contains(action)) {
        NM_LOG("could not find added action at end of menu (note: old count is %d, new is %d), not moving it to the right spot or adding separator", n, menu->actions().count());
        return action;
    }

    if (before != nullptr) {
        menu->removeAction(action);
        menu->insertAction(before, action);
    }

    if (close) {
        // we can't use the signal which createAction can create, as it gets lost when moving the item
        QWidget::connect(action, &QAction::triggered, [=](bool){ menu->hide(); });
    }

    if (separator) {
        // if it's the main menu, we generally want to use a custom separator
        QAction *sep;
        if (loc == NM_MENU_LOCATION(main) && LightMenuSeparator_LightMenuSeparator && BoldMenuSeparator_BoldMenuSeparator) {
            sep = reinterpret_cast<QAction*>(nm_native_storage(last_in_group ? "BoldMenuSeparator" : "LightMenuSeparator", 32));
            if (!sep) return action;
            (last_in_group
                ? BoldMenuSeparator_BoldMenuSeparator
                : LightMenuSeparator_LightMenuSeparator
            )(sep, reinterpret_cast<QWidget*>(_this));
            menu->insertAction(before, sep);
        } else {
            sep = menu->insertSeparator(before);
        }
        sep->setProperty("nm_action", true);
    }

    return action;
}
