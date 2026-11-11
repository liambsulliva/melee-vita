# Slippi Direct play research (Vita)

Research into how melee-3ds implements Slippi Direct play, and what a
functionally identical Vita port needs. No code was changed.

## Sources and citation format

| Short name | Source | Revision read |
| --- | --- | --- |
| (none) | this repo, branch `origin/vita-port` | `68f828b` |
| `3ds:` | https://github.com/2gifts/melee-3ds, branch `slippi-beta`, tag `slippi-beta-v0.1.0`. Paths are relative to `native-3ds/` unless they start with `README.md` | `3800a44deeff56402a9ca9ecfb26c6570c2bb78b` (2026-10-05) |
| `3ds-head:` | https://github.com/2gifts/melee-3ds, default branch `3ds` | `1efbf37caeed0f4bc1bc7828f9e61a12a1376004` |
| `ish:` | https://github.com/project-slippi/Ishiiruka, branch `slippi`. Paths are relative to `Source/Core/Core/` | `cacbc15` (2026-10-23, HEAD when read) |
| `ish@60f7b63:` | the same repository at the revision the 3DS research used | `60f7b63` (2026-09-22) |
| `asm:` | https://github.com/project-slippi/slippi-ssbm-asm | `abbe0ce` (2026-10-23) |
| `rx:` | https://github.com/project-slippi/slippi-rust-extensions, the submodule revision that `ish:` HEAD pins | `ddcee36` (2026-10-20) |
| `newlib:` | https://github.com/vitasdk/newlib, `newlib/libc/sys/vita/` | `2e42829` |
| `headers:` | https://github.com/vitasdk/vita-headers | `e66ebe9` |

Citations are `path:line`. Statements marked (inferred) are my reading of the
code and were not tested. I did not build or run any of this code, and I did
not talk to any Slippi server.

## Summary

- The requested HEAD `1efbf37` contains no Slippi code. It only has the README
  section and a guide that point to a separate pre-release
  (`3ds-head:README.md`, "Slippi Direct beta"). The implementation lives on
  branch `slippi-beta`, tag `slippi-beta-v0.1.0` = `3800a44`, 44 commits ahead
  of `1efbf37`. Everything below describes `3800a44`.
- melee-3ds implements Slippi Direct 1v1 only. It has no Ranked, Unranked,
  Teams, spectating, replay recording on the console, or custom chat editing
  (`3ds:docs/slippi/SLIPPI_BETA.md:48-55`, `3ds:docs/slippi/BETA_RELEASE.md:30`).
- The 3DS has no rollback. It is a lockstep peer with input delay. Each engine
  frame waits until the opponent's pad for that frame has arrived
  (`3ds:port/engine/slippi/online.c:1-8`, `408-422`). The guide says so directly
  (`3ds:docs/slippi/SLIPPI_BETA.md:165`). There is no savestate code and no
  prediction.
- Interoperability with Dolphin Slippi works because the PC peer predicts and
  rolls back on its own. The Slippi wire protocol never asks the remote for
  anything beyond pads, acks, selections and checksums
  (`3ds:docs/slippi/protocol_notes.md:236-240`). The 3DS reimplements Slippi's
  ENet/JSON matchmaking client and P2P packet formats. I compared the packet
  layouts and the 0x138-byte game info block against Ishiiruka and they match
  byte for byte (see the protocol section). The project reports hardware tests
  against a PC on LAN and on a phone hotspot with no desyncs
  (`3ds:docs/slippi/BETA_RELEASE.md:53-57`). I could not verify those runs.
- The hard part of the 3DS work is bit-exact simulation, not networking. The
  3DS port rebuilt the engine's floating point to round like the GameCube,
  compiled Melee's own MSL maths library, emulated `frsqrte`/`fres` and the
  paired-single matrix routines, and ported every gameplay-affecting Slippi
  Gecko code to C (`3ds:docs/slippi/determinism.md`, `3ds:docs/slippi/rules.md`).
- Interoperability is at risk from Slippi itself. Ishiiruka replaced the
  ENet/JSON matchmaking client with a WebSocket and STUN client on 2026-10-22
  (`ish` commit `dbfc621`, "new mm and stun implementation"). The new client
  talks to `wss://matchmaking.slippi.gg/v1/queue` (`rx:matchmaking/src/client.rs:19`).
  The 3DS speaks only the old protocol to `mm.slippi.gg:43113`. At the
  revision read, the new client is unreleased (`ish:../Common/Version.cpp:12`
  says `3.6.5-dev.1`, and the newest release tag is `v3.6.4`). The P2P
  protocol did not change (`SlippiNetplay.cpp` has no diff between `60f7b63`
  and `cacbc15`).
- For Vita, about half of the 3DS Slippi code is platform-neutral C and can be
  copied. The network client, ENet, JSON, matchmaking, P2P, the lockstep
  driver, the match block builder, the C ports of the Gecko codes and the
  online CSS session logic need only a new platform layer. The determinism work
  cannot be copied. The 3DS does it with a clang IR rewrite step, while Vita
  builds with GCC at `-O3 -funsafe-math-optimizations -ffp-contract=fast`
  (`platforms/vita/build-game.ps1:78-83`) and maps `__frsqrte` to `1.0 / sqrt(x)`
  (`src/placeholder.h:13`). The Vita decomp is also a different upstream
  commit (`src/UPSTREAM_COMMIT` = `430c286`, the 3DS uses `039c4bf`,
  `3ds:upstream.lock.json:3`).

## What melee-3ds implements

