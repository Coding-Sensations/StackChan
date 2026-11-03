# Stackchan companion adapter

The Stackchan tile opens a configured session directly after ownership and network
checks. Missing setup appears inline with a Settings action; Wi-Fi and account
setup remain in Settings. Intentional tile entry authorizes the existing stock
runtime. Boot, reconnect and legacy boot_ai do not authorize capture or motion.

The companion face has no persistent Stop/Home footer. Swipe up from the bottom
edge to reveal the original firmware Home button and temporary session status.
Home immediately requests cancellation and revokes the stock audio lease. The
outer session loop keeps this control reachable during startup and shutdown,
retains ownership while workers stop, and restores the launcher after checked
cleanup. It does not reset the MCU. Other apps retain their original first-show
Home behavior.

Stock acquires AudioOwner::Stock before touching the codec. Mira must finish
closing its resources before Stock can start. Idle enables the existing wake
word; the temporary Home overlay reports that accurately. StopListening is not
advertised as microphone mute.

Stock retains official hotspot/browser Wi-Fi configuration and activation-code
flows. Existing Wi-Fi and identity are read from NVS. Phone-entered Wi-Fi and
returned MQTT/WebSocket settings are RAM-only for the current session; saved
NVS is never erased or overwritten by this adapter. A missing UUID is generated
in RAM only after Start. Phone account/device activation stays with the official
vendor app; this firmware does not collect account credentials.

The explicitly started Stock session now restores original camera, motor, head-pet, IMU, speaking/idle reactions, MCP tools and reminder behavior through Hal.startXiaozhi. The stopped selector and Companion do not activate those capabilities. The vendor idle state enables its local wake word;
the status explicitly says so. StopListening is not advertised as microphone mute.

`generate_backend.py` deterministically adapts pinned Xiaozhi commit
`e77dedb1309153bb63fed285772962c920c97dd4` and reviewed esp-wifi-connect 3.1.1
files into the build directory. Hotspot advanced-settings writes are rejected;
Stock-session Wi-Fi setup remains RAM-only. The dedicated Wi-Fi Setup app/Setup worker instead persists successfully tested choices to a bounded versioned CRC-protected atomic NVS blob; legacy keys remain a fallback. Both Stock and Companion read the same saved SSID manager. Acoustic and BLUFI build modes are rejected
because this adapter supports the hotspot/browser flow only. SHA-256 pins fail closed on source drift.
The surrounding official StackChan reference is
`1b5765599fba8aaad1811d9a79358ccc7051f5f3`.
Generated variants suppress asset downloads, firmware upgrade code and user-only
update tools; the vendor OTA endpoint is contacted solely for activation and
protocol configuration, not firmware download or installation.

Source generation can be checked without building or device access:

```sh
python main/apps/stock_agent/generate_backend.py \
  --xiaozhi xiaozhi-esp32/main \
  --wifi managed_components/78__esp-wifi-connect \
  --output /tmp/stock-agent-backend-check
```

On-device pairing, audio, network and UI behavior require separately authorized
hardware validation. No such validation is claimed by the source adapter.
