
## Build

### Fetch Dependencies

```bash
python3 ./fetch_repos.py
```

### Tool Chains

[ESP-IDF v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/index.html)

### Build

```bash
idf.py build
```

### Host-side tests

The suite includes motion, navigation, companion teardown and screensaver tests.
It does not access hardware. Several tests compile generated code from pinned
vendor sources and therefore require dependencies before running:

- Run `python3 fetch_repos.py` in this directory to prepare the revisions in
  `repos.json`, including the Xiaozhi patch and Mooncake.
- Resolve `main/idf_component.yml` through the configured ESP-IDF component
  manager (`idf.py reconfigure`) to prepare managed components. This is a build
  step; do not run flash or device commands.
- For an offline checkout, copy these existing caches from a previously prepared
  checkout into the same relative paths: `xiaozhi-esp32`,
  `components/mooncake`, `components/esp-now`,
  `managed_components/78__esp-wifi-connect`, and
  `managed_components/78__esp-ml307`. The backend and teardown generators reject
  inputs that differ from their checked-in SHA-256 pins. Mooncake's revision is
  pinned in `repos.json`; preserve its exact source.

A complete qualification must report all tests executed with no skips. A bare
checkout lacks those caches: some tests fail and two explicitly skip. Do not
interpret a partial run as installed-firmware acceptance.

From the firmware directory:

```bash
cmake -S tests -B build-host-tests
cmake --build build-host-tests
ctest --test-dir build-host-tests --output-on-failure
```

### Flash

```bash
idf.py flash
```
