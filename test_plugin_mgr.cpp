
#include <QCoreApplication>
#include <QDebug>
#include <kwin/pluginmanager.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    KWin::PluginManager mgr;
    qDebug() << "Available plugins:" << mgr.availablePlugins();
    qDebug() << "Loaded plugins:" << mgr.loadedPlugins();
    return 0;
}
