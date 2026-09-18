#include "app_config.hpp"
#include "main_window.hpp"
#include "single_instance.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QIcon>
#include <QGuiApplication>
#include <QMessageBox>
#include <QTimer>

int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(plainwire::kAppName);
    QCoreApplication::setApplicationVersion(QStringLiteral(PLAINWIRE_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("Plainwire"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("plainwi.re"));
    QApplication::setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(QStringLiteral(":/plainwire/resources/plainwire.png")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Plainwire desktop client"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("url"),
        QStringLiteral("Optional plainwire:// or Plainwire HTTPS link"),
        QStringLiteral("[url]"));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    plainwire::SingleInstance instance(plainwire::kInstanceName);
    if (instance.forwardToExisting(args)) return 0;

    plainwire::MainWindow window;
    if (!instance.listen(&window)) {
        QMessageBox::critical(
            nullptr,
            plainwire::kAppName,
            QStringLiteral("Plainwire could not create its single-instance socket."));
        return 1;
    }

    window.show();
    if (!args.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, args] { window.handleArguments(args); });
    }
    return app.exec();
}
