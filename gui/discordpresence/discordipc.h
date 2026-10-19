// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_DISCORDPRESENCE_DISCORDIPC_H
#define CHIAKI_DISCORDPRESENCE_DISCORDIPC_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>

#include <optional>

class QLocalSocket;
class QTimer;

/**
 * Minimal client for the local Discord RPC socket (named pipe discord-ipc-N on
 * Windows, unix socket elsewhere). Remembers the wanted activity and (re)sends it
 * whenever the connection to the Discord app becomes ready.
 */
class DiscordIpc : public QObject
{
	Q_OBJECT

	public:
		DiscordIpc(const QString &client_id, QObject *parent = nullptr);

		void Start();
		/** Clears the activity and closes the connection; Discord drops the status with it. */
		void Stop();

		void SetActivity(const QJsonObject &activity);
		void ClearActivity();

	private:
		enum class State { Idle, Connecting, Handshaking, Ready };

		void BeginConnect();
		void TryNextCandidate();
		void HandleLost();
		void OnConnected();
		void OnReadyRead();
		void HandleFrame(quint32 opcode, const QByteArray &payload);
		void SendFrame(quint32 opcode, const QByteArray &payload);
		void SendActivity();

		QString client_id;
		QLocalSocket *socket;
		QTimer *retry_timer;
		State state = State::Idle;
		bool running = false;
		bool reported_missing = false;
		QStringList candidates;
		int candidate_index = 0;
		QByteArray buffer;
		quint64 nonce = 0;
		std::optional<QJsonObject> activity;
};

#endif // CHIAKI_DISCORDPRESENCE_DISCORDIPC_H
