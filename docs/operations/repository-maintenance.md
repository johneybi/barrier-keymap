# Repository maintenance

## Before synchronizing `master`

The local branch may contain product work that is not on the remote default
branch. Refresh remote-tracking refs before deciding whether to merge or
rebase:

```bash
git fetch --all --prune
git status --short --branch
git log --oneline --left-right origin/master...master
```

Do not force-push `master` while it contains commits that have not been
reviewed against the refreshed remote history.

## Build outputs

Build directories such as `build-macos-keyboard/` and `out/` are local-only
artifacts. They are ignored by `.gitignore` and should not be committed.

Release archives and installers under `dist/` are a separate policy decision:
keep them only when they are intentionally distributed as repository assets;
otherwise publish them through GitHub Releases and remove them in a dedicated,
reviewed cleanup change.

## Branches and worktrees

Use descriptive branches for active work and preserve release snapshots with
explicit `archive/` names. Before deleting a branch, verify that its commits
are reachable from a release tag, another active branch, or the remote default
branch.

For a worktree whose checkout no longer exists, inspect first and then run:

```bash
git worktree prune --dry-run
git worktree prune
```

## Suggested routine

1. Fetch and prune remote-tracking refs.
2. Confirm the active branch and clean build artifacts.
3. Run the relevant build/tests.
4. Open a pull request with the issue number and verification notes.
5. Tag releases only from reviewed, reproducible commits.
