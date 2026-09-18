#include "main_window.hpp"

#include "app_config.hpp"
#include "plainwire_page.hpp"
#include "url_policy.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QResizeEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QWebEngineDesktopMediaRequest>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineNotification>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <cstddef>
#include <vector>

namespace plainwire {
namespace {

QString profileRoot() {
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(root);
    return root;
}

QString resourceText(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(file.readAll());
}

QString injectedScript() {
    return QStringLiteral(R"JS(
(() => {
  'use strict';
  const ORIGIN = 'https://plainwi.re';
  if (location.origin !== ORIGIN || window.top !== window) return;

  document.documentElement.classList.add('plainwire-desktop');

  try {
    Object.defineProperty(window, '__PLAINWIRE_DESKTOP__', {
      configurable: false,
      enumerable: false,
      writable: false,
      value: Object.freeze({
        active: true,
        version: '%1',
        publicOrigin: ORIGIN,
        shell: 'qtwebengine',
        engine: 'chromium',
        features: Object.freeze({
          persistentSession: true,
          nativeNotifications: true,
          nativeDownloads: true,
          screenCapture: true,
          deepLinks: true
        })
      })
    });
  } catch (_) {}

  const KEY = 'plainwire.desktop.lastRoute.v3';
  const LEGACY_KEY = 'plainwire.desktop.lastRoute.v2';
  const valid = (route) => typeof route === 'string'
    && route.startsWith('#')
    && route.length <= 2048
    && !/[\u0000-\u001f\u007f]/.test(route);

  try {
    const saved = localStorage.getItem(KEY) || localStorage.getItem(LEGACY_KEY);
    if (!location.hash && valid(saved)) {
      history.replaceState(history.state, '', location.pathname + location.search + saved);
      localStorage.setItem(KEY, saved);
    }
  } catch (_) {}

  const save = () => {
    try {
      if (valid(location.hash)) localStorage.setItem(KEY, location.hash);
    } catch (_) {}
  };
  addEventListener('hashchange', save, { passive: true });
  addEventListener('pagehide', save, { passive: true });

  const reconcile = () => {
    if (!document.hidden) {
      try { dispatchEvent(new Event('online')); } catch (_) {}
    }
  };
  addEventListener('pageshow', reconcile, { passive: true });
  document.addEventListener('visibilitychange', reconcile, { passive: true });
})();
)JS").arg(QStringLiteral(PLAINWIRE_VERSION));
}

QString desktopPolishScript() {
    const QString css = resourceText(QStringLiteral(":/plainwire/resources/desktop-polish.css"));
    if (css.isEmpty()) return {};

    QJsonArray encoded;
    encoded.append(css);
    const QString json = QString::fromUtf8(QJsonDocument(encoded).toJson(QJsonDocument::Compact));

    return QStringLiteral(R"JS(
(() => {
  'use strict';
  if (location.origin !== 'https://plainwi.re' || window.top !== window) return;
  document.documentElement.classList.add('plainwire-desktop');
  const old = document.getElementById('plainwire-desktop-polish');
  if (old) old.remove();
  const style = document.createElement('style');
  style.id = 'plainwire-desktop-polish';
  style.textContent = %1[0];
  (document.head || document.documentElement).appendChild(style);

  // Media can appear in long histories. Decode it off the critical scroll path.
  for (const image of document.querySelectorAll('#messages img, .messages img')) {
    image.decoding = 'async';
    if (image.loading === 'auto') image.loading = 'lazy';
  }

  const rawImageData = /data:image\/(?:png|jpe?g|gif|webp|avif);base64,[A-Za-z0-9+/=\s]{512,}/gi;

  const scrubRawImageText = (root) => {
    if (!(root instanceof Element)) return;
    const bodies = root.matches('.msg-body') ? [root] : root.querySelectorAll('.msg-body');
    for (const body of bodies) {
      const walker = document.createTreeWalker(body, NodeFilter.SHOW_TEXT);
      while (walker.nextNode()) {
        const node = walker.currentNode;
        if (node.nodeValue && rawImageData.test(node.nodeValue)) {
          rawImageData.lastIndex = 0;
          node.nodeValue = node.nodeValue.replace(rawImageData, '[image data hidden]');
        }
        rawImageData.lastIndex = 0;
      }
    }
  };

  const markMedia = (root) => {
    if (!(root instanceof Element)) return;
    const images = root.matches('img') ? [root] : root.querySelectorAll('img');
    for (const image of images) {
      if (!image.closest('#messages .msg-body, .messages .msg-body, #messages .embed, .messages .embed')) continue;
      image.decoding = 'async';
      if (image.loading === 'auto') image.loading = 'lazy';
    }
    scrubRawImageText(root);
  };

  markMedia(document.body);
  const observer = new MutationObserver((records) => {
    for (const record of records) {
      for (const node of record.addedNodes) markMedia(node);
    }
  });
  observer.observe(document.body, { childList: true, subtree: true });
})();
)JS").arg(json);
}

QString permissionDescription(QWebEnginePermission::PermissionType type) {
    using Type = QWebEnginePermission::PermissionType;
    switch (type) {
        case Type::Notifications:
            return QStringLiteral("show desktop notifications");
        case Type::MediaAudioCapture:
            return QStringLiteral("use your microphone");
        case Type::MediaVideoCapture:
            return QStringLiteral("use your camera");
        case Type::MediaAudioVideoCapture:
            return QStringLiteral("use your microphone and camera");
        case Type::DesktopVideoCapture:
            return QStringLiteral("share your screen or a window");
        case Type::DesktopAudioVideoCapture:
            return QStringLiteral("share your screen and desktop audio");
        default:
            return {};
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setObjectName(QStringLiteral("PlainwireMainWindow"));
    setWindowTitle(kAppName);
    setWindowIcon(QIcon(QStringLiteral(":/plainwire/resources/plainwire.png")));
    setMinimumSize(780, 560);
    resize(1320, 840);

    createProfile();
    createWebView();
    createActions();
    createTray();
    createNoticeOverlay();
    restoreWindowState();

    view_->load(QUrl(kOrigin + QStringLiteral("/")));
}

MainWindow::~MainWindow() = default;

void MainWindow::openTarget(const QUrl &url) {
    const QUrl target = deepLinkTarget(url);
    if (target.isValid()) view_->setUrl(target);
    showAndFocus();
}

void MainWindow::showAndFocus() {
    if (isMinimized()) showNormal();
    show();
    raise();
    activateWindow();
    view_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::handleArguments(const QStringList &args) {
    for (const QString &arg : args) {
        const QUrl url(arg);
        if (url.isValid() && (url.scheme() == QStringLiteral("plainwire") || isPlainwireUrl(url))) {
            openTarget(url);
            return;
        }
    }
    showAndFocus();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveWindowState();
    if (tray_ && tray_->isVisible() && !quitting_) {
        event->ignore();
        hide();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    positionNotice();
}

void MainWindow::createProfile() {
    profile_ = new QWebEngineProfile(QStringLiteral("Plainwire"), this);
    const QString root = profileRoot();
    const QString storage = QDir(root).filePath(QStringLiteral("chromium-profile"));
    const QString cache = QDir(root).filePath(QStringLiteral("chromium-cache"));
    QDir().mkpath(storage);
    QDir().mkpath(cache);

    profile_->setPersistentStoragePath(storage);
    profile_->setCachePath(cache);
    profile_->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    profile_->setHttpCacheMaximumSize(384 * 1024 * 1024);
    profile_->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    profile_->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    profile_->setDownloadPath(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    profile_->setHttpAcceptLanguage(QLocale().bcp47Name());
    profile_->setSpellCheckEnabled(true);
    profile_->setPushServiceEnabled(false);

    QString ua = profile_->httpUserAgent();
    if (!ua.contains(QStringLiteral("PlainwireDesktop/"))) {
        ua += QStringLiteral(" PlainwireDesktop/") + QStringLiteral(PLAINWIRE_VERSION);
        profile_->setHttpUserAgent(ua);
    }

    QWebEngineSettings *settings = profile_->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, false);
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    settings->setUnknownUrlSchemePolicy(QWebEngineSettings::DisallowUnknownUrlSchemes);

    QWebEngineScript bootstrap;
    bootstrap.setName(QStringLiteral("plainwire-desktop-bootstrap"));
    bootstrap.setInjectionPoint(QWebEngineScript::DocumentCreation);
    bootstrap.setWorldId(QWebEngineScript::MainWorld);
    bootstrap.setRunsOnSubFrames(false);
    bootstrap.setSourceCode(injectedScript());
    profile_->scripts()->insert(bootstrap);

    const QString polishSource = desktopPolishScript();
    if (!polishSource.isEmpty()) {
        QWebEngineScript polish;
        polish.setName(QStringLiteral("plainwire-desktop-polish"));
        polish.setInjectionPoint(QWebEngineScript::DocumentReady);
        polish.setWorldId(QWebEngineScript::MainWorld);
        polish.setRunsOnSubFrames(false);
        polish.setSourceCode(polishSource);
        profile_->scripts()->insert(polish);
    }

    connect(profile_, &QWebEngineProfile::downloadRequested, this,
            [this](QWebEngineDownloadRequest *download) {
        if (!download) return;
        const QString suggested = download->suggestedFileName().isEmpty()
            ? QStringLiteral("download") : download->suggestedFileName();
        const QString initial = QDir(profile_->downloadPath()).filePath(suggested);
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save download"), initial);
        if (path.isEmpty()) {
            download->cancel();
            return;
        }

        const QFileInfo info(path);
        download->setDownloadDirectory(info.absolutePath());
        download->setDownloadFileName(info.fileName());
        const QString destination = info.absoluteFilePath();

        connect(download, &QWebEngineDownloadRequest::stateChanged, this,
                [this, download, destination](QWebEngineDownloadRequest::DownloadState state) {
            using State = QWebEngineDownloadRequest::DownloadState;
            if (state == State::DownloadCompleted) {
                showNotice(QStringLiteral("Saved %1").arg(QFileInfo(destination).fileName()));
            } else if (state == State::DownloadInterrupted) {
                const QString reason = download->interruptReasonString().trimmed();
                showNotice(reason.isEmpty()
                    ? QStringLiteral("Download failed")
                    : QStringLiteral("Download failed: %1").arg(reason), 8000);
            }
        });

        download->accept();
    });

    profile_->setNotificationPresenter([this](std::unique_ptr<QWebEngineNotification> notification) {
        if (!notification || !isPlainwireUrl(notification->origin())) return;
        if (notification_) {
            notification_->close();
            notification_.reset();
        }

        notification_ = std::move(notification);
        QWebEngineNotification *active = notification_.get();
        connect(active, &QWebEngineNotification::closed, this, [this, active] {
            QTimer::singleShot(0, this, [this, active] {
                if (notification_.get() == active) notification_.reset();
            });
        });

        notification_->show();
        if (tray_ && tray_->isVisible()) {
            tray_->showMessage(notification_->title().left(120), notification_->message().left(600),
                               QSystemTrayIcon::Information, 8000);
        } else {
            QApplication::alert(this, 8000);
        }

        QTimer::singleShot(10000, this, [this, active] {
            if (notification_.get() != active) return;
            notification_->close();
            notification_.reset();
        });
    });
}

void MainWindow::createWebView() {
    view_ = new QWebEngineView(this);
    page_ = new PlainwirePage(profile_, [this](const QString &message) {
        showNotice(message, 7000);
    }, view_);
    view_->setPage(page_);
    setCentralWidget(view_);

    connect(page_, &QWebEnginePage::permissionRequested, this,
            [this](QWebEnginePermission permission) {
        if (!isPlainwireUrl(permission.origin())) {
            permission.deny();
            return;
        }

        using Type = QWebEnginePermission::PermissionType;
        if (permission.permissionType() == Type::ClipboardReadWrite) {
            permission.grant();
            return;
        }

        const QString description = permissionDescription(permission.permissionType());
        if (description.isEmpty()) {
            permission.deny();
            return;
        }

        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("Plainwire permission"),
            QStringLiteral("Allow Plainwire to %1?").arg(description),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer == QMessageBox::Yes) permission.grant();
        else permission.deny();
    });

    connect(page_, &QWebEnginePage::desktopMediaRequested, this,
            [this](const QWebEngineDesktopMediaRequest &request) {
        struct Source { bool screen; QModelIndex index; QString label; };
        std::vector<Source> sources;
        QStringList labels;

        const auto appendModel = [&](QAbstractItemModel *model, bool screen, const QString &prefix) {
            if (!model) return;
            for (int row = 0; row < model->rowCount(); ++row) {
                const QModelIndex index = model->index(row, 0);
                const QString name = model->data(index, Qt::DisplayRole).toString();
                const QString label = prefix + (name.isEmpty() ? QString::number(row + 1) : name);
                sources.push_back(Source{screen, index, label});
                labels.push_back(label);
            }
        };

        appendModel(request.screensModel(), true, QStringLiteral("Screen: "));
        appendModel(request.windowsModel(), false, QStringLiteral("Window: "));

        if (sources.empty()) {
            request.cancel();
            showNotice(QStringLiteral("No screens or windows are available to share."));
            return;
        }

        bool ok = false;
        const QString selected = QInputDialog::getItem(
            this,
            QStringLiteral("Share your screen"),
            QStringLiteral("Choose what Plainwire can share:"),
            labels,
            0,
            false,
            &ok);
        if (!ok) {
            request.cancel();
            return;
        }

        const qsizetype chosen = labels.indexOf(selected);
        if (chosen < 0 || chosen >= static_cast<qsizetype>(sources.size())) {
            request.cancel();
            return;
        }

        const Source &source = sources[static_cast<std::size_t>(chosen)];
        if (source.screen) request.selectScreen(source.index);
        else request.selectWindow(source.index);
    });

    connect(page_, &QWebEnginePage::fullScreenRequested, this,
            [this](QWebEngineFullScreenRequest request) {
        request.accept();
        if (request.toggleOn()) showFullScreen();
        else showNormal();
    });

    connect(page_, &QWebEnginePage::newWindowRequested, this,
            [this](QWebEngineNewWindowRequest &request) {
        const QUrl requested = request.requestedUrl();
        if (!isPlainwireUrl(requested)) {
            if (isSafeExternalUrl(requested)) QDesktopServices::openUrl(requested);
            return;
        }

        auto *popup = new QMainWindow();
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->setWindowTitle(kAppName);
        popup->setWindowIcon(windowIcon());
        popup->setMinimumSize(640, 480);
        popup->resize(980, 720);

        auto *popupView = new QWebEngineView(popup);
        auto *popupPage = new PlainwirePage(profile_, [this](const QString &message) {
            showNotice(message, 7000);
        }, popupView);
        popupView->setPage(popupPage);
        popup->setCentralWidget(popupView);
        popup->show();
        request.openIn(popupPage);
    });

    connect(page_, &QWebEnginePage::windowCloseRequested, this, [this] { close(); });
    connect(view_, &QWebEngineView::titleChanged, this, [this](const QString &title) {
        const QString clean = title.trimmed().left(140);
        setWindowTitle(clean.isEmpty() ? kAppName : clean);
    });
    connect(view_, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (ok) {
            recoveringRenderer_ = false;
            return;
        }
        showNotice(QStringLiteral("Plainwire could not load. Check your connection and press Ctrl+R to retry."), 8000);
    });
    connect(page_, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus status, int) {
        if (status == QWebEnginePage::NormalTerminationStatus) return;
        if (recoveringRenderer_) {
            showNotice(QStringLiteral("The renderer stopped again. Press Ctrl+R to retry."), 9000);
            return;
        }
        recoveringRenderer_ = true;
        showNotice(QStringLiteral("The Plainwire renderer stopped unexpectedly. Reloading…"), 6000);
        QTimer::singleShot(500, this, [this] {
            if (view_) view_->reload();
        });
    });

    // Keep the realtime client active while hidden to tray instead of allowing
    // Chromium to freeze an occluded page and stall sockets/calls.
    page_->setLifecycleState(QWebEnginePage::LifecycleState::Active);
    connect(page_, &QWebEnginePage::recommendedStateChanged, this,
            [this](QWebEnginePage::LifecycleState) {
        if (page_) page_->setLifecycleState(QWebEnginePage::LifecycleState::Active);
    });
}

void MainWindow::createActions() {
    const auto addShortcut = [this](const QString &shortcut, auto callback) {
        auto *action = new QAction(this);
        action->setShortcut(QKeySequence(shortcut));
        action->setShortcutContext(Qt::ApplicationShortcut);
        addAction(action);
        connect(action, &QAction::triggered, this, callback);
        return action;
    };

    addShortcut(QStringLiteral("Ctrl+R"), [this] { view_->reload(); });
    addShortcut(QStringLiteral("Ctrl+Shift+R"), [this] {
        page_->triggerAction(QWebEnginePage::ReloadAndBypassCache);
        showNotice(QStringLiteral("Reloading without cache…"), 2500);
    });
    addShortcut(QStringLiteral("Ctrl+,"), [this] {
        view_->setUrl(QUrl(kOrigin + QStringLiteral("/#settings")));
    });
    addShortcut(QStringLiteral("Ctrl++"), [this] { setZoomFactor(zoomFactor_ + 0.1); });
    addShortcut(QStringLiteral("Ctrl+="), [this] { setZoomFactor(zoomFactor_ + 0.1); });
    addShortcut(QStringLiteral("Ctrl+-"), [this] { setZoomFactor(zoomFactor_ - 0.1); });
    addShortcut(QStringLiteral("Ctrl+0"), [this] { setZoomFactor(1.0); });
    addShortcut(QStringLiteral("F11"), [this] {
        if (isFullScreen()) showNormal();
        else showFullScreen();
    });
    addShortcut(QStringLiteral("Esc"), [this] {
        if (isFullScreen()) showNormal();
    });
}

void MainWindow::createTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QApplication::setQuitOnLastWindowClosed(true);
        return;
    }

