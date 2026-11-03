#pragma once
// CMake supplies explicit 0/1 values. Unprofiled source consumers retain the
// existing full catalog; no change to persisted provider IDs or runtime grants.
#ifndef STACKCHAN_BUILD_HERMES
#define STACKCHAN_BUILD_HERMES 1
#endif
#ifndef STACKCHAN_BUILD_MUSE
#define STACKCHAN_BUILD_MUSE 1
#endif
// Dormant protocol support is explicit and adds no native voice authority.
#ifndef STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#define STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL 0
#endif
#ifndef STACKCHAN_BUILD_CLAUDE
#define STACKCHAN_BUILD_CLAUDE 1
#endif
#ifndef STACKCHAN_BUILD_CODEX
#define STACKCHAN_BUILD_CODEX 1
#endif
#ifndef STACKCHAN_BUILD_MUSIC
#define STACKCHAN_BUILD_MUSIC 1
#endif
#ifndef STACKCHAN_BUILD_GAMES
#define STACKCHAN_BUILD_GAMES 1
#endif
#ifndef STACKCHAN_BUILD_TOOLBOX
#define STACKCHAN_BUILD_TOOLBOX 1
#endif
#ifndef STACKCHAN_BUILD_NETWORK_TOOLS
#define STACKCHAN_BUILD_NETWORK_TOOLS 1
#endif
#ifndef STACKCHAN_BUILD_LED_CONTROLS
#define STACKCHAN_BUILD_LED_CONTROLS 1
#endif
#define STACKCHAN_BUILD_PROVIDERS (STACKCHAN_BUILD_HERMES || STACKCHAN_BUILD_MUSE || STACKCHAN_BUILD_CLAUDE || STACKCHAN_BUILD_CODEX)

// Optional bounded follower is source-only until its native owner qualifies.
#ifndef STACKCHAN_BUILD_BOUNDED_MOTION
#define STACKCHAN_BUILD_BOUNDED_MOTION 0
#endif
