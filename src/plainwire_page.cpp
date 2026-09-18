#include "plainwire_page.hpp"

#include "url_policy.hpp"

#include <QDesktopServices>

#include <utility>

namespace plainwire {

PlainwirePage::PlainwirePage(QWebEngineProfile *profile,
                             NoticeHandler noticeHandler,
                             QObject *parent)
    : QWebEnginePage(profile, parent),
      noticeHandler_(std::move(noticeHandler)) {}

bool PlainwirePage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) {
    Q_UNUSED(type);
    if (!isMainFrame) return true;
    if (isPlainwireUrl(url)) return true;
    if (isSafeExternalUrl(url)) QDesktopServices::openUrl(url);
    return false;
}

void PlainwirePage::javaScriptAlert(const QUrl &securityOrigin, const QString &message) {
    if (isPlainwireUrl(securityOrigin) && noticeHandler_) {
        const QString cleaned = message.trimmed().left(700);
        if (!cleaned.isEmpty()) noticeHandler_(cleaned);
        return;
    }
    QWebEnginePage::javaScriptAlert(securityOrigin, message);
}

} // namespace plainwire
