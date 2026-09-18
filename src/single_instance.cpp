#include "single_instance.hpp"

#include "app_config.hpp"
#include "main_window.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLocalSocket>

#include <utility>

namespace plainwire {

SingleInstance::SingleInstance(QString name)
    : name_(std::move(name)) {}

bool SingleInstance::forwardToExisting(const QStringList &arguments) {
    QLocalSocket socket;
    socket.connectToServer(name_, QIODevice::WriteOnly);
    if (!socket.waitForConnected(160)) return false;

    QJsonArray array;
    for (const QString &argument : arguments) array.push_back(argument);
    const QByteArray payload = QJsonDocument(array).toJson(QJsonDocument::Compact) + '\n';
    if (payload.size() > kMaxInstancePayloadBytes) return false;

    socket.write(payload);
    socket.flush();
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen(MainWindow *window) {
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_.listen(name_)) {
        // A crashed process can leave a stale local socket. Probe before
        // removing it so a live primary instance is never disrupted.
        QLocalSocket probe;
        probe.connectToServer(name_, QIODevice::WriteOnly);
        if (probe.waitForConnected(250)) return false;
        QLocalServer::removeServer(name_);
        if (!server_.listen(name_)) return false;
    }

    QObject::connect(&server_, &QLocalServer::newConnection, window, [this, window] {
        while (QLocalSocket *socket = server_.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::readyRead, window, [this, socket, window] {
                QByteArray &buffer = buffers_[socket];
                buffer += socket->readAll();
                if (buffer.size() > kMaxInstancePayloadBytes) {
                    buffers_.remove(socket);
                    socket->abort();
                    return;
                }

                const qsizetype newline = buffer.indexOf('\n');
                if (newline < 0) return;

                const QByteArray payload = buffer.left(newline);
                buffers_.remove(socket);
                const QJsonDocument doc = QJsonDocument::fromJson(payload);
                QStringList args;
                if (doc.isArray()) {
                    for (const QJsonValue &value : doc.array()) {
                        if (value.isString()) args.push_back(value.toString());
                    }
                }
                window->handleArguments(args);
                socket->disconnectFromServer();
            });

            QObject::connect(socket, &QLocalSocket::disconnected, socket, [this, socket] {
                buffers_.remove(socket);
                socket->deleteLater();
            });
        }
    });
    return true;
}

} // namespace plainwire