| Area | State at `3800a44` | Evidence |
| --- | --- | --- |
| Matchmaking, Direct mode by connect code | Implemented, old ENet/JSON protocol | `3ds:port/3ds/slippi/slippi_mm.c` |
| Ranked, Unranked, Teams, Party | Not implemented. Menu entries are shown locked | `3ds:docs/slippi/UI_PLAN.md:18` |
| P2P pads, acks, selections, chat | Implemented | `3ds:port/3ds/slippi/slippi_p2p.c` |
| Rollback, prediction, savestates | Not implemented. Lockstep with input delay | `3ds:port/engine/slippi/online.c:1-8` |
| Time sync | Partial. Drops pad samples when ahead, adds extra engine bodies when behind | `3ds:port/engine/slippi/online.c:551-636` |
| Desync detection | Implemented, stricter than Slippi | `3ds:port/engine/slippi/online.c:498-526` |
| Slippi gameplay codes | Ported to C, gated by a mask | `3ds:port/engine/slippi/rules.c`, `3ds:tools/slippi_edits/online_rules.py` |
| Online CSS, SSS, chat, HUD texts | Implemented, with bottom-screen companion pages | `3ds:port/3ds/slippi/slippi_ui.c`, `3ds:port/engine/slippi/online_mode.c` |
| Replay recording on the console | Not implemented. The PC records the `.slp` | `3ds:docs/slippi/SLIPPI_BETA.md:48-53` |
| 3DS vs 3DS | Tested only in two emulators | `3ds:docs/slippi/BETA_RELEASE.md:57`, `3ds:docs/slippi/SLIPPI_BETA.md:55` |
| Login | Copies Slippi Launcher's `user.json` to the SD card | `3ds:docs/slippi/SLIPPI_BETA.md:115-121` |

It is not LAN-only. It uses the production matchmaking server by default
(`3ds:port/3ds/slippi/slippi_internal.h:24-26`) and connects to the opponent's
public or LAN address from the server's reply (`3ds:port/3ds/slippi/slippi_mm.c:132-145`).

## Architecture

### Code layout

The 3DS port compiles the decomp as a big-endian ARM (BE8) "engine" and
links it with native little-endian libctru code. Assembly stubs switch
endianness on every call between the two (`3ds:port/3ds/game_bridge.S:5-35`).
Only scalars and byte buffers cross that boundary
(`3ds:port/include/slippi_net_bridge.h:1-8`).

| Layer | Files | Platform-neutral |
| --- | --- | --- |
| ENet 1.3.13 | `3ds:port/3ds/slippi/enet/*.c` | Yes, except `ctru.c` |
| ENet 3DS socket layer | `3ds:port/3ds/slippi/enet/ctru.c` | No (libctru soc:U) |
| Platform layer | `3ds:port/3ds/slippi/slippi_plat.[ch]` | No. Has 3DS and Win32 branches |
| JSON | `3ds:port/3ds/slippi/slippi_json.[ch]` | Yes |
| Config, `user.json`, connect-code encoding | `3ds:port/3ds/slippi/slippi_config.c` | Yes |
| Matchmaking client | `3ds:port/3ds/slippi/slippi_mm.c` | Yes |
| P2P protocol | `3ds:port/3ds/slippi/slippi_p2p.c` | Yes |
| Lifecycle, service loop, thread | `3ds:port/3ds/slippi/slippi_net.[ch]` | Yes |
| Engine API, lockstep rule, match block | `3ds:port/3ds/slippi/slippi_bridge.c` | Yes, except a 3DS prefetch call at `188-194` |
| Online CSS session, code entry, chat | `3ds:port/3ds/slippi/slippi_ui.[ch]` | Mostly. Includes `<3ds.h>` and reads 3DS keys in `slippi_ui_pad` (`828-869`) |
| Boot menu, self-test, prefetch, file helpers | `3ds:port/3ds/slippi/slippi_boot_ui.c`, `slippi_selftest.c`, `slippi_prefetch.c`, `slippi_files.c` | Self-test is neutral. The rest is 3DS-specific |
| Engine-side online driver | `3ds:port/engine/slippi/online.c` | Yes, decomp C |
| Online mode, CSS text, splash | `3ds:port/engine/slippi/online_mode.c` | Yes, decomp C |
| In-game HUD texts | `3ds:port/engine/slippi/online_hud.c` | Yes, decomp C |
| Gecko codes as C | `3ds:port/engine/slippi/rules.c` | Yes, decomp C |
| Replay playback test tool | `3ds:port/engine/slippi/replay.c` | Yes, decomp C |
| Hooks into decomp sources | `3ds:tools/slippi_edits/*.py` | Python text edits anchored on decomp `039c4bf` source text |
| PC test tools | `3ds:tools/slippi/src/fake_mm.c`, `fake_peer.c`, `3ds:tools/slippi/*.py` | Yes, Win32 |

The decomp hooks are string replacements applied at build time
(`3ds:tools/slippi_mods.py:1-27`). Each edit names exact decomp source text, so
they will not apply to another decomp revision without rework (inferred).

### Threads

- The engine thread runs the game, the online driver and the CSS session.
- One network thread runs `enet_host_service(host, 0)` every 2000 us under a
  global lock (`3ds:port/3ds/slippi/slippi_net.c:146-156`,
  `3ds:port/3ds/slippi/slippi_config.c:19`). It is created one priority step
  above the caller with a 32 KiB stack (`3ds:port/3ds/slippi/slippi_plat.c:197-207`).
- Every API call takes the same lock, so the engine thread can call anything
  (`3ds:docs/slippi/network.md:31-43`). DNS resolution drops the lock while it
  blocks (`3ds:port/3ds/slippi/slippi_mm.c:254-258`).
- Log lines from the network thread go into a queue. The engine thread writes
  them out on its next API call (`3ds:port/3ds/slippi/slippi_plat.c:102-123`, `170-188`).

### Data flow per frame

1. The GameCube pad alarm samples pads into the pad queue. The 3DS hook right
   after `PADRead` can drop a sample to slow the game when it runs ahead
   (`3ds:tools/slippi_edits/online.py:24-27`, `3ds:port/engine/slippi/online.c:324-333`).
2. At the top of each engine body, `mp_slippi_online_frame_begin` runs
   (`3ds:tools/slippi_edits/online.py:37`). It calls `exchange_pads` with the
   global frame counter (`3ds:port/engine/slippi/online.c:456-473`).
3. `exchange_pads` zeroes all pads before frame `78 - delay`, clamps a stick at
   rest, replaces a stale pad, and converts port 1's pad to the 12-byte wire
   form (`3ds:port/engine/slippi/online.c:383-400`).
4. It then calls `send_inputs` in a loop. Each call queues and sends the local
   pad tagged `frame + delay` once, and returns SKIP until the remote pad for
   `frame` is present. Between retries the engine polls the network and
   sleeps about 1 ms (`3ds:port/engine/slippi/online.c:408-422`,
   `3ds:port/3ds/slippi/slippi_bridge.c:244-282`).
