// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QUrl>
namespace BaronService {
inline QUrl migrate(QUrl url) {
    if (url.scheme() == "https" && url.host() == "orchestrion.su"
        && url.port(443) == 443 && url.userInfo().isEmpty()) {
        url.setHost("orchestrion.ink");
        url.setPort(-1);
    }
    return url;
}
}
