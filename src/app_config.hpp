#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QUrl>

#ifndef PLAINWIRE_VERSION
#define PLAINWIRE_VERSION "0.0.0"
#endif

namespace plainwire {

inline const QString kAppName = QStringLiteral("Plainwire");
inline const QString kDefaultOrigin = QStringLiteral("https://plainwi.re");
inline const QString kDefaultHost = QStringLiteral("plainwi.re");
inline const QString kInstanceName = QStringLiteral("me.kokonico.plainwire.desktop.v2");
inline constexpr qsizetype kMaxDeepLinkRouteLength = 2048;
inline constexpr qsizetype kMaxRouteValueLength = 512;
inline constexpr qsizetype kMaxInstancePayloadBytes = 16 * 1024;

// The active server origin, persisted per user so self-hosted Plainwire
// servers can be used next to the official one. Every policy/load decision in
// the app reads this through ServerConfig::instance().
class ServerConfig {
public:
    static ServerConfig &instance() {
        static ServerConfig config;
        return config;
    }

    QString origin() const { return origin_; }
    QString host() const { return host_; }
    int port() const { return port_; }
    bool isDefault() const { return origin_ == kDefaultOrigin; }

    // Normalize and persist the active origin. Returns false (and leaves the
    // active config untouched) when the value is not a usable HTTP(S) origin.
    bool setOrigin(const QString &raw) {
        const QString normalized = normalizedOrigin(raw);
        if (normalized.isEmpty()) return false;
        applyNormalized(normalized);
        QSettings settings;
        settings.setValue(QStringLiteral("server/origin"), origin_);
        settings.sync();
        return true;
    }

    void resetToDefault() {
        setOrigin(kDefaultOrigin);
    }

    QStringList customHosts() const {
        QSettings settings;
        return settings.value(QStringLiteral("server/customHosts")).toStringList();
    }

    // Normalizes raw before storing so a host always appears once, exactly as
    // it would be navigated to.
    void addCustomHost(const QString &raw) {
        const QString normalized = normalizedOrigin(raw);
        if (normalized.isEmpty()) return;
        QSettings settings;
        QStringList hosts = customHosts();
        if (!hosts.contains(normalized)) hosts.append(normalized);
        settings.setValue(QStringLiteral("server/customHosts"), hosts);
        settings.sync();
    }

    void removeCustomHost(const QString &origin) {
        QSettings settings;
        QStringList hosts = customHosts();
        hosts.removeAll(origin);
        settings.setValue(QStringLiteral("server/customHosts"), hosts);
        settings.sync();
    }

    static QString normalizedOrigin(const QString &raw) {
        const QUrl url = QUrl(raw.trimmed());
        if (!url.isValid()) return {};
        const QString scheme = url.scheme().toLower();
        if (scheme != QStringLiteral("https") && scheme != QStringLiteral("http")) return {};
        const QString host = url.host().toLower();
        if (host.isEmpty() || !url.userName().isEmpty() || !url.password().isEmpty()) return {};
        if (!url.path().isEmpty() && url.path() != QStringLiteral("/")) return {};
        if (!url.query().isEmpty() || !url.fragment().isEmpty()) return {};

        QString origin = scheme + QStringLiteral("://") + host;
        const int port = url.port(-1);
        if (port != -1) origin += u':' + QString::number(port);
        return origin;
    }

private:
    ServerConfig() { load(); }

    void load() {
        origin_ = kDefaultOrigin;
        host_ = kDefaultHost;
        port_ = -1;
        QSettings settings;
        const QString stored = settings.value(QStringLiteral("server/origin")).toString();
        if (stored.isEmpty()) return;
        const QString normalized = normalizedOrigin(stored);
        if (!normalized.isEmpty()) applyNormalized(normalized);
    }

    void applyNormalized(const QString &normalized) {
        const QUrl url(normalized);
        origin_ = normalized;
        host_ = url.host().toLower();
        port_ = url.port(-1);
    }

    QString origin_;
    QString host_;
    int port_;
};

} // namespace plainwire