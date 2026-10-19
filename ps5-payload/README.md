# ps5-rpc payload

Tiny PS5 payload that reports the title ID of the currently running game. The
Discord Rich Presence module of this fork (`gui/discordpresence/`) reads it
while a stream session is active.

Taken unchanged from [smokeyxd/ps5-rust-rpc](https://github.com/smokeyxd/ps5-rust-rpc)
(`payload/`, commit `d784cf06`). Licensed under GPLv3, see [LICENSE](LICENSE).

## What it does

- Listens on TCP port **8000**. Every connected client immediately receives one
  line, then a new line whenever the running game changes (checked every 2 s)
  and a repeat of the current line every 10 s.
- Lines are either a title ID (`PPSA03977\n`, `CUSA00265\n`), `No game running.\n`
  or `Failed to get title ID.\n`. Same format as etaHEN's Discord RPC server, so
  either one can be used.
- Anything a client sends is ignored. It accepts no commands.
- Answers a discovery probe on UDP port 1010 (echoes the magic `0xFFFFAAAA`).
  chiaki-ng does not use it, the console address comes from the stream session.

The title ID is obtained with `sceSystemServiceGetAppIdOfRunningBigApp()` and
`sceSystemServiceGetAppTitleId()`.

## Build

Requires the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk):

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make
```

## Load

Send it to the ELF loader on the console (port 9021):

```sh
make test PS5_HOST=192.168.1.7
# or
nc -q0 192.168.1.7 9021 < ps5-rpc.elf
```

The console shows the notification `ps5-rpc: ready on port 8000`. The payload
has to be loaded again after every console reboot.

If etaHEN is running with `discord_rpc=1`, it already occupies port 8000 and the
payload steps aside; chiaki-ng then reads etaHEN's server instead.
