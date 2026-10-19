// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "titlemetadata.h"
#include "presencelog.h"
#include "presencevalidation.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{

constexpr qint64 kCacheTtlSecs = 30 * 24 * 3600;
constexpr qint64 kRetryFailedSecs = 3600;
constexpr int kMaxCacheEntries = 5000;
constexpr qint64 kMaxCacheFileBytes = 8 * 1024 * 1024;
constexpr qint64 kMaxResponseBytes = 256 * 1024;
constexpr int kRequestTimeoutMs = 10000;
constexpr int kMaxNameChars = 128;

qint64 NowSecs()
{
	return QDateTime::currentSecsSinceEpoch();
}

bool IsFresh(const GameMeta &meta, qint64 now)
{
	qint64 ttl = meta.found ? kCacheTtlSecs : kRetryFailedSecs;
	return now - meta.fetched_at < ttl;
}

GameMeta Fallback(const QString &title_id)
{
	GameMeta meta;
	meta.name = title_id;
	meta.fetched_at = NowSecs();
	meta.found = false;
	return meta;
}

}

TitleMetadata::TitleMetadata(QObject *parent)
	: QObject(parent),
	network(new QNetworkAccessManager(this))
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	cache_path = dir + "/discord_presence_cache.json";
	Load();
}

void TitleMetadata::Lookup(const QString &title_id)
{
	auto it = cache.constFind(title_id);
	if(it != cache.constEnd() && IsFresh(*it, NowSecs()))
	{
		emit Ready(title_id, *it);
		return;
	}

	// homebrew and system apps are unknown to the metadata sites, don't ask
	if(!PresenceValidation::IsRetailTitleId(title_id))
	{
		GameMeta meta = Fallback(title_id);
		Store(title_id, meta);
		emit Ready(title_id, meta);
		return;
	}

	// one request per title at a time; Ready goes out when it finishes
	if(pending.contains(title_id))
		return;
	pending.insert(title_id);

	bool ps5 = title_id.startsWith("PPSA");
	QString site = ps5 ? QStringLiteral("https://prosperopatches.com") : QStringLiteral("https://orbispatches.com");
	QNetworkRequest request(QUrl(QStringLiteral("%1/api/lookup?titleid=%2").arg(site, title_id)));
	request.setTransferTimeout(kRequestTimeoutMs);
	request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("chiaki-ng-discord-presence"));
	qCInfo(chiakiDiscord) << "Looking up" << title_id << "on" << site;
	QNetworkReply *reply = network->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply, title_id]() {
		OnReplyFinished(reply, title_id);
	});
}

void TitleMetadata::OnReplyFinished(QNetworkReply *reply, const QString &title_id)
{
	reply->deleteLater();
	pending.remove(title_id);

	GameMeta meta = Fallback(title_id);
	if(reply->error() != QNetworkReply::NoError)
	{
		qCWarning(chiakiDiscord) << "Lookup of" << title_id << "failed:" << reply->errorString();
	}
	else
	{
		QJsonObject metadata = QJsonDocument::fromJson(reply->read(kMaxResponseBytes)).object().value("metadata").toObject();
		QString name = PresenceValidation::SanitizeText(metadata.value("name").toString(), kMaxNameChars);
		if(name.isEmpty())
		{
			qCWarning(chiakiDiscord) << "No name for" << title_id << "in the lookup response";
		}
		else
		{
			meta.name = name;
			meta.found = true;
			QString icon = metadata.value("icon").toString();
			if(PresenceValidation::IsSafeHttpsUrl(icon))
				meta.image_url = icon;
			QString site = title_id.startsWith("PPSA") ? QStringLiteral("https://prosperopatches.com") : QStringLiteral("https://orbispatches.com");
			meta.game_url = site + "/" + title_id;
		}
	}

	Store(title_id, meta);
	emit Ready(title_id, meta);
}

void TitleMetadata::Store(const QString &title_id, const GameMeta &meta)
{
	if(cache.size() >= kMaxCacheEntries && !cache.contains(title_id))
	{
		auto oldest = cache.begin();
		for(auto it = cache.begin(); it != cache.end(); ++it)
			if(it->fetched_at < oldest->fetched_at)
				oldest = it;
		cache.erase(oldest);
	}
	cache.insert(title_id, meta);
	Save();
}

void TitleMetadata::Load()
{
	QFile file(cache_path);
	if(!file.exists() || file.size() > kMaxCacheFileBytes || !file.open(QIODevice::ReadOnly))
		return;
	QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
	for(auto it = root.constBegin(); it != root.constEnd(); ++it)
	{
		if(!PresenceValidation::IsValidTitleId(it.key()))
			continue;
		QJsonObject entry = it.value().toObject();
		GameMeta meta;
		meta.name = PresenceValidation::SanitizeText(entry.value("name").toString(), kMaxNameChars);
		meta.image_url = entry.value("image_url").toString();
		meta.game_url = entry.value("game_url").toString();
		meta.fetched_at = entry.value("fetched_at").toInteger();
		meta.found = entry.value("found").toBool();
		if(meta.name.isEmpty())
			continue;
		if(!PresenceValidation::IsSafeHttpsUrl(meta.image_url))
			meta.image_url.clear();
		if(!PresenceValidation::IsSafeHttpsUrl(meta.game_url))
			meta.game_url.clear();
		cache.insert(it.key(), meta);
	}
	qCInfo(chiakiDiscord) << "Loaded" << cache.size() << "cached titles from" << cache_path;
}

void TitleMetadata::Save() const
{
	QJsonObject root;
	for(auto it = cache.constBegin(); it != cache.constEnd(); ++it)
	{
		root.insert(it.key(), QJsonObject {
			{ "name", it->name },
			{ "image_url", it->image_url },
			{ "game_url", it->game_url },
			{ "fetched_at", it->fetched_at },
			{ "found", it->found },
		});
	}
	QDir().mkpath(QFileInfo(cache_path).absolutePath());
	QSaveFile file(cache_path);
	if(!file.open(QIODevice::WriteOnly))
	{
		qCWarning(chiakiDiscord) << "Can't write" << cache_path;
		return;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	file.commit();
}
