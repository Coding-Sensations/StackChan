# Pending installation approval — no commands executed

Source/offline qualification is complete. This plan requires explicit parent/user
approval before installing a driver, reloading audio services or migrating aliases.
The existing authorization for opaque firmware backup/app-only flash does not
authorize these Mac changes. This candidate remains uninstalled and unselected.

Reviewed local deliverables in the executor's `pr-preparation/host-qualification`:

| Artifact | SHA256 |
|---|---|
| `StackChanDirectionalAudio.driver/Contents/MacOS/StackChanDirectionalAudio` | `22a413581149e9d71358b71170deafe8075f0cdac5667a9bd5c09f876b1263e8` |
| Driver `Contents/Info.plist` | `be5dd39ab1884f85f98320b121fd9c8508ea5a16d22578d53b2cb2ded048f1b6` |
| `libstackchan_audio_directional_candidate.dylib` | `632bf7c24f46d61189f165aa63245bfae347f705f3c0a998694d6ff6d164b3cd` |
| `StackChanPairing-direction-source-only` | `c4d64e1bab94d878ed929884203b3af6446c46d6eed433ae95c38d54a6232ac1` |

The smallest next approval covers the following installation/metadata phase:

1. Agree an interruption window with the user; a CoreAudio service reload affects
   unrelated audio clients. Recheck artifact/source pins, free disk and ownership.
   Read current defaults/client use and exact legacy aggregate configuration into
   an owner-only rollback record. Do not capture audio or alter any default route.
2. Install only the pinned bundle at
   `/Library/Audio/Plug-Ins/HAL/StackChanDirectionalAudio.driver`, with normal root
   ownership/permissions. Do not overwrite an existing different bundle. No new
   driver download, credential, signing enrollment, privacy permission, security
   bypass or reduced protection is part of this plan. If the OS requires an
   unavailable distribution identity or refuses the candidate, report that gate.
3. Reload the audio service during the agreed window. Read metadata only: exact
   plugin identity, four UIDs, scoped stream/channel counts, public/hidden flags,
   48kHz format, shared clock, default eligibility and unchanged defaults. Verify
   the two public devices each appear only in their intended OS category; hidden
   transport endpoints resolve by UID and are absent from the visible list.
4. Only after new endpoint verification, migrate the two exact owned legacy
   aggregates: `local.mrkillbob.StackChan.microphone.v1` backed solely by2-A and
   `local.mrkillbob.StackChan.speaker.v1` backed solely by2-B. Verify names, aggregate
   class and backing-UID list before removing either. Leave internal buses and
   every other device intact. If an alias is active/default, defer removal rather
   than silently switching routes. Re-read visible-device metadata and certify
   no old duplex alias remains; preserve the rollback configuration.

This phase does not open AUHAL IO, replace the installed pairing app, alter the
worker/profile or trust binding, activate camera/wake, or flash firmware. The
normal worker still uses legacy internal2-A/2-B UIDs. The additive directional
entry point has no production caller yet.

A subsequent separately reviewed activation step must choose the pinned new
native library and explicit directional entry point, with a matching public trust
binding rather than weakening the library hash check. Preserve physical Listen,
existing authentication, codec/half-duplex/epoch gates and checked close. At that
later stage measure real latency, restart/removal behavior, multi-client mixing
and both signal directions. Existing firmware backup/flash authorization persists,
but the agreed all-upgrades hold and parent direction still control deployment.

Rollback: remove only the exact newly installed bundle whose bytes match these
pins, restore only the saved owned aggregate definitions if migration occurred,
and reload CoreAudio in an agreed window. Confirm the original2-A/2-B buses,
defaults, saved pairing and installed idle app remain intact. Never substitute a
broader driver uninstall, default-route reset or security change.
