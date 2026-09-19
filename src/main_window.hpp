#pragma once

#include <QMainWindow>
#include <QStringList>

#include <memory>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QResizeEvent;
class QStackedWidget;
class QSystemTrayIcon;
class QTabBar;
class QTimer;
class QUrl;
class QWebEngineNotification;
class QWebEngineProfile;
class QWebEngineView;
class QWidget;

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
    void createTabs();
    void createWebView();
    void createActions();
    void createTray();
    void createNoticeOverlay();
    void restoreWindowState();
    void saveWindowState();
    void showNotice(const QString &message, int timeoutMs = 5000);
    void positionNotice();
    void setZoomFactor(double factor, bool announce = true);

    QWidget *createServersPanel();
    void refreshServerPanel();
    void addServerFromUi();
    void removeServerFromUi();
    void applyServerFromUi();
    void resetServerFromUi();
    void openServersTab();
    void switchToChat();
    void applyServerScripts();
    void reloadForServer();

    QWebEngineProfile *profile_ = nullptr;
    QWebEngineView *view_ = nullptr;
    PlainwirePage *page_ = nullptr;
    QSystemTrayIcon *tray_ = nullptr;
    QLabel *noticeLabel_ = nullptr;
    QTimer *noticeTimer_ = nullptr;
    QTabBar *tabBar_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QWidget *serversPanel_ = nullptr;
    QComboBox *serverCombo_ = nullptr;
    QLineEdit *customHostEdit_ = nullptr;
    std::unique_ptr<QWebEngineNotification> notification_;
    bool quitting_ = false;
    bool recoveringRenderer_ = false;
    double zoomFactor_ = 1.0;
};

} // namespace plainwire