5. The local player's pad is taken from a delay ring, `delay` frames old. The
   remote pad goes into the remote port (`3ds:port/engine/slippi/online.c:434-438`).
6. The engine body simulates. The driver computes the desync checksum for this
   frame and compares the opponent's latest finalized checksum
   (`3ds:port/engine/slippi/online.c:485-526`).
7. At the end of the body the time-sync logic may queue extra pad samples or
   drop future ones (`3ds:port/engine/slippi/online.c:551-636`). A body that
   no draw will follow still sets up camera and bone matrices, because
   gameplay reads them (`3ds:port/engine/slippi/online.c:638-666`).
8. Rendering runs on its own schedule. The online build forces 60 FPS mode
   (`3ds:port/3ds/frame_rate.h:15-18`).

## Rollback and frame control

### What the 3DS does

- No game state is saved or restored. A frame runs only after the remote pad
  for it has arrived (`3ds:port/engine/slippi/online.c:1-8`). Audio, effects
  and RNG never need resimulation handling because no frame runs twice.
- Input delay comes from `config.ini` `delay=`, default 2, range 1 to 15
  (`3ds:port/3ds/slippi/slippi_config.c:12`, `58`). The settings page offers 1 to 9
  (`3ds:port/3ds/slippi/slippi_ui.c:472-486`). The delay goes to the engine in
  the match block trailer (`3ds:port/3ds/slippi/slippi_bridge.c:181`).
- At frame 1 the sender queues zero pads for frames 1 to `delay`, one packet
  each, then the real pad (`3ds:port/3ds/slippi/slippi_net.c:431-443`). This
  matches Dolphin's `handleSendInputs` (`ish:HW/EXI_DeviceSlippi.cpp:1646`).
- A stall longer than 7 s force-disconnects the peer. The 3DS measures the 7 s
  in time, because the engine retries a frame about once per millisecond
  (`3ds:port/3ds/slippi/slippi_bridge.c:212-225`). Dolphin counts 420 skipped
  frames (`ish:HW/EXI_DeviceSlippi.cpp:1465`).
- Early time sync copies Dolphin. On frames that are multiples of 30 and at
  most 120, if the offset exceeds 10 ms, the 3DS skips up to 5 frames
  (`3ds:port/3ds/slippi/slippi_bridge.c:228-241`, `ish:HW/EXI_DeviceSlippi.cpp:1493-1517`).
- Late time sync is new code. After frame 120 the 3DS drops at most one pad
  sample per 180 frames when more than 8 ms ahead
  (`3ds:port/engine/slippi/online.c:564-580`). When behind by more than an
  adaptive allowance, it runs up to 3 extra engine bodies, one every 5 frames
  (`3ds:port/engine/slippi/online.c:613-635`). The allowance starts at 1.6
  frames and grows by half a frame per 30-frame window with 4 or more waits,
  up to 5 frames (`3ds:port/engine/slippi/online.c:56-59`, `588-610`).
  Running behind the PC costs the PC rollbacks inside its 7-frame window and
  costs the 3DS nothing (`3ds:docs/slippi/network.md:258-264`).
- The 3DS pad timer is exactly 60 Hz and Dolphin runs at 59.94 Hz, so on LAN
  the 3DS drops one frame about every 12 s (`3ds:docs/slippi/network.md:278`).

### Desync detection

- `compute_checksum` reproduces `FN_COMPUTE_CHECKSUM` from
  `asm:Online/Core/StartEngineLoop.asm:75-118`. It XORs action state, X, Y,
  percent and spawn number of each fighter and Nana or the secondary, XORs
  each player's stocks, and sums X, Y and percent as floats
  (`3ds:port/engine/slippi/online.c:256-281`).
- It keeps a 21-entry ring of local checksums and sends the checksum of its
  stable finalized frame in every pad packet
  (`3ds:port/engine/slippi/online.c:402-407`, `479-496`).
- Difference from Slippi. Slippi ends the game only when the low 16 bits differ
  by more than 1. A mismatch in the high 16 bits shows "Desync Risk" and play
  continues (`asm:Online/Core/StartEngineLoop.asm:540-595`). The 3DS treats a
  high-16 mismatch as a hard desync and ends the match
  (`3ds:port/engine/slippi/online.c:510-521`). A Vita port that wants identical
  behaviour to Slippi should decide which of the two to copy.
- `config.ini` `send_checksum=0` sends checksum frame 0, which turns off the
  PC's desync check (`3ds:docs/slippi/network.md:113`).

### How Slippi Dolphin does rollback, for reference

The 3DS does not use this, but a Vita port with real rollback would.

- Game-side ASM predicts remote pads from the last received pad, compares
  predictions with arrived inputs, and sets a rollback target
  (`3ds:docs/slippi/protocol_notes.md:216-226`, `asm:Online/Core/TriggerSendInput.asm`).
- `LoopEngineForRollback.asm` reruns the engine without rendering
  (`asm:Online/Core/LoopEngineForRollback.asm`).
- Dolphin restores whole RAM regions. `SlippiSavestate` lists fixed
  GameCube address ranges and excludes audio areas
  (`ish:Slippi/SlippiSavestate.cpp:53-91`). The rollback limit is 7 frames
  (`ish:Slippi/SlippiNetplay.h:31`, `asm:Online/Online.s:43`).

## Protocol

All P2P payloads use SFML packet encoding with big-endian integers. The 3DS
P2P code follows `SlippiNetplay.cpp` and I found no difference in layout.

