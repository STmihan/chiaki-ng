// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_DISCORDPRESENCE_PRESENCEACTIVITY_H
#define CHIAKI_DISCORDPRESENCE_PRESENCEACTIVITY_H

#include <QJsonObject>
#include <QString>

struct GameMeta;

// Activity payloads for SET_ACTIVITY, laid out like smokeyxd/ps5-rust-rpc does.
namespace PresenceActivity
{

QJsonObject Game(const QString &title_id, const GameMeta &meta, qint64 start_ts);
QJsonObject Idle();

}

#endif // CHIAKI_DISCORDPRESENCE_PRESENCEACTIVITY_H
