**Superseded: Michael declined the full-backup/encrypted-container workflow. Do not execute these steps or request a password. No full backup is a release prerequisite.**

# Private backup container: user step

The complete flash read is approved, but has not started. This is a blank local container, not an image of any attached disk or the robot. Apple's current guide: https://support.apple.com/guide/disk-utility/create-a-disk-image-dskutl11888/mac

1. Open **Disk Utility**, then **File → New Image → Blank Image**.
2. Save as **installed-state-20261008.dmg**. In the save dialog press **Command–Shift–G**, paste `/Users/mikedemott/Developer/AgentWork/StackChan/.private-recovery/`, and choose that folder.
3. Name **STACKCHAN-RECOVERY**; Size **1 GB**; Format **APFS**; Encryption **256-bit AES**. Enter and confirm your password directly in Disk Utility; leave any Remember in Keychain option unchecked.
4. Partitions **Single partition – GUID Partition Map**; Image Format **read/write disk image**. Click **Save**, then **Done**. Keep the newly mounted image open and tell the project task the container is ready; never send the password.

1 GB is a conservative capacity allowing APFS overhead and two 16 MB reads plus manifests. This Mac currently has about 120 GiB available. Settings follow Apple's published guide; the current UI was not successfully captured by automation. If options differ, stop before saving and describe the option labels only.

After creation, the agent must verify encryption, resolved file/mount location, exclusion from unapproved backup/export, and confidential remount/password behavior before any credential-bearing read. User approval persists; container creation alone is not a verification receipt. No flash/reset/erase or live audio is authorized by this backup approval.
