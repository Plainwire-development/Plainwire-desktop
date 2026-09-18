#include "url_policy.hpp"

#include "app_config.hpp"

#include <QChar>
#include <QString>
#include <QUrlQuery>

namespace plainwire {
namespace {

bool hasCredentials(const QUrl &url) {
    return !url.userName().isEmpty() || !url.password().isEmpty();
}

bool validRouteValue(const QString &value) {
    if (value.isEmpty() || value.size() > kMaxRouteValueLength) return false;
    for (const QChar ch : value) {
        if (!(ch.isLetterOrNumber()
              || ch == u'-' || ch == u'_' || ch == u'.' || ch == u'~')) {
            return false;
        }
    }
    return true;
}

} // namespace

bool isPlainwireUrl(const QUrl &url) {
    if (!url.isValid() || hasCredentials(url)) return false;
    const int port = url.port(-1);
    return url.scheme() == QStringLiteral("https")
        && url.host().compare(kHost, Qt::CaseInsensitive) == 0
        && (port == -1 || port == 443);
}

bool isSafeExternalUrl(const QUrl &url) {
    if (!url.isValid() || hasCredentials(url)) return false;
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("https")
        || scheme == QStringLiteral("http")
        || scheme == QStringLiteral("mailto");
}

QUrl deepLinkTarget(const QUrl &url) {
    if (isPlainwireUrl(url)) return url;
    if (url.scheme() != QStringLiteral("plainwire") || hasCredentials(url)) return {};

    const QString action = url.host().toLower();
    if (action == QStringLiteral("open")) {
        const QString route = QUrlQuery(url).queryItemValue(QStringLiteral("route"), QUrl::FullyDecoded);
        if (!route.startsWith(u'#') || route.size() > kMaxDeepLinkRouteLength) return {};
        for (const QChar ch : route) {
            if (ch.category() == QChar::Other_Control) return {};
        }
        return QUrl(kOrigin + QStringLiteral("/") + route);
    }

    QString value = url.path();
    while (value.startsWith(u'/')) value.remove(0, 1);
    while (value.endsWith(u'/')) value.chop(1);
    if (!validRouteValue(value)) return {};

    QString webAction;
    if (action == QStringLiteral("wire") || action == QStringLiteral("invite")) webAction = QStringLiteral("wire");
    else if (action == QStringLiteral("dm")) webAction = QStringLiteral("dm");
    else if (action == QStringLiteral("channel")) webAction = QStringLiteral("channel");
    else if (action == QStringLiteral("server")) webAction = QStringLiteral("server");
    else if (action == QStringLiteral("profile")) webAction = QStringLiteral("profile");
    else return {};

    return QUrl(kOrigin + QStringLiteral("/#") + webAction + u'/' + value);
}

} // namespace plainwire