| ID | Layout | ENet | 3DS | Ishiiruka |
| --- | --- | --- | --- | --- |
| 0x80 pad | u8 id, s32 frame, u8 port, s32 checksum frame, u32 checksum, N x 8-byte pads newest first | unsequenced, channel 1 | `3ds:port/3ds/slippi/slippi_p2p.c:163-209` | `ish:Slippi/SlippiNetplay.cpp:1278-1297` |
| 0x81 ack | u8 id, s32 frame, u8 port. Sent only when the pad packet added frames | unsequenced, channel 2 | `3ds:port/3ds/slippi/slippi_p2p.c:300-308` | `ish:Slippi/SlippiNetplay.cpp:371-383` |
| 0x82 selections | u8 id, char, colour, char selected, player idx, u16 stage, stage selected, u32 rng offset, team, alt stage mode | reliable, channel 0 | `3ds:port/3ds/slippi/slippi_p2p.c:127-140` | `ish:Slippi/SlippiNetplay.cpp:550-559` |
| 0x84 chat | u8 id, s32 message id, u8 player idx | reliable, channel 0 | `3ds:port/3ds/slippi/slippi_p2p.c:152-160` | `ish:Slippi/SlippiNetplay.cpp:561-566` |
| 0x83, 0x85, 0x86 | Ignored on receive. Never sent | | `3ds:port/3ds/slippi/slippi_p2p.c:383-384` | `ish:NetPlayProto.h:60-66` |

Other wire behaviour:

- A pad packet carries every pad not yet acked, capped at the newest frame
  minus 128 (`3ds:port/3ds/slippi/slippi_p2p.c:163-176`,
  `ish:Slippi/SlippiNetplay.cpp:1254-1270`). The receiver copies
  `frame - head_frame` pads and rejects more than 128
  (`3ds:port/3ds/slippi/slippi_p2p.c:280-296`, `ish:Slippi/SlippiNetplay.cpp:331-349`).
- The wire pad holds the first 8 bytes of the 12-byte PAD report. The 3DS
  zeroes bytes 8 to 11 of its own pad before it simulates, so both sides see
  the same bytes (`3ds:port/engine/slippi/online.c:398-400`).
- Ping comes from the ack of the pad packet with the same frame. The time
  offset uses a 30-sample ring and averages the middle third
  (`3ds:port/3ds/slippi/slippi_p2p.c:222-241`, `270-278`, `311-328`,
  `ish:Slippi/SlippiNetplay.cpp:304-316`, `1654-1700`).
- Receiving selections clears the remote pad queue. This is how a new game
  starts (`3ds:port/3ds/slippi/slippi_p2p.c:350-354`).
- There is no version field or hello packet between peers. The ENet connect
  event is the handshake (`3ds:docs/slippi/protocol_notes.md:93`).
- ENet is vendored from Ishiiruka. I diffed it against
  `ish:../../../Externals/enet`. All files are identical except two 3DS guards:
  `unix.c` line 5 excludes the file on the 3DS, and `include/enet/unix.h`
  adds `arpa/inet.h` for libctru.
- The game info block template is byte-identical to Dolphin's
  `onlineMatchBlock` (all 312 bytes compared, `3ds:port/3ds/slippi/slippi_bridge.c:61-83`,
  `ish:HW/EXI_DeviceSlippi.cpp:2122`). The builder sets stock mode, 4 stocks,
  480 s, items off unless the server's `items` bits say otherwise, p3 and p4
  empty, and pause allowed in Direct (`3ds:port/3ds/slippi/slippi_bridge.c:90-156`,
  `ish:HW/EXI_DeviceSlippi.cpp:2549-2623`).
- The decider (`isHost` from matchmaking) is player index 0 and supplies the
  RNG offset (`3ds:port/3ds/slippi/slippi_bridge.c:107`, `180`).

## Matchmaking

### What the 3DS sends

- Server `mm.slippi.gg:43113` over ENet, 3 channels, one JSON text per reliable
  packet on channel 0 (`3ds:port/3ds/slippi/slippi_internal.h:24-26`,
  `3ds:port/3ds/slippi/slippi_mm.c:86-90`, `266`).
- One ENet host serves matchmaking and then P2P on the same random local port
  41000 to 50999. Dolphin uses two hosts on the same port. The packets are the
  same (`3ds:port/3ds/slippi/slippi_net.c:33-49`, `3ds:docs/slippi/network.md:29`).
  Because the host accepts the opponent's connection before the match reply,
  early P2P events are queued and replayed (`3ds:port/3ds/slippi/slippi_net.c:51-100`).
- `create-ticket` holds `appVersion`, `ipAddressLan`,
  `search {connectCode: [Shift-JIS bytes], mode: 2}`, `type` and
  `user {connectCode, displayName, playKey, uid}`, with keys sorted like
  nlohmann `dump()` (`3ds:port/3ds/slippi/slippi_mm.c:44-94`).
- `appVersion` defaults to `3.6.4` (`3ds:port/3ds/slippi/slippi_internal.h:26`).
  That is the netplay version string at `ish@60f7b63` (`Common/Version.cpp:12`).
- The connect code goes out as full-width Shift-JIS, the bytes Slippi's
  name-entry screen produces. For example `A` becomes 0x8260 and `#` becomes
  0x8194 (`3ds:port/3ds/slippi/slippi_config.c:116-150`). Dolphin forwards the
  raw bytes the game hands it (`ish:HW/EXI_DeviceSlippi.cpp:1929-1949`).
- The LAN address comes from a UDP `connect` toward the server, else
  `gethostid()` (`3ds:port/3ds/slippi/slippi_mm.c:30-42`).
- The reply handling follows `handleMatchmaking`. It reads `players[]`,
  `isHost`, `stages`, `items` and `matchId`, and picks the opponent's LAN
  address only when both external IPs match (`3ds:port/3ds/slippi/slippi_mm.c:106-165`).

### Login

The 3DS has no login flow. The player copies Slippi Launcher's `user.json` to
`sdmc:/3ds/melee/slippi/user.json` or lets the builder copy it
(`3ds:docs/slippi/SLIPPI_BETA.md:80`, `115`). The client reads `uid`,
`playKey`, `connectCode`, `displayName` and `latestVersion`
(`3ds:port/3ds/slippi/slippi_config.c:89-107`). It logs only the play key's
length (`3ds:port/3ds/slippi/slippi_net.c:179`) and masks public IP
addresses in `game.log` (`3ds:port/3ds/slippi/slippi_plat.c:135-168`).

### NAT traversal and direct connect

- The 3DS relies on plain UDP hole punching through the address the server
  observed. Both peers `enet_host_connect` from the same port they used for
  matchmaking. There is no STUN, relay or UPnP (`3ds:docs/slippi/network.md:287-291`).
