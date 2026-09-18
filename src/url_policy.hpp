#pragma once

#include <QUrl>

namespace plainwire {

bool isPlainwireUrl(const QUrl &url);
bool isSafeExternalUrl(const QUrl &url);
QUrl deepLinkTarget(const QUrl &url);

} // namespace plainwire
