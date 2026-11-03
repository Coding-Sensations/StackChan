# Smallest input needed for screensaver removals

**Simplest:** provide the names you deselected, exactly as the Settings screen labels them. No additional device browsing or setting changes are needed if you already have that list.

**Equivalent saved configuration:** provide only the current NVS string from namespace `screensaver`, key `playlist`. In this source catalog it is 90 characters: `1:0:` (sequential) or `1:1:` (random), followed by 86 hexadecimal digits encoding 342 stable slide-selection bits. This is one non-secret setting; no whole NVS/flash dump, credentials or other keys are needed. The matching firmware/catalog identity is needed if the catalog has changed since that selection was saved.

The exact read-only firmware operation, in a context where NVS is already initialized, is:

```cpp
nvs_handle_t handle;
auto error = nvs_open("screensaver", NVS_READONLY, &handle);
if (error == ESP_OK) {
    char playlist[91]{};
    size_t bytes = sizeof(playlist);
    error = nvs_get_str(handle, "playlist", playlist, &bytes);
    // On ESP_OK, export only playlist[0..bytes-2]. Do not write or commit.
    nvs_close(handle);
}
```

No installed, already authorized public diagnostic command exposing this key was identified. The snippet specifies a narrowly scoped reader; it was not installed or executed against the device. A names list is the smallest currently available path. No reset/download-mode flash reader was used, no assets were guessed, and no assets have been removed for this request.