    tray_ = new QSystemTrayIcon(windowIcon(), this);
    auto *menu = new QMenu(this);
    QAction *openAction = menu->addAction(QStringLiteral("Open Plainwire"));
    QAction *reloadAction = menu->addAction(QStringLiteral("Reload"));
    QAction *settingsAction = menu->addAction(QStringLiteral("Settings"));
    menu->addSeparator();
    QAction *quitAction = menu->addAction(QStringLiteral("Quit"));
    tray_->setContextMenu(menu);
    tray_->setToolTip(kAppName);

    connect(openAction, &QAction::triggered, this, [this] { showAndFocus(); });
    connect(reloadAction, &QAction::triggered, this, [this] {
        view_->reload();
        showAndFocus();
    });
    connect(settingsAction, &QAction::triggered, this, [this] {
        view_->setUrl(QUrl(kOrigin + QStringLiteral("/#settings")));
        showAndFocus();
    });
    connect(quitAction, &QAction::triggered, this, [this] {
        quitting_ = true;
        saveWindowState();
        qApp->quit();
    });
    connect(tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) showAndFocus();
    });
    connect(tray_, &QSystemTrayIcon::messageClicked, this, [this] {
        if (notification_) {
            notification_->click();
            notification_->close();
            notification_.reset();
        }
        showAndFocus();
    });
    tray_->show();
}