- The P2P connect times out after 8 s and then re-tickets with a new port,
  forever by default (`3ds:port/3ds/slippi/slippi_p2p.c:451-467`).
- During connect, the 3DS matches the remote by IP only, which tolerates NATs
  that change the port. Duplicate connections are resolved with the lower
  player index keeping one (`3ds:port/3ds/slippi/slippi_p2p.c:254-268`, `412-423`).
- There is no IP-based direct connect. Stock Slippi Dolphin can only reach a
  peer through the matchmaking server (`3ds:docs/slippi/protocol_notes.md:68-72`).
  For tests the 3DS points `mm_host` at a local fake server
  (`3ds:tools/slippi/src/fake_mm.c:1-8`).

### The protocol change in Slippi

- At `ish:` HEAD, `startMatchmaking` sends STUN binding requests to Google,
  Cloudflare and Twilio servers from the netplay socket, then hands the search
  to Rust (`ish:Slippi/SlippiMatchmaking.cpp:272-424`,
  `ish:Slippi/SlippiMatchmaking.h:112-116`).
- The Rust client opens `wss://matchmaking.slippi.gg/v1/queue` and sends a
  `join` message with the same `user` and `search` fields plus `port`,
  `candidates` and `natType` (`rx:matchmaking/src/client.rs:19`, `282`,
  `rx:matchmaking/src/protocol.rs:55-75`).
- The match result is still the same JSON shape, and C++ parses it as before
  (`ish:Slippi/SlippiMatchmaking.cpp:466-612`).
- The 3DS docs knew this design only as a reverted commit
  (`3ds:docs/slippi/protocol_notes.md:66`). It landed in `ish` `dbfc621` on
  2026-10-22, after the beta shipped.
- I cannot tell whether `mm.slippi.gg:43113` will keep serving the old
  protocol, or whether old-protocol tickets and new-protocol tickets can be
  paired. The server source is private (`3ds:docs/slippi/protocol_notes.md:335`).

## Competitive format

### Ruleset

The match rules come from the game info block, not from menu settings. Direct
uses stock mode, 4 stocks, 8 minutes, items off and pause allowed
(`3ds:port/3ds/slippi/slippi_bridge.c:133-153`). Stages are whatever the
players pick. The first stage is a random pick from the server's `stages`
list, without repeats until all were used
(`3ds:port/3ds/slippi/slippi_ui.c:160-205`). The fallback list is FoD, Pokemon
Stadium, Yoshi's Story, Dream Land, Battlefield and Final Destination
(`3ds:port/3ds/slippi/slippi_mm.c:152-156`). After game 1 the loser picks
(`3ds:port/3ds/slippi/slippi_ui.c:628-637`, `675-683`), and the first player
by index with a stage selected wins (`3ds:port/3ds/slippi/slippi_bridge.c:99-106`).
Frozen Pokemon Stadium travels as `alt_stage_mode`.

### Gecko codes as C

The 3DS reproduces each gameplay-affecting code from Slippi's "Required"
sections of `asm:netplay.json` (sections at `51`, `234`, `279`). Each is a C
function in `3ds:port/engine/slippi/rules.c` plus a decomp edit in
`3ds:tools/slippi_edits/online_rules.py`, gated by one bit of
`mp_slippi_rules_mask` (`3ds:port/include/slippi_rules.h:17-34`).

| Code | 3DS location |
| --- | --- |
| Neutral spawns | `rules.c:94-203`, hook in `gmvs.c` (`online_rules.py:159-166`) |
| UCF 0.84 | the port's `port/engine/ucf.c`, now reading the raw pads the body consumed (`3ds:port/engine/ucf.c:23-28`) |
| Prevent freeze glitch | `online_rules.py:150-157` |
| Initialize Player Data, Initialize Stage Data | `online_rules.py:20-27`, `198-211` |
| BrawlOffscreenDamage | `rules.c:205-235` |
| PreventWobbling | `rules.c:237-318` |
| FreezeDeadUpFallPhysics | `rules.c:320-355`, `online_rules.py:37-72` |
| PSCameraIndependentMonitor, frozen Stadium, Stadium file load | `rules.c:357-406`, `online_rules.py:213-262` |
| DynamicsFix | `rules.c:408-430` |
| LGLExceededGameEnd | `rules.c:432-462` |
| DesyncProofBGTransformations (FD) | `online_rules.py:183-197` |
| NanaDeterminism, WhispyBlowDirFix | `online_rules.py:73-84`, `143-149` |
| InitOnlinePlay held-A clear and per-frame RNG reseed | `rules.c:80-92`, `3ds:port/engine/slippi/online.c:790-847` |

Codes judged visual, audio or menu-only were not ported, with reasons in
`3ds:docs/slippi/rules.md` (for example Lagless FoD). The per-frame RNG reseed
writes `rotl32(frame, 16) + rng_offset` from a GObj proc of type 4
(`3ds:port/engine/slippi/online.c:790-795`, `845-846`), as `FN_SyncRNG` does
(`asm:Online/Core/InitOnlinePlay.asm:266-299`). Tap jump is forced to vanilla
online (`3ds:port/engine/tap_jump.c:18`).

### Verification

A replay test tool plays Slippi `.slp` files through the port and compares
every fighter's pre-frame and post-frame state (`3ds:port/engine/slippi/replay.c:1-12`).
The rules doc reports no structural divergence on 15 human-vs-human replays
after the codes landed, and lists cases no replay covered
(`3ds:docs/slippi/rules.md:102-134`). The determinism doc lists remaining
fused-op count mismatches, none on a legal stage
(`3ds:docs/slippi/determinism.md:217-233`). Subnormals flush to zero on the
3DS, which can differ from the console in rare cases
(`3ds:docs/slippi/determinism.md:174-178`, `301`).

## UI

- Entry is 1P mode, then the hidden "Online Play" entry, then Slippi's Online
  submenu with only Direct enabled (`3ds:tools/slippi_edits/online_ui.py:123-152`).
- Direct replaces the unused major scene `GM_HANYU_CSS` with a CSS, SSS and VS
  state table (`3ds:port/engine/slippi/online_mode.c:62-68`,
  `3ds:tools/slippi_edits/online_ui.py:116-121`).
