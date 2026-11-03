# Provider companion source provenance

The new provider catalog, setup views and `companion_events.py` are independent implementations. No donor source, dependency, installer or artwork is copied or executed. These pinned references informed requirements and tests:

| Reference | Pinned revision | License/reuse boundary | Ideas retained |
| --- | --- | --- | --- |
| [Agent Buddy](https://github.com/DanWahlin/agent-buddy) | `d2d52d48e538b1eede994b43b20bd3d42791bfd4` | MIT code; branding and character rights separate | Lifecycle observations, agent-specific activity, decorative idle vs real work, renderer parity |
| [Codex Cardputer / CodexDeck](https://github.com/dkyazzentwatwa/codex-cardputer) | `68baf99af9c954664a5166c5dc26035c9e659167` | No recognized root code license found; no code copied | App Server-owned terminal receipts and exact pending response custody; authenticated rewrite needed for device ingress |
| [Code-chan](https://github.com/stelis-dev/code-chan) | `54cbf26aaacffbff6f34217e2388e0015e859f0b` | Apache-2.0; preserve notices for any later actual reuse | Foreground/provider identity, turn/request correlation, explicit decisions, disconnect/replacement rejection |
| [Hermes-StackChan](https://github.com/circlemouth/Hermes-StackChan) | `018666f30fafbd66e59b6a245db7b6bd16dcc1f8` | MIT firmware; inspected new ai-server has no applicable license found; SanoTTS separate restrictions | Distinct Hermes voice/tool backend, task generation and joined cleanup requirements |
| [MuseChan](https://github.com/Tjtelenda/musechan) | `4407c712e2c62e3f6e3fc31f1824ad399e8afed1` | Apache-2.0 code; third-party dependencies separate; Jollybot art excluded | CoreS3 board/API reference, distinct Muse account/session, per-provider world and device behavior |

Primary protocol references checked on 2026-10-27:

- [Codex hooks](https://developers.openai.com/codex/hooks/): active turn correlation and bounded allow/deny hook response.
- [Codex App Server](https://developers.openai.com/codex/app-server/): started/completed turn notifications and terminal status. A Stop hook alone is not a final completion receipt.
- [Claude Code hooks](https://code.claude.com/docs/en/hooks): session/prompt correlation and PermissionRequest response. Missing native correlation is rejected rather than guessed.
- [Muse SDK token terms](https://gadgets.muse.ai/sdk-terms): source license and actual service/token access are separate. No token was requested/read/provisioned and no terms were accepted on the user's behalf.

Fixtures are synthetic shapes based on these contracts, not captured proof from Michael's installed clients. No live CLI/App Server/hook/audio/provider session was started. Existing established Mira art is copied unchanged under the earlier project handoff; none of these donor characters replaces it.

## Later repository reuse references (2026-10-31)

- [Nous-Girl Hermes tool adapter](https://github.com/dax8it/nous-girl-stackchan/blob/fe8b02342a382598de1faa5c9bc93144549ea9e9/server/addons/stackchan-tools/hermes/stackchan_mcp.py), commit `fe8b02342a382598de1faa5c9bc93144549ea9e9`, Git blob `ca9f2aafe48bae083b4b01408028c39bada6aa83`: MIT code reference. Its fixed typed-tool pattern was independently rewritten for this firmware's actual RPC names, strict arguments and captured owner. No donor server, global device registry, query-token route, camera/dance/SD/music tool or artwork was imported.
- [CustomFW firmware patch](https://github.com/7H3-CH053N/StackChan-CustomFW/blob/fd563a14c80effecff4cb7fb025c46f2fd8b8d29/firmware/xiaozhi-esp32.patch), commit `fd563a14c80effecff4cb7fb025c46f2fd8b8d29`, Git blob `a985df41b0c4cc56eb0ae483633f91e6be627e44`, patch SHA256 `d2d3c313145f3f7e71976355f9da00321de69b34ecf046c1176df21986b9a032`: MIT reference. The bounded follower is an original implementation of the acceleration/speed-limited technique. No full board patch, binary, character artwork, autostart camera/motion/audio, partition or coredump change was imported. The selected profile leaves bounded motion off; physical stop/rearm is not qualified.

The actual portable Muse SDK code port has its own retained Apache-2.0 notices and provenance under `firmware/components/muse_direct_protocol`; those source rights do not establish account/token entitlements or character-art rights. Optional fuzzy Muse/Clawd payloads remain absent. The existing accepted Mira PNG remains unchanged; preservation is not a claim of blanket licensing clearance for other artwork.