void MainWindow::createNoticeOverlay() {
    noticeLabel_ = new QLabel(this);
    noticeLabel_->setObjectName(QStringLiteral("PlainwireNotice"));
    noticeLabel_->setWordWrap(true);
    noticeLabel_->setAlignment(Qt::AlignCenter);
    noticeLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    noticeLabel_->setStyleSheet(QStringLiteral(
        "QLabel#PlainwireNotice {"
        " color: #f5f7fb;"
        " background: rgba(25, 29, 38, 235);"
        " border: 1px solid rgba(255,255,255,38);"
        " border-radius: 12px;"
        " padding: 10px 14px;"
        " font-weight: 600;"
        "}"));
    noticeLabel_->hide();

    noticeTimer_ = new QTimer(this);
    noticeTimer_->setSingleShot(true);
    connect(noticeTimer_, &QTimer::timeout, noticeLabel_, &QLabel::hide);
}

void MainWindow::showNotice(const QString &message, int timeoutMs) {
    if (!noticeLabel_) return;
    const QString clean = message.simplified().left(700);
    if (clean.isEmpty()) return;

    noticeLabel_->setText(clean);
    const int available = qMax(260, width() - 48);
    const int targetWidth = qMin(560, available);
    noticeLabel_->setMinimumWidth(qMin(320, targetWidth));
    noticeLabel_->setMaximumWidth(targetWidth);
    noticeLabel_->adjustSize();
    positionNotice();
    noticeLabel_->show();
    noticeLabel_->raise();
    noticeTimer_->start(qMax(1200, timeoutMs));
}

