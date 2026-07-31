# Windows server remap requirements from the active Mac profile

## Source and intent

This inventory was extracted on 2026-07-31 from the selected Karabiner profile
`Default profile` on `ESKui-MacBookPro`.

- Source SHA-256:
  `d6c8b1c693150a5b6c952376aced07c881120a4e5ac868ea7fb6b304ac087688`
- 47 enabled rule groups
- 86 manipulators

Karabiner-free operation is a useful secondary outcome. The product should
also preserve a future path for remote input to participate in Karabiner
rules. Do not treat every rule below as an unconditional server remap:
Karabiner currently uses target-app and input-source conditions that the
Barrier server cannot observe.

The first successful live contract is:

```text
Windows Right Alt / Hangul tap
-> server remapper F19
-> macOS Barrier client
-> macOS F19 input-source shortcut
```

Keep this working while expanding the rule set.

## Phase 0: required keyboard identity rules

Implement and test these first for the Mac target screen:

```text
right_alt tap       -> F19
right_alt hold      -> right_super
hangul tap          -> F19
hangul hold         -> right_super

right_super tap     -> F19
right_super hold    -> right_alt

shift+space         -> F19
lang1               -> F19
lang2               -> right_alt+return
```

Notes:

- The active Karabiner description still says F18 for Right Command, but its
  actual `to_if_alone` value is F19.
- Tap output must be one balanced down/up pair with no source modifier left in
  the mask.
- Hold output must remain a real modifier for combinations.
- The Right Command rule currently excludes Terminal, iTerm2, Hyper,
  Alacritty, and Kitty. Until app context exists, expose this rule as an
  explicit config option rather than silently applying it everywhere.
- `lang2` currently applies only while the Mac input source is Korean and
  excludes remote-desktop and terminal applications.

## Phase 1: Windows-style global chords

These are the useful, mostly application-independent transformations in the
active profile:

```text
control+c           -> command+c
control+v           -> command+v
control+x           -> command+x
control+z           -> command+z
control+y           -> command+shift+z
control+a           -> command+a
control+s           -> command+s
control+n           -> command+n
control+f           -> command+f
control+g           -> command+g
control+w           -> command+w
control+t           -> command+t
control+b           -> command+b
control+i           -> command+i

alt+f4              -> command+q
print_screen        -> command+shift+4
f5                  -> command+r

home                -> command+left
shift+home          -> command+shift+left
control+home        -> command+up
control+shift+home  -> command+shift+up
end                 -> command+right
shift+end           -> command+shift+right
control+end         -> command+down
control+shift+end   -> command+shift+down

control+left        -> option+left
control+shift+left  -> option+shift+left
control+right       -> option+right
control+shift+right -> option+shift+right
control+up          -> up
control+shift+up    -> shift+up
control+down        -> down
control+shift+down  -> shift+down
```

The current Karabiner rules exclude terminal, remote-desktop, virtualization,
X11, and Parsec applications from most of this group. A server-only first
version may offer these as a named `windows-style-mac` preset, but it must be
opt-in and the missing exclusions must be documented.

The active profile also contains these modifier-navigation rules:

```text
option+tab          -> command+tab
option+shift+tab    -> command+shift+tab
command+tab         -> command+option+0
```

Their descriptions use Windows-oriented names that do not exactly match the
Karabiner `from` modifiers. Preserve the actual mappings above.

## Phase 2: target-application rules

These cannot be reproduced correctly from the Windows server until the Mac
client reports frontmost application context, or until remote events can pass
through Karabiner.

### Figma Desktop

```text
control+d           -> command+d
control+g           -> command+g
control+shift+g     -> command+shift+g
control+option+g    -> command+option+g
control+slash       -> command+slash
left_control        -> left_command
command+y           -> command+shift+z
left_command        -> left_option
```

### Photoshop and Illustrator

```text
left_control        -> left_command
command+y           -> command+shift+z
left_command        -> left_option
```

### Finder

```text
control+x           -> command+c
control+v           -> command+option+v
return              -> command+o
f2                  -> return
delete_forward      -> command+backspace
backspace           -> command+up
```

### Browsers only

```text
control+l           -> command+l
control+r           -> command+r
```

The browser set is Firefox variants, Edge, Chrome, Brave, and Safari.

## Phase 3: input-source-dependent rules

These need Mac input-source context and must not be made unconditional:

```text
Korean grave/tilde  -> option+grave/tilde
German control+y   -> command+y
non-German control+z -> command+z
German control+z   -> command+shift+y
non-German control+y -> command+shift+z
Korean lang2       -> right_option+return
```

The duplicate German Y/Z rules are intentional because the active profile
accounts for the German keyboard layout.

## Phase 4: consumer keys, pointer actions, and local commands

The active profile contains bidirectional top-row/media mappings:

```text
option+F1..F12 -> brightness down, brightness up, Mission Control, Launchpad,
                 keyboard illumination down/up, rewind, play/pause,
                 fast-forward, mute, volume down/up

the same 12 consumer keys -> F1..F12
```

It also contains:

```text
control+left_click -> command+left_click   (except Figma)
command+l          -> run CGSession suspend
control+escape     -> Launchpad
control+shift+escape -> open Activity Monitor
```

These require consumer-key support, mouse-button remapping, or execution on
the Mac. Shell commands must never be accepted from an untrusted network
configuration. Keep them out of the initial server-remap subset.

## Engine capabilities needed

The current inventory requires more than single-key replacement:

1. Tap-hold with a timeout and balanced cleanup.
2. Left/right modifier identity.
3. Chord output containing multiple modifiers.
4. Preservation of optional Shift for navigation selections.
5. Ordered key-down/key-up output so modifiers cannot stick.
6. Repeat policy per rule.
7. Consumer-key and pointing-button event types in later phases.
8. Optional Mac-to-server context for frontmost app and input source.

Do not hard-code this inventory into platform key injection. Keep it in the
server `KeyRemapper` and its configuration model so each target screen can
select a preset and override individual rules.

## Recommended implementation order

1. Preserve the proven F19 Right Alt/Hangul tap behavior.
2. Add the Phase 0 identity rules and tests.
3. Add a `windows-style-mac` opt-in preset for Phase 1.
4. Test copy/paste, undo/redo, Alt+F4, Home/End, word selection, modifier
   cleanup, repeat, disconnect, and screen transitions.
5. Design app/input-source context before implementing Phases 2 and 3.
6. Treat Phase 4 as separate event-type and security work.
