# Global rules

## Git commits

Never run `git commit` (or any command that creates a commit — including via automated workflows/skills that normally commit after each step) unless the user has explicitly asked for a commit in that specific instance. This applies even mid-task: if a skill or plan's default process is to commit after each step, pause that step instead of committing, keep the work as uncommitted changes in the working tree, and continue without committing until the user asks.

A prior "commit this" does not carry forward to later work — ask or wait for a fresh explicit request each time.