- On the CSS, START with a fighter chosen opens connect-code entry. Confirming
  locks in and starts the search. Z cancels a search or clears an error.
  Holding Z for 0x30 frames disconnects (`3ds:port/3ds/slippi/slippi_ui.c:559-648`).
- Code entry is a bottom-screen keyboard with 8 characters, recent codes on L
  and R, and a suggestion from history (`3ds:port/3ds/slippi/slippi_ui.c:93-156`,
  `718-869`). Slippi Dolphin uses the game's patched name-entry screen
  instead (`3ds:docs/slippi/UI_PLAN.md:46-54`).
- The top screen shows Slippi's CSS text lines at Slippi's positions
  (`3ds:port/engine/slippi/online_mode.c:134-215`,
  `3ds:port/3ds/slippi/slippi_ui.c:952-1035`).
- Quick chat uses D-pad page then direction, 16 messages, ids per Slippi, a
  60-frame cooldown, and a "has chat disabled" reply when chat is off
  (`3ds:port/3ds/slippi/slippi_ui.c:253-403`, `3ds:port/3ds/slippi/slippi_p2p.c:360-373`).
- In game, the HUD shows names, "Delay: Nf", and DISCONNECTED or DESYNC
  DETECTED (`3ds:port/engine/slippi/online_hud.c:47-122`). A disconnect or
  desync ends the match as a no contest after 90 frames
  (`3ds:tools/slippi_edits/online.py:65-75`).
- Slippi's menu art comes from Slippi Dolphin's VCDIFF patches applied to the
  player's own disc files on a PC (`3ds:tools/slippi/slippi_files.py:1-20`).
  The 3DS loads them from an override folder (`3ds:port/3ds/file_io.c`, Slippi
  index).
- The VS splash exists but is off to shorten loads (`3ds:docs/slippi/UI_PLAN.md:129`).
- The 3DS caches match files in RAM because SD opens cost about 220 ms each
  and the PC waits at frame 1 (`3ds:docs/slippi/network.md:266-277`).

Doc and code disagree on one default. `network.md` says `chat` defaults to 0
(`3ds:docs/slippi/network.md:176`). The code sets it to 1
(`3ds:port/3ds/slippi/slippi_config.c:20`).

## Platform layer

### Functions the 3DS supplies, and Vita equivalents

| 3DS function | 3DS implementation | Vita equivalent |
| --- | --- | --- |
| `sp_time_us` | `svcGetSystemTick` (`3ds:port/3ds/slippi/slippi_plat.c:70-74`) | `sceKernelGetProcessTimeWide`, already used by the frame loop (`src/melee/gm/gmscene.c:342-343`) |
| `sp_sleep_us` | `svcSleepThread` | `sceKernelDelayThread` (`platforms/vita/game/profiler_live.c:123`) |
| `sp_lock_init`, `sp_lock`, `sp_unlock` | `LightLock` (`3ds:port/3ds/slippi/slippi_plat.c:78-86`) | `SceKernelLwMutexWork`, as the heap uses (`platforms/vita/game/heap.c:60-69`) |
| `sp_thread_start`, `sp_thread_join`, `sp_on_thread` | `threadCreate`, 32 KiB stack (`3ds:port/3ds/slippi/slippi_plat.c:190-224`) | `sceKernelCreateThread` with a CPU mask (`platforms/vita/game/gx_submit.c:86-88`) |
| `sp_log`, `sp_log_drain` | queue into `mp_native_log` (`3ds:port/3ds/slippi/slippi_plat.c:88-188`) | `melee_vita_log_info` (`platforms/vita/vita_log.h`). Release builds compile logging out, per `GRAPHICS-SETTINGS-RESEARCH.md` |
| `sp_read_file` | stdio | stdio, newlib |
| `sp_net_init`, `sp_net_exit` | `socInit` with a 1 MiB buffer, `acInit` (`3ds:port/3ds/slippi/slippi_plat.c:226-251`) | `sceSysmoduleLoadModule(SCE_SYSMODULE_NET)`, `sceNetInit`, `sceNetCtlInit` (`platforms/vita/updater/update_runtime.cpp:163-186`). newlib also does this on the first socket call (`newlib:vitanet.c:48-102`) |
| `sp_lan_ip_fallback` | `gethostid` | `sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS)` (`headers:include/psp2/net/netctl.h:37`, `113`) |
| `sp_wifi_status` | `ACU_GetWifiStatus` | `sceNetCtlInetGetState` (`platforms/vita/updater/update_runtime.cpp:184-186`) |
| `sp_app_running` | `aptMainLoop` | none needed. Vita has no equivalent loop (inferred) |
| `sp_data_dir` | `sdmc:/3ds/melee/slippi` | `ux0:data/melee/slippi` (the port keeps its data under `ux0:data/melee`, `platforms/vita/game/dvd.c:19`) |
| ENet socket layer | `enet/ctru.c` | stock `enet/unix.c` (inferred, see below) |
| `mp_platform_idle` | 1 ms `svcSleepThread` (`3ds:port/3ds/services.c:11`) | `sceKernelDelayThread(1000)` |
| `mp_native_slippi_wait_poll` | HOME, power and SELECT checks while waiting (`3ds:port/3ds/game.c:77-84`) | a Vita quit check, or nothing (inferred) |
| `mp_native_slippi_file_*`, `mp_native_file_write_async` | stdio and the port's async writer | stdio |
| Slippi menu file override | SD override index (`3ds:port/3ds/file_io.c`) | extend the DVD overlay table, which already maps 5 costume files to `ux0:data/melee/costumes/` (`platforms/vita/game/dvd.c:26-40`) |
| CSS pad filter `slippi_ui_pad` | reads 3DS keys (`3ds:port/3ds/services.c:70-71`) | a filter in `PADRead` (`platforms/vita/game/pad.c:258`) |

### Vita networking

- vitasdk newlib implements BSD sockets over SceNet, including `sendmsg`,
  `recvmsg`, `getsockname`, `fcntl(O_NONBLOCK)`, `poll`, `select`,
  `gethostbyname` and `getaddrinfo` (`newlib:socket.c:46-507`,
  `newlib:fcntl.c:105-108`, `newlib:poll.c:41`, `newlib:net/`).
