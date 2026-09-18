#pragma once

#include <QString>

#ifndef PLAINWIRE_VERSION
#define PLAINWIRE_VERSION "0.0.0"
#endif

namespace plainwire {

inline const QString kAppName = QStringLiteral("Plainwire");
inline const QString kOrigin = QStringLiteral("https://plainwi.re");
inline const QString kHost = QStringLiteral("plainwi.re");
inline const QString kInstanceName = QStringLiteral("me.kokonico.plainwire.desktop.v2");
inline constexpr qsizetype kMaxDeepLinkRouteLength = 2048;
inline constexpr qsizetype kMaxRouteValueLength = 512;
inline constexpr qsizetype kMaxInstancePayloadBytes = 16 * 1024;

} // namespace plainwire
