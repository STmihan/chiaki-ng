// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "discordipc.h"
#include "presencelog.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>
#include <QtEndian>

namespace
{

enum Opcode : quint32
{
	OpHandshake = 0,
	OpFrame = 1,
	OpClose = 2,
	OpPing = 3,
	OpPong = 4,
};

constexpr int kRetryMs = 15000;
constexpr int kHeaderSize = 8;
constexpr quint32 kMaxFrameSize = 64 * 1024;
constexpr int kPipeCount = 10;

QStringList CandidateNames()
{
	QStringList names;
#ifdef Q_OS_WIN
	// QLocalSocket turns a bare name into \\.\pipe\<name>
	for(int i = 0; i < kPipeCount; i++)
		names.append(QStringLiteral("discord-ipc-%1").arg(i));
#else
	QStringList bases;
	for(const char *var : { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" })
	{
		QString value = qEnvironmentVariable(var);
		if(!value.isEmpty() && !bases.contains(value))
			bases.append(value);
	}
	if(!bases.contains("/tmp"))
		bases.append("/tmp");
	// plain, Flatpak and Snap installs of Discord put the socket in different places
	const QStringList subdirs = {
		QString(),
		QStringLiteral("app/com.discordapp.Discord/"),
		QStringLiteral(".flatpak/com.discordapp.Discord/xdg-run/"),
		QStringLiteral("snap.discord/"),
	};
	for(const QString &base : bases)
		for(const QString &subdir : subdirs)
			for(int i = 0; i < kPipeCount; i++)
			{
				QString path = QStringLiteral("%1/%2discord-ipc-%3").arg(base, subdir).arg(i);
				if(QFileInfo::exists(path))
					names.append(path);
			}
#endif
	return names;
}

}

DiscordIpc::DiscordIpc(const QString &client_id, QObject *parent)
	: QObject(parent),
	client_id(client_id),
	socket(new QLocalSocket(this)),
	retry_timer(new QTimer(this))
{
	retry_timer->setSingleShot(true);
	retry_timer->setInterval(kRetryMs);
	connect(retry_timer, &QTimer::timeout, this, &DiscordIpc::BeginConnect);

	connect(socket, &QLocalSocket::connected, this, &DiscordIpc::OnConnected);
	connect(socket, &QLocalSocket::readyRead, this, &DiscordIpc::OnReadyRead);
	connect(socket, &QLocalSocket::disconnected, this, &DiscordIpc::HandleLost);
	connect(socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
		if(state == State::Connecting)
		{
			// try the next pipe once this error has unwound
			QTimer::singleShot(0, this, &DiscordIpc::TryNextCandidate);
			return;
		}
		HandleLost();
	});
}

void DiscordIpc::Start()
{
	running = true;
	if(state == State::Idle && !retry_timer->isActive())
		BeginConnect();
}

void DiscordIpc::Stop()
{
	running = false;
	retry_timer->stop();
	if(state == State::Ready)
	{
		activity.reset();
		SendActivity();
	}
	activity.reset();
	state = State::Idle;
	// disconnectFromServer() still flushes the clear command written above
	socket->disconnectFromServer();
}

void DiscordIpc::SetActivity(const QJsonObject &activity)
{
	this->activity = activity;
	if(state == State::Ready)
		SendActivity();
}

void DiscordIpc::ClearActivity()
{
	if(!activity)
		return;
	activity.reset();
	if(state == State::Ready)
		SendActivity();
}

void DiscordIpc::BeginConnect()
{
	if(!running)
		return;
	candidates = CandidateNames();
	candidate_index = 0;
	TryNextCandidate();
}

void DiscordIpc::TryNextCandidate()
{
	if(!running)
		return;
	socket->abort();
	if(candidate_index >= candidates.size())
	{
		state = State::Idle;
		if(!reported_missing)
		{
			qCInfo(chiakiDiscord) << "Discord is not running, will keep retrying";
			reported_missing = true;
		}
		retry_timer->start();
		return;
	}
	state = State::Connecting;
	socket->connectToServer(candidates[candidate_index++]);
}

void DiscordIpc::HandleLost()
{
	if(state == State::Idle || state == State::Connecting)
		return;
	qCInfo(chiakiDiscord) << "Connection to Discord lost";
	state = State::Idle;
	socket->abort();
	if(running)
		retry_timer->start();
}

void DiscordIpc::OnConnected()
{
	state = State::Handshaking;
	buffer.clear();
	QJsonObject handshake {
		{ "v", 1 },
		{ "client_id", client_id },
	};
	SendFrame(OpHandshake, QJsonDocument(handshake).toJson(QJsonDocument::Compact));
}

void DiscordIpc::OnReadyRead()
{
	buffer.append(socket->readAll());
	while(buffer.size() >= kHeaderSize)
	{
		quint32 opcode = qFromLittleEndian<quint32>(buffer.constData());
		quint32 length = qFromLittleEndian<quint32>(buffer.constData() + 4);
		if(length > kMaxFrameSize)
		{
			qCWarning(chiakiDiscord) << "Oversized frame from Discord, reconnecting";
			HandleLost();
			return;
		}
		if(buffer.size() < kHeaderSize + static_cast<int>(length))
			return;
		QByteArray payload = buffer.mid(kHeaderSize, length);
		buffer.remove(0, kHeaderSize + length);
		HandleFrame(opcode, payload);
		if(state == State::Idle)
			return;
	}
}

void DiscordIpc::HandleFrame(quint32 opcode, const QByteArray &payload)
{
	switch(opcode)
	{
		case OpPing:
			SendFrame(OpPong, payload);
			break;
		case OpClose:
			qCWarning(chiakiDiscord) << "Discord closed the connection:" << payload;
			HandleLost();
			break;
		case OpFrame:
		{
			QJsonObject msg = QJsonDocument::fromJson(payload).object();
			QString evt = msg.value("evt").toString();
			if(evt == "READY")
			{
				state = State::Ready;
				reported_missing = false;
				qCInfo(chiakiDiscord) << "Connected to Discord";
				if(activity)
					SendActivity();
			}
			else if(evt == "ERROR")
			{
				qCWarning(chiakiDiscord) << "Discord error:" << msg.value("data").toObject().value("message").toString();
			}
			break;
		}
		default:
			break;
	}
}

void DiscordIpc::SendFrame(quint32 opcode, const QByteArray &payload)
{
	QByteArray frame(kHeaderSize, Qt::Uninitialized);
	qToLittleEndian<quint32>(opcode, frame.data());
	qToLittleEndian<quint32>(static_cast<quint32>(payload.size()), frame.data() + 4);
	frame.append(payload);
	socket->write(frame);
}

void DiscordIpc::SendActivity()
{
	QJsonObject args {
		{ "pid", static_cast<qint64>(QCoreApplication::applicationPid()) },
	};
	// leaving "activity" out clears the status
	if(activity)
		args.insert("activity", *activity);
	QJsonObject command {
		{ "cmd", "SET_ACTIVITY" },
		{ "args", args },
		{ "nonce", QString::number(++nonce) },
	};
	SendFrame(OpFrame, QJsonDocument(command).toJson(QJsonDocument::Compact));
}
