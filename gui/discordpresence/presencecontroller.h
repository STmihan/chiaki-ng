// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_DISCORDPRESENCE_PRESENCECONTROLLER_H
#define CHIAKI_DISCORDPRESENCE_PRESENCECONTROLLER_H

#include <QObject>
#include <QString>

class DiscordIpc;
class StreamSession;
class TitleMetadata;
class TitleSource;
struct GameMeta;

/**
 * Shows the game running on the console as Discord Rich Presence while a
 * stream session is connected, and removes it as soon as the session ends.
 */
class PresenceController : public QObject
{
	Q_OBJECT

	public:
		static PresenceController *Instance();

		void Attach(StreamSession *session);

	private:
		explicit PresenceController(QObject *parent);

		void Begin(StreamSession *session);
		void End();
		void OnTitleReceived(const QString &title_id);
		void OnUnavailable();
		void OnMetaReady(const QString &title_id, const GameMeta &meta);

		DiscordIpc *discord;
		TitleMetadata *metadata;
		TitleSource *source = nullptr;
		/** identity only, never dereferenced: the session may already be gone */
		const StreamSession *active_session = nullptr;
		bool has_state = false;
		QString current_title;
		qint64 start_ts = 0;
};

#endif // CHIAKI_DISCORDPRESENCE_PRESENCECONTROLLER_H
