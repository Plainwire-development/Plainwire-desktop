#pragma once

#include <QByteArray>
#include <QHash>
#include <QLocalServer>
#include <QString>
#include <QStringList>

class QLocalSocket;

namespace plainwire {

class MainWindow;

class SingleInstance final {
public:
    explicit SingleInstance(QString name);

    bool forwardToExisting(const QStringList &arguments);
    bool listen(MainWindow *window);

private:
    QString name_;
    QLocalServer server_;
    QHash<QLocalSocket *, QByteArray> buffers_;
};

} // namespace plainwire
