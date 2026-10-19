// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "presenceactivity.h"
#include "presencevalidation.h"
#include "titlemetadata.h"

#include <QJsonArray>

namespace
{

// Art asset uploaded to the Discord application under this key
const QString kBadgeKey = QStringLiteral("ps5");

enum StatusDisplayType
{
	StatusDisplayName = 0,
	StatusDisplayState = 1,
	StatusDisplayDetails = 2,
};

QString PlatformLine(const QString &title_id)
{
	return title_id.startsWith("CUSA") ? QStringLiteral("PS4 game on PS5") : QStringLiteral("on PS5");
}

}

namespace PresenceActivity
{

QJsonObject Game(const QString &title_id, const GameMeta &meta, qint64 start_ts)
{
	QString name = PresenceValidation::SanitizeText(meta.name, 128);
	if(name.isEmpty())
		name = QStringLiteral("Unknown title");
	QString image = PresenceValidation::IsSafeHttpsUrl(meta.image_url) ? meta.image_url : kBadgeKey;

	QJsonObject activity {
		{ "type", 0 }, // Playing
		{ "details", name },
		{ "state", PlatformLine(title_id) },
		// without this the member list says "Playing <app name>" instead of the game
		{ "status_display_type", StatusDisplayDetails },
		{ "assets", QJsonObject {
			{ "large_image", image },
			{ "large_text", name },
			{ "small_image", kBadgeKey },
			{ "small_text", QStringLiteral("PS5") },
		} },
		{ "timestamps", QJsonObject {
			{ "start", start_ts },
		} },
	};
	if(PresenceValidation::IsSafeHttpsUrl(meta.game_url))
	{
		activity.insert("buttons", QJsonArray {
			QJsonObject {
				{ "label", QStringLiteral("View game") },
				{ "url", meta.game_url },
			},
		});
	}
	return activity;
}

QJsonObject Idle()
{
	return QJsonObject {
		{ "type", 0 },
		{ "details", QStringLiteral("Idle") },
		{ "state", QStringLiteral("No game running") },
		{ "assets", QJsonObject {
			{ "large_image", kBadgeKey },
			{ "large_text", QStringLiteral("PS5") },
		} },
	};
}

}