void MainWindow::positionNotice() {
    if (!noticeLabel_ || !noticeLabel_->isVisible()) return;
    const QSize size = noticeLabel_->sizeHint().boundedTo(QSize(qMax(260, width() - 48), 180));
    noticeLabel_->resize(size);
    noticeLabel_->move((width() - size.width()) / 2, qMax(18, height() - size.height() - 28));
    noticeLabel_->raise();
}

void MainWindow::setZoomFactor(double factor, bool announce) {
    zoomFactor_ = qBound(0.75, factor, 1.50);
    if (view_) view_->setZoomFactor(zoomFactor_);

    QSettings settings;
    settings.setValue(QStringLiteral("view/zoomFactor"), zoomFactor_);
    if (announce) {
        showNotice(QStringLiteral("Zoom %1%").arg(qRound(zoomFactor_ * 100.0)), 1800);
    }
}

void MainWindow::restoreWindowState() {
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    const QByteArray state = settings.value(QStringLiteral("window/state")).toByteArray();
    if (!state.isEmpty()) restoreState(state);
    setZoomFactor(settings.value(QStringLiteral("view/zoomFactor"), 1.0).toDouble(), false);
}

void MainWindow::saveWindowState() {
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("view/zoomFactor"), zoomFactor_);
    settings.sync();
}

} // namespace plainwire
