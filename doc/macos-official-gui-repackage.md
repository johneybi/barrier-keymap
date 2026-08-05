# macOS official GUI package

The macOS client package keeps Barrier 2.4.0's original Qt GUI and menu bar
integration. Only the bundled `barrierc` executable is replaced with the
patched client that maps the upstream F16 slot to F19.

This is intentionally different from wrapping `barrierc` in a second launcher.
The original `barrier` process continues to own configuration, start and stop
actions, log display, connection state, and the menu bar icon. It starts the
patched client with `--no-tray`, exactly as the upstream GUI starts its bundled
client.

## Build the app

Start with the official Barrier 2.4.0 macOS app and a patched `barrierc` built
from this branch:

```sh
./dist/macos/repackage_official_app.sh \
  "/Applications/Barrier-original-2.4.0.app" \
  "/path/to/patched/barrierc" \
  "/tmp/Barrier Keymap.app"
```

The script refuses to overwrite an existing output, changes the bundle identity
to `com.johneybi.barrier-keymap`, adds the local-network usage description, and
ad-hoc signs and verifies the complete bundle.

## Install and authorize

Move the generated app to `/Applications` before granting permissions. In
System Settings, add that exact app under Privacy & Security > Accessibility.
If another experimental Barrier Keymap app was authorized previously, remove
the old entry first and keep old app bundles outside `/Applications` while
authorizing the replacement. macOS associates Accessibility approval with the
specific bundle identity, signature, and path; an old bundle with the same
display name can otherwise make the permission list misleading.

The expected runtime process tree is:

```text
Barrier Keymap.app/Contents/MacOS/barrier
  -> Barrier Keymap.app/Contents/MacOS/barrierc --no-tray ...
```

The menu bar icon comes from the original GUI process, not from a custom
launcher.
