// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_DISCORDPRESENCE_TITLEMETADATA_H
#define CHIAKI_DISCORDPRESENCE_TITLEMETADATA_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct GameMeta
{
	QString name;
	/** https URL, or empty to fall back to the badge asset */
	QString image_url;
	/** page of the game on prosperopatches/orbispatches, may be empty */
	QString game_url;
	qint64 fetched_at = 0;
	bool found = false;
};

/**
 * Name and cover art for a title ID from prosperopatches.com (PS5) or
 * orbispatches.com (PS4). Results are cached on disk so a title is requested at
 * most once per 30 days, or once per hour while the lookup keeps failing.
 */
class TitleMetadata : public QObject
{
	Q_OBJECT

	public:
		explicit TitleMetadata(QObject *parent = nullptr);

		/** Emits Ready for title_id, right away when cached, otherwise after one request. */
		void Lookup(const QString &title_id);

	signals:
		void Ready(const QString &title_id, const GameMeta &meta);

	private:
		void OnReplyFinished(QNetworkReply *reply, const QString &title_id);
		void Store(const QString &title_id, const GameMeta &meta);
		void Load();
		void Save() const;

		QNetworkAccessManager *network;
		QHash<QString, GameMeta> cache;
		QSet<QString> pending;
		QString cache_path;
};

#endif // CHIAKI_DISCORDPRESENCE_TITLEMETADATA_H
