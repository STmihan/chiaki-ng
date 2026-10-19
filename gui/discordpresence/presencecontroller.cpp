// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "presencecontroller.h"
#include "discordpresence.h"
#include "discordipc.h"
#include "presenceactivity.h"
#include "presencelog.h"
#include "titlemetadata.h"
#include "titlesource.h"

#include "streamsession.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(chiakiDiscord, "chiaki.discord", QtInfoMsg)

namespace
{

const QString kDiscordClientId = QStringLiteral("1561688446366388225");
constexpr quint16 kDefaultPayloadPort = 8000;

QString ConfigPath()
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/discord_presence.ini";
}

}

void DiscordPresence::Attach(StreamSession *session)
{
	PresenceController::Instance()->Attach(session);
}

PresenceController *PresenceController::Instance()
{
	static PresenceController *instance = nullptr;
	if(!instance)
	{
		instance = new PresenceController(nullptr);
		// QtNetwork objects must be gone before QCoreApplication is torn down
		QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, QCoreApplication::instance(), []() {
			delete instance;
			instance = nullptr;
		});
	}
	return instance;
}

PresenceController::PresenceController(QObject *parent)
	: QObject(parent),
	discord(new DiscordIpc(kDiscordClientId, this)),
	metadata(new TitleMetadata(this))
{
	connect(metadata, &TitleMetadata::Ready, this, &PresenceController::OnMetaReady);
}

void PresenceController::Attach(StreamSession *session)
{
	// StreamSession emits from the chiaki session thread, hop over to ours
	QPointer<StreamSession> guard(session);
	connect(session, &StreamSession::ConnectedChanged, this, [this, guard]() {
		if(!guard)
			return;
		if(guard->IsConnected())
			Begin(guard.data());
		else if(active_session == guard.data())
			End();
	}, Qt::QueuedConnection);
	connect(session, &QObject::destroyed, this, [this, session]() {
		if(active_session == session)
			End();
	});
}

void PresenceController::Begin(StreamSession *session)
{
	if(active_session == session)
		return;
	End();

	// read on every session, so edits apply without restarting chiaki
	QSettings config(ConfigPath(), QSettings::IniFormat);
	if(!config.contains("enabled"))
		config.setValue("enabled", true);
	if(!config.contains("payload_port"))
		config.setValue("payload_port", kDefaultPayloadPort);
	if(!config.value("enabled").toBool())
	{
		qCInfo(chiakiDiscord) << "Disabled in" << ConfigPath();
		return;
	}
	bool ok = false;
	uint port = config.value("payload_port").toUInt(&ok);
	if(!ok || port == 0 || port > 65535)
		port = kDefaultPayloadPort;

	QString host = session->GetHost();
	if(host.isEmpty())
		return;

	active_session = session;
	qCInfo(chiakiDiscord) << "Stream connected, reading the running game from" << host << "port" << port;
	discord->Start();
	source = new TitleSource(host, static_cast<quint16>(port), this);
	connect(source, &TitleSource::TitleReceived, this, &PresenceController::OnTitleReceived);
	connect(source, &TitleSource::Unavailable, this, &PresenceController::OnUnavailable);
	source->Start();
}

void PresenceController::End()
{
	if(!active_session)
		return;
	active_session = nullptr;
	source->Stop();
	source->deleteLater();
	source = nullptr;
	has_state = false;
	current_title.clear();
	discord->Stop();
	qCInfo(chiakiDiscord) << "Stream ended, status removed";
}

void PresenceController::OnTitleReceived(const QString &title_id)
{
	if(has_state && title_id == current_title)
		return;
	has_state = true;
	current_title = title_id;

	if(title_id.isEmpty())
	{
		qCInfo(chiakiDiscord) << "No game running";
		discord->SetActivity(PresenceActivity::Idle());
		return;
	}

	start_ts = QDateTime::currentSecsSinceEpoch();
	metadata->Lookup(title_id);
}

void PresenceController::OnUnavailable()
{
	qCInfo(chiakiDiscord) << "Payload gone, status removed until it is back";
	has_state = false;
	current_title.clear();
	discord->ClearActivity();
}

void PresenceController::OnMetaReady(const QString &title_id, const GameMeta &meta)
{
	// the game may have changed while the lookup was running
	if(!active_session || title_id != current_title)
		return;
	qCInfo(chiakiDiscord) << "Playing" << meta.name << "(" << title_id << ")";
	discord->SetActivity(PresenceActivity::Game(title_id, meta, start_ts));
}
