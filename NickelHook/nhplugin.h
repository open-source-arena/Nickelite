#ifdef __cplusplus
#include <QImageIOHandler>
#include <QObject>
#include <QtPlugin>

// we make it a fake image plugin so we can have Qt automatically load it
// without needing extra configuration (e.g. LD_PRELOAD, -plugin arg, etc).

// note: this class does not need to be renamed when using NickelHook, as
// duplicate classes don't matter for separate plugins
class NHPlugin : public QImageIOPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "nhplugin.json")
public:
    Capabilities capabilities(QIODevice*, QByteArray const&) const override { return {}; };
    QImageIOHandler *create(QIODevice*, QByteArray const& = QByteArray()) const override { return nullptr; };
};

#endif
