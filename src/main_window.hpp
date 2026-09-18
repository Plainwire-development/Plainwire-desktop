#pragma once

#include <QMainWindow>
#include <QStringList>

#include <memory>

class QCloseEvent;
class QLabel;
class QResizeEvent;
class QSystemTrayIcon;
class QTimer;
class QUrl;
class QWebEngineNotification;
class QWebEngineProfile;
class QWebEngineView;

namespace plainwire {

class PlainwirePage;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openTarget(const QUrl &url);
    void showAndFocus();
    void handleArguments(const QStringList &args);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void createProfile();
    void createWebView();
    void createActions();
    void createTray();
    void createNoticeOverlay();
    void restoreWindowState();
    void saveWindowState();
    void showNotice(const QString &message, int timeoutMs = 5000);
    void positionNotice();
    void setZoomFactor(double factor, bool announce = true);

    QWebEngineProfile *profile_ = nullptr;
    QWebEngineView *view_ = nullptr;
    PlainwirePage *page_ = nullptr;
    QSystemTrayIcon *tray_ = nullptr;
    QLabel *noticeLabel_ = nullptr;
    QTimer *noticeTimer_ = nullptr;
    std::unique_ptr<QWebEngineNotification> notification_;
    bool quitting_ = false;
    bool recoveringRenderer_ = false;
    double zoomFactor_ = 1.0;
};

} // namespace plainwire
