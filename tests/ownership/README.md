# Ownership regressions

A synthetic 4D engine (`engine_harness.cpp`) replaces the plugin-API callback (`gCall4D`). It links the real plugin sources (`4DPlugin-ExifTool.cpp`, `support/4DPlugin-JSON.cpp`, `support/TagInfo.cpp`) and the bundled, unmodified `4D Plugin API/4DPluginAPI.c`. It tracks every engine-owned string, picture, object, collection and handle, and runs under ASan/UBSan.

```sh
tests/ownership/run.sh . fixed        # every test must pass
D=$(mktemp -d); git archive <base-sha> | tar -x -C "$D"
tests/ownership/run.sh "$D" baseline  # named defect_*/guard_* tests must fail, control_* must pass
```

ExifTool itself never runs. The plugin finds it through `[NSBundle bundleWithIdentifier:@"com.4D.ExifTool"]`, which is nil in the harness, so every command stops with `exiftool not found in the plugin bundle` before a process exists. The tests assert that message.

Contracts enforced:

- `PA_GetObjectProperty` and `PA_GetCollectionElement` return an owned copy. Read `PA_GetLastError()` first. On `eER_NoErr`, `PA_ClearVariable` the copy, whatever its kind; on an error, neither read nor clear it.
- `ob_get_o` and `ob_get_c` return an object or collection reference that the caller now owns and must release. A value of the wrong kind is cleared.
- `PA_SetObjectProperty` and `PA_SetCollectionElement` deep-copy; the caller still clears its own variable.

Limits:

- macOS only. The Windows build is not compiled or run.
- The engine semantics are modelled, not a real 4D engine.
- No claim is made about how the engine treats a value already held by the out-parameter of `PICTURE TO BLOB` or `BLOB TO PICTURE`.
- `ob_get_b`, `ob_get_c` and `ob_get_o` have no callers in the plugin; they are tested directly.
