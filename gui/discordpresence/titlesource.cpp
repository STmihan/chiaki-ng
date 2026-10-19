// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "titlesource.h"
#include "presencelog.h"
#include "presencevalidation.h"

#include <QTcpSocket>
#include <QTimer>

namespace
{

constexpr int kReconnectMs = 5000;
// the payload repeats its line every 10 s, so silence this long means a dead link
// (also bounds how long a connect attempt to a sleeping console may hang)
constexpr int kWatchdogMs = 30000;
constexpr int kMaxBufferBytes = 4096;
constexpr int kMaxLineChars = 64;

}

TitleSource::TitleSource(const QString &host, quint16 port, QObject *parent)
	: QObject(parent),
	host(host),
	port(port),
	socket(new QTcpSocket(this)),
	reconnect_timer(new QTimer(this)),
	watchdog(new QTimer(this))
{
	reconnect_timer->setSingleShot(true);
	reconnect_timer->setInterval(kReconnectMs);
	connect(reconnect_timer, &QTimer::timeout, this, &TitleSource::Connect);

	watchdog->setSingleShot(true);
	watchdog->setInterval(kWatchdogMs);
	connect(watchdog, &QTimer::timeout, this, [this]() {
		qCInfo(chiakiDiscord) << "No data from the payload for" << kWatchdogMs / 1000 << "s, reconnecting";
		HandleLost();
	});

	connect(socket, &QTcpSocket::readyRead, this, &TitleSource::OnReadyRead);
	connect(socket, &QTcpSocket::disconnected, this, &TitleSource::HandleLost);
	connect(socket, &QTcpSocket::errorOccurred, this, &TitleSource::HandleLost);
}

void TitleSource::Start()
{
	running = true;
	failures = 0;
	Connect();
}

void TitleSource::Stop()
{
	running = false;
	reconnect_timer->stop();
	watchdog->stop();
	AbortQuietly();
}

void TitleSource::AbortQuietly()
{
	// abort() emits disconnected synchronously, which must not count as a loss
	aborting = true;
	socket->abort();
	aborting = false;
}

void TitleSource::Connect()
{
	if(!running)
		return;
	AbortQuietly();
	reconnect_timer->stop();
	buffer.clear();
	watchdog->start();
	socket->connectToHost(host, port);
}

void TitleSource::HandleLost()
{
	// disconnected and errorOccurred usually both fire for one loss
	if(!running || aborting || reconnect_timer->isActive())
		return;
	watchdog->stop();
	QString error = socket->errorString();
	AbortQuietly();
	failures++;
	if(failures == 1)
		qCInfo(chiakiDiscord) << "Payload not reachable on" << host << "port" << port << "-" << error;
	// one failure can be a hiccup, two in a row mean the payload is gone
	if(failures == 2)
		emit Unavailable();
	reconnect_timer->start();
}

void TitleSource::OnReadyRead()
{
	buffer.append(socket->readAll());
	if(buffer.size() > kMaxBufferBytes)
	{
		qCWarning(chiakiDiscord) << "Oversized line from the payload, reconnecting";
		HandleLost();
		return;
	}
	int idx;
	while((idx = buffer.indexOf('\n')) >= 0)
	{
		QByteArray line = buffer.left(idx);
		buffer.remove(0, idx + 1);
		HandleLine(line);
	}
}

void TitleSource::HandleLine(const QByteArray &line)
{
	QString text = PresenceValidation::SanitizeText(QString::fromUtf8(line), kMaxLineChars);
	if(text.isEmpty())
		return;

	QString title_id;
	QString low = text.toLower();
	if(low.contains("no game") || low.contains("not running") || low == "none")
	{
		title_id = QString();
	}
	else
	{
		QString token = text.section(' ', 0, 0, QString::SectionSkipEmpty);
		// anything else ("Failed to get title ID.") keeps the previous state
		if(!PresenceValidation::IsValidTitleId(token))
			return;
		title_id = token;
	}

	watchdog->start();
	if(failures > 0)
		qCInfo(chiakiDiscord) << "Payload connected on" << host << "port" << port;
	failures = 0;
	emit TitleReceived(title_id);
}
