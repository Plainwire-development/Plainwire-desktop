#include "app_config.hpp"
#include "url_policy.hpp"

#include <QCoreApplication>
#include <QString>
#include <QUrl>

#include <iostream>

namespace {

bool expect(bool condition, const char *message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("PlainwireDesktopTests"));
    app.setApplicationName(QStringLiteral("url-policy"));
    Q_UNUSED(app);
    bool ok = true;

    ok &= expect(plainwire::ServerConfig::instance().isDefault(),
                 "fresh config should point at the official server");
    ok &= expect(plainwire::isPlainwireUrl(QUrl(QStringLiteral("https://plainwi.re/#dm/12"))),
                 "production Plainwire URL should be accepted");
    ok &= expect(!plainwire::isPlainwireUrl(QUrl(QStringLiteral("http://plainwi.re/"))),
                 "plaintext Plainwire URL should be rejected");
    ok &= expect(!plainwire::isPlainwireUrl(QUrl(QStringLiteral("https://user:pass@plainwi.re/"))),
                 "credential-bearing Plainwire URL should be rejected");
    ok &= expect(!plainwire::isPlainwireUrl(QUrl(QStringLiteral("https://plainwire.kokonico.me/"))),
                 "legacy production host should not be treated as the active app origin");

    const QUrl dm = plainwire::deepLinkTarget(QUrl(QStringLiteral("plainwire://dm/42")));
    ok &= expect(dm == QUrl(QStringLiteral("https://plainwi.re/#dm/42")),
                 "DM deep link should route to plainwi.re");

    const QUrl settings = plainwire::deepLinkTarget(
        QUrl(QStringLiteral("plainwire://open?route=%23settings")));
    ok &= expect(settings == QUrl(QStringLiteral("https://plainwi.re/#settings")),
                 "open deep link should preserve a safe hash route");

    ok &= expect(plainwire::ServerConfig::normalizedOrigin(QStringLiteral("https://selfhost.example.com"))
                     == QStringLiteral("https://selfhost.example.com"),
                 "self-hosted origin should be accepted");
    ok &= expect(plainwire::ServerConfig::normalizedOrigin(QStringLiteral("https://selfhost.example.com:8443"))
                     == QStringLiteral("https://selfhost.example.com:8443"),
                 "self-hosted origin with a port should be accepted");
    ok &= expect(plainwire::ServerConfig::normalizedOrigin(QStringLiteral("ftp://selfhost.example.com")).isEmpty(),
                 "non-HTTP servers should be rejected");
    ok &= expect(plainwire::ServerConfig::instance().setOrigin(QStringLiteral("https://selfhost.example.com")),
                 "switching to a self-hosted server should succeed");
    ok &= expect(plainwire::isPlainwireUrl(QUrl(QStringLiteral("https://selfhost.example.com/#dm/12"))),
                 "self-hosted Plainwire URL should be accepted");
    ok &= expect(!plainwire::isPlainwireUrl(QUrl(QStringLiteral("https://plainwi.re/#dm/12"))),
                 "official host should not be accepted after switching to a self-hosted server");
    ok &= expect(!plainwire::isPlainwireUrl(QUrl(QStringLiteral("https://selfhost.example.com:8443/#dm/12"))),
                 "mismatched port on the self-hosted server should be rejected");
    const QUrl selfDm = plainwire::deepLinkTarget(QUrl(QStringLiteral("plainwire://dm/42")));
    ok &= expect(selfDm == QUrl(QStringLiteral("https://selfhost.example.com/#dm/42")),
                 "DM deep link should route to the active self-hosted server");
    plainwire::ServerConfig::instance().resetToDefault();
    ok &= expect(plainwire::isPlainwireUrl(QUrl(QStringLiteral("https://plainwi.re/#dm/12"))),
                 "resetting should restore the official server");

    ok &= expect(!plainwire::deepLinkTarget(
                     QUrl(QStringLiteral("plainwire://open?route=%23dm%2F12%0Aevil"))).isValid(),
                 "control characters in deep links should be rejected");
    ok &= expect(plainwire::isSafeExternalUrl(QUrl(QStringLiteral("https://example.com/"))),
                 "HTTPS external links should be accepted");
    ok &= expect(plainwire::isSafeExternalUrl(QUrl(QStringLiteral("mailto:test@example.com"))),
                 "mailto links should be accepted");
    ok &= expect(!plainwire::isSafeExternalUrl(QUrl(QStringLiteral("javascript:alert(1)"))),
                 "javascript external links should be rejected");
    ok &= expect(!plainwire::isSafeExternalUrl(QUrl(QStringLiteral("file:///tmp/test"))),
                 "file external links should be rejected");

    if (!ok) return 1;
    std::cout << "Plainwire URL policy tests passed.\n";
    return 0;
}
