#!/bin/sh
#
# install git hooks (pre-commit / pre-push)
#

set -e

ROOT_DIR=$(git rev-parse --show-toplevel)
HOOK_SRC="$ROOT_DIR/scripts/git-hooks"
HOOK_DST="$ROOT_DIR/.git/hooks"

echo "[hooks] Installing git hooks..."

# ----------------------------
# 基础校验
# ----------------------------
if [ ! -d "$HOOK_DST" ]; then
    echo "[hooks] Error: .git/hooks not found"
    exit 1
fi

# ----------------------------
# 安装函数
# ----------------------------
install_hook() {
    name="$1"

    if [ ! -f "$HOOK_SRC/$name" ]; then
        echo "[hooks] Skip: $name (not found)"
        return
    fi

    echo "[hooks] Install: $name"
    cp "$HOOK_SRC/$name" "$HOOK_DST/$name"
    chmod +x "$HOOK_DST/$name"
}

install_hook pre-commit
install_hook pre-push

echo "[hooks] Done ✅"