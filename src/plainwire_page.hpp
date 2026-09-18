#pragma once

#include <QWebEnginePage>

#include <functional>

class QWebEngineProfile;

namespace plainwire {

class PlainwirePage final : public QWebEnginePage {
public:
    using NoticeHandler = std::function<void(const QString &)>;

    explicit PlainwirePage(QWebEngineProfile *profile,
                           NoticeHandler noticeHandler = {},
                           QObject *parent = nullptr);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    void javaScriptAlert(const QUrl &securityOrigin, const QString &message) override;

private:
    NoticeHandler noticeHandler_;
};

} // namespace plainwire
