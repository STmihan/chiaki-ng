
![chiaki-ng Logo](gui/res/chiaking-logo.svg)

# [chiaki-ng](https://streetpea.github.io/chiaki-ng/)

An open source PlayStation remote play project serving as the next-generation of Chiaki with improvements and ongoing support now that the original Chiaki project is in maintenance mode only. [Click here to see the accompanying site for documentation, updates and more](https://streetpea.github.io/chiaki-ng/).

## Discord
[chiaki-ng community Discord](https://discord.gg/tAMbRuwXDH)

## Disclaimer
This project is not endorsed or certified by Sony Interactive Entertainment LLC.

Chiaki is a Free and Open Source Software Client for PlayStation 4 and PlayStation 5 Remote Play
for Linux, FreeBSD, OpenBSD, Android, macOS, Windows, Nintendo Switch and potentially even more platforms.

## About this fork: Discord Rich Presence

This fork ([STmihan/chiaki-ng](https://github.com/STmihan/chiaki-ng)) adds one feature on top of
upstream chiaki-ng: while a stream to a **jailbroken PS5** is connected, the game running on the
console is shown in Discord, with its cover art and play time. When the stream ends the status is
removed.

### How it works

1. A tiny payload on the console ([`ps5-payload/`](ps5-payload/), taken from
   [smokeyxd/ps5-rust-rpc](https://github.com/smokeyxd/ps5-rust-rpc)) reports the title ID of the
   running game on TCP port 8000. etaHEN's built-in server (`discord_rpc=1`) speaks the same
   format and works too.
2. Once a stream session is connected, chiaki-ng connects to that port on the same console.
3. The title ID is turned into a name and cover art via
   [prosperopatches.com](https://prosperopatches.com) (PS5) or
   [orbispatches.com](https://orbispatches.com) (PS4). Results are cached on disk: a title is
   looked up at most once per 30 days, failed lookups are retried after an hour.
4. The status goes to the Discord desktop app over its local RPC socket.

| Situation | Discord status |
|---|---|
| Streaming, game running | Game name, "on PS5" (or "PS4 game on PS5"), cover, elapsed time, "View game" button |
| Streaming, no game running | "Idle — No game running" |
| Not streaming, or payload not reachable | No status |

### Setup

1. Build and load the payload, see [`ps5-payload/README.md`](ps5-payload/README.md). It has to be
   loaded again after every console reboot.
2. Keep the Discord desktop app running on the same PC as chiaki-ng.
3. Start a stream as usual.

### Configuration

There is no UI for it. On the first stream `discord_presence.ini` is created in the chiaki-ng
data directory (`%APPDATA%\Chiaki\Chiaki\` on Windows, `~/.local/share/Chiaki/Chiaki/` on Linux):

```ini
enabled=true
payload_port=8000
```

The metadata cache lives next to it in `discord_presence_cache.json`. Log lines of the module use
the `chiaki.discord` category.

### Differences from upstream

All code of the feature is new and kept separate, so merging upstream stays painless:

- `gui/discordpresence/` — the module (Discord IPC client, payload reader, metadata cache);
- `ps5-payload/` — payload sources (GPLv3, original author's license kept);
- `gui/CMakeLists.txt` — one `include()` line at the end;
- `gui/src/qmlbackend.cpp` — one `#include` and one `DiscordPresence::Attach(session)` call;
- `README.md` — this section.

CI fix, not related to the feature: MSYS2 dropped `mingw-w64-x86_64-hidapi`, so the manual
Windows build (`.github/workflows/build-msys2.yml`) uses the UCRT64 environment instead of
MINGW64, and `scripts/build-libplacebo-windows.sh` installs into `$MINGW_PREFIX` instead of a
hardcoded `/mingw64` (same result under MINGW64).
