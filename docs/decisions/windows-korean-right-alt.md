# Decision: treat Korean Windows Right Alt and Hangul as distinct sources

## Context

On Korean Windows keyboard layouts, the physical Right Alt key may arrive as
`Hangul` rather than the generic `Alt_R` event. Configurations that only name
one source are therefore not portable across layouts and keyboard paths.

## Decision

Keep `right_alt` and `hangul` available as separate source names, and allow
users to map either source explicitly. The expected behavior is documented in
the [live Windows/macOS baseline](../../doc/live-windows-mac-baseline.md).

## Consequences

- Korean Windows layouts can participate in the same cross-platform workflow.
- A configuration may intentionally define matching rules for both sources.
- Regression tests must distinguish the physical key from the semantic event
  reported by Windows.
