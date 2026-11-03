**Later acceptance:** Michael reports the exact muted custom screen (Sentinel_3fc76a3036e08191857785f0813b27c4). Display boot acceptance is now user-confirmed; touch, pairing and audio remain unverified. Earlier execution observations below are historical.

# Limited flash execution result

Michael approved the previously specified app/OTA write, verification and one reboot. Approval question Sentinel_e078488362608191964b0b7cb15a2465; verified owner response Sentinel_635aba1cc38c819193341e47b895b7d9: “Approval for all that you still need”.

Fresh checks matched ESP32-S3 rev0.2, 16MB flash, MAC80:45:6b:56:14:04, secure boot/encryption disabled, prior partition hash and exact file hashes. The write command targeted only app0x20000 and OTA metadata0xd000. Explicit verify_flash returned digest matches for 2,577,504 app bytes and 8,192 OTA bytes. Write took30.23s; verification4.65s. NVS/calibration and other partitions were outside the explicit write/erase ranges; no NVS read or full backup was done.

One approved software reset was executed. The following passive10s serial window showed USB_UART_CHIP_RESET and DOWNLOAD(USB/UART0), then “waiting for download”. The custom screen has NOT been observed or confirmed. No second software reset or automatic reflashing was performed. Next hardware step is to leave download mode, using a short press/release of the RST button (not the3s download-mode hold), then check for the muted Mira companion screen. User physical observation is still needed. No remote Mira audio is ready.

Exact machine receipt: LIMITED-FLASH-RESULT.json. Detailed command logs: project notes/limited-flash-evidence. Factory recovery artifact remains available; no recovery write was attempted. Full backup/encryption remains declined.
