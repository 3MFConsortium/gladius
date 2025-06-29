# Git Hooks

This directory contains shared git hooks for the project.

## Installing Hooks

After cloning the repository, run:

```bash
bash scripts/git-hooks/install-hooks.sh
```

This will copy the `pre-commit` hook to your local `.git/hooks` directory and make it executable.

## Note
Git hooks are not tracked by git. Each contributor must run the install script after cloning or pulling updates to ensure hooks are installed locally.