- Stock ENet `unix.c` uses those calls and undefines `HAS_POLL`
  (`ish:../../../Externals/enet/unix.c:23-62`). It should build for Vita
  without a custom layer like `ctru.c` (inferred, not compiled).
- The release link line adds `-lSceNet_stub` and `-lSceNetCtl_stub` only for
  Debug, updater or profiler builds (`platforms/vita/build-full.ps1:454`,
  `463`, `468`). An online build must link them unconditionally.
- No game code opens a socket today. Only the updater and the profiler do
  (`platforms/vita/updater/update_runtime.cpp:160-222`,
  `platforms/vita/game/profiler_live.c:136-169`).

### Vita threads, timing and loop

- Vita threads today: the game thread, a render thread on core 1
  (`platforms/vita/game/gxm_game.c:1164-1165`), and the GX submit worker,
  audio and DVD threads on core 2 (`platforms/vita/game/gx_submit.c:86-88`,
  `platforms/vita/game/audio.c:356-358`, `platforms/vita/game/dvd.c:542-544`).
  A network thread needs a core. Core 2 is the likely home (inferred).
- The frame loop matches the decomp. Each queued pad sample runs one engine
  body, then one render (`src/melee/gm/gmscene.c:322-408`). The 3DS hook sites
  exist here as well. `HSD_PerfSetStartTime` opens each body
  (`src/melee/gm/gmscene.c:347`), and the body loop breaks on `unk_C`
  (`src/melee/gm/gmscene.c:405-407`). `PADRead` sits at
  `src/sysdolphin/baselib/controller.c:65`.
- The pad sampler alarm period is 1/60 s (`src/melee/lb/lb_0195.c:93-94`,
  `197`), the same 60 Hz the 3DS reports. The extra-sample function the 3DS
  calls to catch up, `fn_800195FC`, exists at `src/melee/lb/lb_0195.c:61`.
- Vita runs OS alarms only from `VIWaitForRetrace`, once per rendered frame.
  A late frame delivers up to 8 elapsed periods in a burst
  (`platforms/vita/game/vi.c:48-70`, `platforms/vita/game/os.c:368-404`).
  While the online driver waits inside a body, no new samples queue. That is
  the same as the 3DS (inferred).

### Endianness

- On the 3DS the engine is big-endian, so `mp_slippi_online_start_melee`
  copies the 0x138-byte Dolphin block straight into `StartMeleeData`
  (`3ds:port/engine/slippi/online.c:805-806`).
- On Vita `StartMeleeRules` and `PlayerInitData` are native little-endian
  structs with bit-fields (`src/melee/mn/types.h:97-262`). Only disc data uses
  `scalar_storage_order("big-endian")` (`src/pc/disc.h:1-35`, `148`).
- A Vita port needs a decoder from the big-endian block to the native struct,
  field by field. A plain `memcpy` would scramble the bit-fields and the
  multi-byte fields (inferred).
- The 3DS assembly bridge (`SDKBRIDGE`, `SDKBRIDGE6`) is not needed on Vita.
  The engine and the platform code are one little-endian binary.

### Settings and UI surfaces on Vita

- Vita has one screen. The 3DS puts the code keyboard, chat page, settings and
  match cards on the bottom screen (`3ds:docs/slippi/UI_PLAN.md:70-78`).
- Vita options live in a "VITA OPTIONS" panel under Options
  (`src/melee/mn/mnmain.c:832`, `platforms/vita/README.md:172-216`). That panel
  could host the delay and chat settings.
- `SceCommonDialog` is already linked (`platforms/vita/build-full.ps1:474`).
  Its IME dialog could take the connect code (inferred). Porting Slippi's
  name-entry patch would match Dolphin more closely.
- The debug menu already jumps to `GM_HANYU_CSS`
  (`src/melee/if/soundtest.c:1305-1312`). Replacing that scene's state table
  changes the debug entry too.

## Porting map

| 3DS file or module | Vita location | Action | Notes |
| --- | --- | --- | --- |
| `port/3ds/slippi/enet/*.c` except `ctru.c`, `unix.c` | `extern/enet/` or `platforms/vita/slippi/enet/` | Copy verbatim | Identical to Ishiiruka's copy |
| `enet/ctru.c` | none | Drop. Use stock `unix.c` | inferred to build against vitasdk newlib |
| `slippi_json.[ch]` | `src/slippi/` | Copy verbatim | |
| `slippi_config.c` | `src/slippi/` | Copy, change the `user.json` error text path | |
| `slippi_mm.c` | `src/slippi/` | Copy | Speaks only the old ENet/JSON protocol. See risks |
| `slippi_p2p.c`, `slippi_net.[ch]`, `slippi_internal.h` | `src/slippi/` | Copy verbatim | |
| `slippi_bridge.c` | `src/slippi/` | Copy, drop the 3DS prefetch call | |
| `slippi_plat.[ch]` | `platforms/vita/slippi/slippi_plat_vita.c` | Write new Vita branch | See the platform table |
| `slippi_ui.[ch]` | `src/slippi/` plus a Vita input filter | Adapt | Remove `<3ds.h>`. Replace bottom-screen pages with a Vita overlay or IME dialog |
| `slippi_boot_ui.c`, `slippi_prefetch.c`, `slippi_files.c` | none or `platforms/vita/slippi/` | Rewrite if needed | 3DS-specific. Vita reads the ISO, so load times differ (inferred) |
| `slippi_selftest.c`, `slippi_testpattern.h` | `src/slippi/` | Copy | Useful as a boot self-test against the fake peer |
| `port/engine/slippi/online.c` | `src/melee/slippi/` or `src/slippi/` | Adapt | Replace the block `memcpy` with a big-endian decoder. Replace `mp_platform_*` calls |
| `online_mode.c`, `online_hud.c` | same | Adapt | Check every decomp symbol and struct offset against `430c286` |
| `rules.c` | same | Adapt | Static asserts on `Fighter` offsets (`rules.c:43-50`) need checking on Vita |
| `replay.c` | same | Adapt | Needed as the determinism test |
| `tools/slippi_edits/*.py` | direct edits in `src/melee`, `src/sysdolphin` | Rewrite by hand | The anchors are `039c4bf` text. Vita sources are `430c286` with PC-port changes |
| `port/engine/ucf.c` | new | Write new or port | Vita has no UCF today (no `ucf` match in `src/` or `platforms/`) |
| `port/engine/fp_exact.c`, `ps_math.c`, MSL maths, `mp_fp.h` | `src/` | Adapt | Replace `src/placeholder.h:13-15` estimates |
| `tools/fp_contract.py`, `engine_build.py` changes | build scripts | Write new | The 3DS rewrites clang IR. Vita uses GCC (`platforms/vita/build-game.ps1:59`) |
| `tools/slippi/src/fake_mm.c`, `fake_peer.c` | `tools/slippi/` | Copy | PC-side test peers. Win32 only, needs a POSIX `slippi_plat` branch for Linux CI (inferred) |
| `tools/slippi/slippi_files.py`, `vcdiff.py`, `gamefiles/*.diff` | `tools/slippi/` | Copy | Output goes to the Vita DVD overlay folder |

