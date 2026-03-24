#!/usr/bin/env bash

# Remote Qt cross-build helper for this repo.
#
# Workflow:
#   1. Edit source code on the Mac worktree.
#   2. Run this script from the repo root on the Mac.
#   3. The script rsyncs the selected project to the remote Linux builder.
#   4. The builder sources the NXP SDK environment, then runs qmake + make.
#   5. The built artifact is optionally copied to the board.
#
# Source-of-truth rule:
#   - The Mac worktree is the intended source of truth.
#   - The builder directory is a disposable build mirror.
#   - Each run uses `rsync --delete`, so builder files are forced to match the Mac.
#   - If you edit files directly on the builder or board, those changes do NOT
#     sync back to the Mac automatically and may be overwritten on the next run.
#
# Common commands:
#   ./scripts/remote_qt_build.sh My_Project --skip-deploy
#   ./scripts/remote_qt_build.sh Camera_Project
#   ./scripts/remote_qt_build.sh all
#   ./scripts/remote_qt_build.sh Camera_Project --deploy-dir /tmp

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILDER="${BUILDER:-ladykaka@10.20.41.20}"
BOARD="${BOARD:-root@10.20.20.36}"
BUILDER_BASE="${BUILDER_BASE:-/home/ladykaka/qt_build_from_mac}"
SDK_ENV="${SDK_ENV:-/opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi}"
DEPLOY_DIR="${DEPLOY_DIR:-/lib/modules/4.1.15-g3dc0a4b}"
JOBS="${JOBS:-4}"
SKIP_DEPLOY=0

usage() {
    cat <<'EOF'
Usage:
  scripts/remote_qt_build.sh <Camera_Project|My_Project|all> [--skip-deploy]
  scripts/remote_qt_build.sh <project> [--builder user@host] [--board user@host]
                             [--builder-base /path/on/builder]
                             [--sdk-env /path/to/environment-setup-*]
                             [--deploy-dir /path/on/board] [--jobs N]

Workflow:
  - Run this script on the Mac worktree after editing local source files.
  - The Mac worktree is the source of truth.
  - The remote builder directory is only a synced build mirror.
  - Each run uses rsync --delete, so remote builder files are overwritten to
    match the current Mac state.
  - Direct edits made on the builder or board do not sync back automatically.

Examples:
  ./scripts/remote_qt_build.sh My_Project --skip-deploy
  ./scripts/remote_qt_build.sh Camera_Project
  ./scripts/remote_qt_build.sh all
  ./scripts/remote_qt_build.sh Camera_Project --deploy-dir /tmp

Environment overrides:
  BUILDER, BOARD, BUILDER_BASE, SDK_ENV, DEPLOY_DIR, JOBS
EOF
}

die() {
    echo "[ERROR] $*" >&2
    exit 1
}

log() {
    echo "[INFO] $*"
}

PROJECT_ARG=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        Camera_Project|My_Project|all)
            [[ -z "$PROJECT_ARG" ]] || die "project already set to '$PROJECT_ARG'"
            PROJECT_ARG="$1"
            shift
            ;;
        --skip-deploy)
            SKIP_DEPLOY=1
            shift
            ;;
        --builder)
            BUILDER="$2"
            shift 2
            ;;
        --board)
            BOARD="$2"
            shift 2
            ;;
        --builder-base)
            BUILDER_BASE="$2"
            shift 2
            ;;
        --sdk-env)
            SDK_ENV="$2"
            shift 2
            ;;
        --deploy-dir)
            DEPLOY_DIR="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown argument: $1"
            ;;
    esac
done

[[ -n "$PROJECT_ARG" ]] || {
    usage
    exit 1
}

if [[ "$PROJECT_ARG" == "all" ]]; then
    PROJECTS=(Camera_Project My_Project)
else
    PROJECTS=("$PROJECT_ARG")
fi

for project in "${PROJECTS[@]}"; do
    local_dir="$REPO_ROOT/$project"
    remote_dir="$BUILDER_BASE/$project"
    artifact="$remote_dir/$project"

    [[ -d "$local_dir" ]] || die "local project dir not found: $local_dir"
    [[ -f "$local_dir/$project.pro" ]] || die "missing project file: $local_dir/$project.pro"

    log "prepare builder dir for $project"
    ssh "$BUILDER" "mkdir -p '$remote_dir'"

    log "sync $project to $BUILDER:$remote_dir"
    rsync -av --delete "$local_dir/" "$BUILDER:$remote_dir/"

    log "build $project on builder"
    ssh "$BUILDER" "bash -lc 'set -eo pipefail; source \"$SDK_ENV\" >/dev/null 2>&1; cd \"$remote_dir\"; rm -f Makefile .qmake.stash \"$project\"; qmake; make -j\"$JOBS\"'"

    if [[ "$SKIP_DEPLOY" -eq 1 ]]; then
        log "skip deploy for $project"
        continue
    fi

    log "check board deploy dir on $BOARD"
    ssh "$BOARD" "test -d '$DEPLOY_DIR'"

    log "deploy $project to $BOARD:$DEPLOY_DIR"
    scp -3 "$BUILDER:$artifact" "$BOARD:$DEPLOY_DIR/"
done

log "done"
