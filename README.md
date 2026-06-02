# Barrier Keymap

Eliminate the barrier between your machines, with key remapping handled before
Barrier sends remote input to the active screen.

Barrier normally relays keyboard events from the server machine to the selected
client screen. Once those events arrive at the receiving OS, tools such as
Karabiner-Elements or AutoHotkey may not see them as ordinary local keyboard
input, or may see them too late to preserve the intended modifier behavior. This
build solves that problem inside Barrier: the server translates selected key
events before they are delivered to the target screen.

The normal Barrier mouse-edge switching UX is unchanged. The added keymap layer
only affects configured keyboard rules on configured screens.

## Downloads

Download Linux and macOS builds from this repository's
[releases](https://github.com/johneybi/barrier-keymap/releases). Each release
includes `.sha256` files for checking the downloaded archives.

## Server-side key remaps

Remaps are configured in a `section: remaps` block and can be scoped per target
screen:

```text
section: remaps
  mac:
    right_alt = right_super
    right_super.alone = F19
    right_super.hold = right_super
    control+space = F19

  windows:
    left_super = left_control
    right_super.alone = hangul
end
```

Supported behavior includes direct key remaps, left/right modifier key names,
tap-hold rules such as `right_super.alone`, and hotkey-style chord remaps such
as `control+space = F19`.

See [server-side key remaps](doc/key-remaps.md) for the supported config subset,
examples, limitations, and verification commands. See the
[release checklist](doc/release-checklist.md) for the publish and manual
verification flow used by this build.

## Relationship to Barrier

This is an unofficial modified build of Barrier focused on server-side key
remapping. It is not affiliated with or endorsed by the upstream Barrier
maintainers.

The original Barrier project and its package ecosystem remain available from
the upstream repository at <https://github.com/debauchee/barrier>.

### What is it?

Barrier is software that mimics the functionality of a KVM switch, which historically would allow you to use a single keyboard and mouse to control multiple computers by physically turning a dial on the box to switch the machine you're controlling at any given moment. Barrier does this in software, allowing you to tell it which machine to control by moving your mouse to the edge of the screen, or by using a keypress to switch focus to a different system.

Barrier was forked from Symless's Synergy 1.9 codebase. Synergy was a commercialized reimplementation of the original CosmoSynergy written by Chris Schoeneman.

At the moment, barrier is not compatible with synergy. Barrier needs to be installed on all machines that will share keyboard and mouse.

### What's different?

Whereas Synergy has moved beyond its goals from the 1.x era, Barrier aims to maintain that simplicity.
Barrier will let you use your keyboard and mouse from one computer to control one or more other computers.
Clipboard sharing is supported.
That's it.

### Project goals

Hassle-free reliability. We are users, too. Barrier was created so that we could solve the issues we had with synergy and then share these fixes with other users.

Compatibility. We use more than one operating system and you probably do, too. Windows, OSX, Linux, FreeBSD... Barrier should "just work". We will also have our eye on Wayland when the time comes.

Communication. Everything we do is in the open. Our issue tracker will let you see if others are having the same problem you're having and will allow you to add additional information. You will also be able to see when progress is made and how the issue gets resolved.

### Usage

Install and run barrier on each machine that will be sharing.
On the machine with the keyboard and mouse, make it the server.

Click the "Configure server" button and drag a new screen onto the grid for each client machine.
Ensure the "screen name" matches exactly (case-sensitive) for each configured screen -- the clients' barrier windows will tell you their screen names (just above the server IP).

On the client(s), put in the server machine's IP address (or use Bonjour/auto configuration when prompted) and "start" them.
You should see `Barrier is running` on both server and clients.
You should now be able to move the mouse between all the screens as if they were the same machine.

Note that if the keyboard's Scroll Lock is active then this will prevent the mouse from switching screens.

### Contact & support

For key remapping behavior, packaging, or release issues in this build, open an
issue in this repository:
<https://github.com/johneybi/barrier-keymap/issues>.

For bugs that also reproduce in upstream Barrier without the `section: remaps`
configuration, check the upstream Barrier project:
<https://github.com/debauchee/barrier>.

### Contributions

Contributions are welcome, especially around key remap rules, platform key-code
coverage, tests, and release packaging.

## Distro specific packages

While not a comprehensive list, repology provides a decent list of distro
specific packages.

[![Packaging status](https://repology.org/badge/vertical-allrepos/barrier.svg)](https://repology.org/project/barrier/versions)

## FAQ - Frequently Asked Questions

**Q: Does drag and drop work on linux?**

> A: No *(see [#855](https://github.com/debauchee/barrier/issues/855) if you'd like to change that)*


**Q: What OSes are supported?**

> A: The [most recent release](https://github.com/debauchee/barrier/releases/latest) of Barrier is known to work on:
>  - Windows 7, 8, 8.1, 10, and 11
>  - macOS *(previously known as OS X or Mac OS X)*  
>    - _The current GUI does **not** work on OS versions prior to macOS 10.12 Sierra (but see the related answer below)_
>  - Linux
>  - FreeBSD
>  - OpenBSD


**Q: Are 32-bit versions of Windows supported?**

> A: No


__Q: Is it possible to use Barrier on Mac OS X / OS X versions prior to 10.12?__

> A: Not officially.
>   - For OS X 10.10 Yosemite and later:
>     - [Barrier v2.1.0](https://github.com/debauchee/barrier/releases/tag/v2.1.0) or earlier _may_ work.
>   - For Mac OS X 10.9 Mavericks _(and perhaps earlier)_:
>     1. the command-line portions of the [current release](https://github.com/debauchee/barrier/releases/latest) _should_ run fine.
>     2. The GUI will _not_ run, as that OS version does not include Apple's *Metal* framework.
>         - _(For a GUI workaround for Mac OS X 10.9, see the [discussion at issue #544](https://github.com/debauchee/barrier/issues/544))_

> Note: Only versions [v2.3.4](https://github.com/debauchee/barrier/releases/tag/v2.3.4) and [later](https://github.com/debauchee/barrier/releases/latest) of Barrier can be supported by this project.
>  - Anyone using an earlier version is advised to upgrade due to recently-addressed security vulnerabilities *(and other bug fixes)*. 
>    - This is especially important for computers accessible from the public Internet *(or from other shared/untrusted networks, such as when using shared WiFi)*.


**Q: How do I load my configuration on startup?**

> A: Start the binary with the argument `--config <path_to_saved_configuration>`


**Q: After loading my configuration on the client the field 'Server IP' is still empty!**

> A: Edit your configuration to include the server's ip address manually with
> 
>```
>(...)
>
>section: options
>    serverhostname=<AAA.BBB.CCC.DDD>
>```

**Q: Are there any other significant limitations with the current version of Barrier?**

> A: Currently:
>    - Barrier currently has limited UTF-8 support; issues have been reported with processing various languages.
>      - *(see [#860](https://github.com/debauchee/barrier/issues/860))*
>    - There is interest in future support for the Wayland compositor/display server protocol *([official site](https://wayland.freedesktop.org/) | [Wikipedia article](https://en.wikipedia.org/wiki/Wayland_(display_server_protocol)))* on Linux.
>      - As of late 2021, there is no expected completion date for *Wayland* support.
>      - *(see [#109](https://github.com/debauchee/barrier/issues/109) and [#1251](https://github.com/debauchee/barrier/issues/1251) for status or to volunteer your talents)*
>
> The complete list of open issues can be found in the ['Issues' tab on GitHub](https://github.com/debauchee/barrier/issues?q=is%3Aissue+is%3Aopen). Help is always appreciated.