### Risks

- Determinism is the largest item. Vita release builds compile the game with
  `-O3 -funsafe-math-optimizations -fno-signed-zeros -ffp-contract=fast` and
  NEON (`platforms/vita/build-game.ps1:78-83`). Those flags allow
  reassociation and NEON vectorization of float code. NEON on ARMv7 always
  flushes subnormals (inferred). The game also uses newlib `libm` and a
  `1.0 / sqrt(x)` stand-in for `frsqrte` (`src/placeholder.h:13-15`). Any of
  these can make the Vita disagree with Dolphin and end matches with DESYNC
  DETECTED. The fix is the 3DS approach applied to GCC. Online builds need
  `-ffp-contract=off`, no unsafe maths, explicit GameCube-style fused ops,
  exact estimate tables and the MSL maths functions.
- Decomp drift. Vita builds decomp `430c286` with PC-port changes. The 3DS
  determinism and rules edits were made and checked against `039c4bf`. Every
  hook and every no-contract function needs re-checking, and the 3DS census
  tooling expects clang IR (`3ds:docs/slippi/determinism.md:184-215`).
- CPU budget. Lockstep needs no resimulation, so the only requirement is 60
  engine bodies per second. The README says Vita performance "varies by scene;
  not locked" (`README.md:20`). Rendering may drop frames because the loop
  runs several bodies per render. If one engine body alone costs more than
  16.7 ms, the Vita falls behind, and the PC stalls once it is 7 frames ahead
  (`ish:HW/EXI_DeviceSlippi.cpp:1455`). The 3DS measured its exact-maths cost
  at about 1 to 1.5 ms per frame in the emulator
  (`3ds:docs/slippi/determinism.md:285-288`). Vita has update-time counters
  that can measure this (`src/melee/gm/gmscene.c:430-431`).
- Real rollback on Vita would cost far more. Each rollback reruns up to 7
  bodies in one frame, and each snapshot would cover the 24 MiB MEM1
  (`platforms/vita/game/vita_platform.h:7`) plus host-side state (inferred).
  That is not needed for parity with the 3DS.
- Wi-Fi latency. A lockstep peer turns late packets into visible waits. On a
  72 to 113 ms hotspot the 3DS waited 4345 times in a 6420-frame game
  (`3ds:docs/slippi/network.md:260`). Vita Wi-Fi is 2.4 GHz only like the 3DS
  (inferred, not checked in a source).
- Matchmaking protocol. The old ENet/JSON client may stop working once Slippi
  ships the WebSocket client. A new client needs TLS WebSocket and STUN on the
  netplay socket. The Vita updater already links curl and mbedTLS
  (`platforms/vita/build-full.ps1:460`). Whether that curl build has WebSocket
  support is unknown.
- Endianness of the game info block, described above.
- Vita-only gameplay changes. The PC port and Vita add code under
  `TARGET_PC` and `TARGET_VITA`. I spot-checked fighter and stage code and
  found only cosmetic and pointer plumbing changes
  (`src/melee/ft/ftparts.c:550-553`, `src/melee/gr/grpstadium.c:1165-1192`).
  I did not audit all of them.
- Policy. The 3DS presents itself to Slippi's server as a Slippi Dolphin
  version (`3ds:docs/slippi/SLIPPI_BETA.md:249`). Slippi's sources contain no
  statement that permits or forbids third-party clients
  (`3ds:docs/slippi/protocol_notes.md:331-338`). slippi-ssbm-asm is GPLv3, and
  the 3DS code is a port of GPL code (`3ds:port/3ds/slippi/slippi_net.c:1-6`).

## Open questions

- Does `mm.slippi.gg:43113` still accept `create-ticket` from clients
  reporting `3.6.4`, and for how long after Slippi ships `dbfc621`? Can an
  old-protocol ticket pair with a new-protocol ticket? The server source is
  private.
- The 3DS hardware results in `3ds:docs/slippi/BETA_RELEASE.md:53-57` and
  `3ds:docs/slippi/network.md:214-278` are the project's claims. I did not run
  any of them.
- Does stock ENet `unix.c` compile and behave against vitasdk newlib, in
  particular `getsockname` after a UDP `connect`, and non-blocking `recvmsg`?
- What does one engine body cost on Vita in a 1v1 on each legal stage, with
  exact maths turned on?
- Does vitasdk GCC give a way to reproduce MWCC's fusion choices? The 3DS
  approach depends on clang's `llvm.fmuladd` markers
  (`3ds:tools/fp_contract.py:1-30`). Melee Unlocked, which the 3DS docs cite,
  reportedly does this with GCC and source annotations
  (`3ds:docs/slippi/determinism.md:5-8`). I did not read Melee Unlocked.
- What FPSCR state do Vita game threads run with? The 3DS forces round to
  nearest with flush-to-zero (`3ds:docs/slippi/determinism.md:151-182`). I
  found no FPSCR handling in the Vita platform code.
- Which desync rule should Vita copy, the 3DS rule or Slippi's "Desync Risk"
  rule?
- Do sockets survive a Vita suspend with the PS button? I did not check.
- Is the 3DS `chat` default 1 (code) or 0 (doc)?
