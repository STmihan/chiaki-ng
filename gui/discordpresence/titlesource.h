// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_DISCORDPRESENCE_TITLESOURCE_H
#define CHIAKI_DISCORDPRESENCE_TITLESOURCE_H

#include <QByteArray>
#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

/**
 * Reads the running title from the ps5-rpc payload (or etaHEN's Discord RPC
 * server) on the console. Both push one line per state: a title ID or
 * "No game running.".
 */
class TitleSource : public QObject
{
	Q_OBJECT

	public:
		TitleSource(const QString &host, quint16 port, QObject *parent = nullptr);

		void Start();
		void Stop();

	signals:
		/** title_id is empty when no game is running */
		void TitleReceived(const QString &title_id);
		/** emitted once after the second failed connection in a row */
		void Unavailable();

	private:
		void Connect();
		void HandleLost();
		void AbortQuietly();
		void OnReadyRead();
		void HandleLine(const QByteArray &line);

		QString host;
		quint16 port;
		QTcpSocket *socket;
		QTimer *reconnect_timer;
		QTimer *watchdog;
		QByteArray buffer;
		bool running = false;
		bool aborting = false;
		int failures = 0;
};

#endif // CHIAKI_DISCORDPRESENCE_TITLESOURCE_H
