#!/bin/bash
# Setup script to install git hooks
# Usage: bash scripts/git-hooks/install-hooks.sh

HOOKS_DIR="$(git rev-parse --show-toplevel)/.git/hooks"
SCRIPT_DIR="$(git rev-parse --show-toplevel)/scripts/git-hooks"

cp "$SCRIPT_DIR/pre-commit" "$HOOKS_DIR/pre-commit"
chmod +x "$HOOKS_DIR/pre-commit"
echo "pre-commit hook installed."
