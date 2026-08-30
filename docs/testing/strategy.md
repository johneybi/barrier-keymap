# Testing strategy

KeyStitch changes should be verified at three levels:

1. **Configuration-level checks** — parse representative `section: remaps`
   rules, including direct remaps, tap/hold rules, modifier chords, and
   screen-specific rules.
2. **Build and automated checks** — run the platform-appropriate CMake build
   and test targets, plus the GitHub Actions CI workflow.
3. **Live cross-platform checks** — verify the resulting behavior between the
   actual Windows, macOS, and Linux machines involved in the scenario.

The live checks are especially important for modifier semantics, Korean
Windows `Right Alt`/`Hangul`, macOS extended keys, and input-source switching.
Record the observed baseline and regressions in the existing documents under
`doc/` rather than relying only on a commit message.

For releases, follow the [release checklist](../../doc/release-checklist.md)
and verify the generated archives and SHA256 files before publishing.
