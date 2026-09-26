#!/usr/bin/env bash
set -Eeuo pipefail

# BLOCK 1 — clone/update only. No Python, pip or builds.
PROJECT_BASE="${PROJECT_BASE:-${HOME}/project}"
REPO_DIR="${REPO_DIR:-${PROJECT_BASE}/ESP32-S3-4848S040}"
REPO_URL="${REPO_URL:-https://github.com/wpv10barza/ESP32-S3-4848S040.git}"
REPO_REF="${REPO_REF:-main}"

mkdir -p "${PROJECT_BASE}"

if [[ ! -d "${REPO_DIR}/.git" ]]; then
  [[ ! -e "${REPO_DIR}" ]] || { echo "[ERROR] ${REPO_DIR} exists and is not a Git repo."; exit 1; }
  git clone --branch "${REPO_REF}" --single-branch "${REPO_URL}" "${REPO_DIR}"
else
  cd "${REPO_DIR}"
  [[ "$(git remote get-url origin)" == "${REPO_URL}" ]] || { echo "[ERROR] origin mismatch."; exit 2; }
  git diff --quiet && git diff --cached --quiet || { echo "[ERROR] local changes detected."; exit 2; }
  git fetch --prune origin
  git switch "${REPO_REF}" >/dev/null 2>&1 || git switch -C "${REPO_REF}" "origin/${REPO_REF}"
  git reset --hard "origin/${REPO_REF}"
fi

cd "${REPO_DIR}"
printf '\n[OK] BLOCK 1\nRepo: %s\nBranch: %s\nCommit: %s\n' "$(git remote get-url origin)" "$(git branch --show-current)" "$(git rev-parse --short HEAD)"
