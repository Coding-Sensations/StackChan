# Muse direct protocol source notices

The pairing and noise_core files are imported/adapted from Meta Platforms, Inc. and affiliates, Muse Gadget SDK commit 729d0c8b3113c0cdbd7efb3b8aad8a313839a1bd, under Apache License 2.0. Each original source retains its notice. The full license is included in Apache-2.0.txt.

Modifications: IDF5.5.4 public MbedTLS3 header compatibility, community-only epoch and hardware-attestation exclusion, standalone component CMake selection, and the new dormant direct_pairing_api facade. The new facade/glue is also licensed under Apache-2.0. MbedtlsCryptoBackend is the upstream implementation, selected instead of PsaCryptoBackend. The nested original noise_core CMake/PSA source are retained as upstream source context but are not selected by this component's build.

No source license conveys account entitlement, token rights, pairing approval or device-write permission. No donor avatar/art/font/BSP/media implementation is included.

The new `firmware/main/companion/ble_owner.hpp` and `muse_direct_*` ownership, worker, storage, HTTPS verifier, audio lease and runtime adapter files are also Apache-2.0. Changes to pre-existing StackChan files retain those files’ existing notices/licenses. These additions perform no hardware/account action merely by linking.
