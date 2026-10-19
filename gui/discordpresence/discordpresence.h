// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_DISCORDPRESENCE_H
#define CHIAKI_DISCORDPRESENCE_H

class StreamSession;

// Fork addition: Discord Rich Presence for the game running on the console.
// The only entry point the rest of chiaki-ng uses.
namespace DiscordPresence
{

/** Call once for every new StreamSession; the module follows it from there. */
void Attach(StreamSession *session);

}

#endif // CHIAKI_DISCORDPRESENCE_H
