#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════
# rebuild.sh — Stop services, recompile C, restart
#
# Usage:
#   ./rebuild.sh              # do everything: C binaries + restart
#   ./rebuild.sh listener     # rebuild serial_listener only
#   ./rebuild.sh sender       # rebuild send_serial only
#   ./rebuild.sh -h           # help
# ═══════════════════════════════════════════════════════════════════════════

set -e

# ── Paths ──────────────────────────────────────────────────────────────────
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_DIR="/usr/local/bin"

LISTENER_SRC="${SRC_DIR}/serial_listener.c"
SENDER_SRC="${SRC_DIR}/send_serial.c"

LISTENER_BIN="serial_listener"
SENDER_BIN="send_serial"

LISTENER_SERVICE="serial-listener"

# ── Colours ────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

info()  { echo -e "${GREEN}[*]${NC} $1"; }
warn()  { echo -e "${YELLOW}[!]${NC} $1"; }
error() { echo -e "${RED}[x]${NC} $1" >&2; }

# ── Helpers ────────────────────────────────────────────────────────────────
usage() {
  grep '^#' "$0" | head -n 12 | sed 's/^# \{0,1\}//'
  exit 0
}

stop_service() {
  local svc="$1"
  if systemctl is-active --quiet "${svc}"; then
    info "Stopping ${svc}.service"
    sudo systemctl stop "${svc}"
  else
    warn "${svc} is not running — skipping stop"
  fi
}

start_service() {
  local svc="$1"
  info "Starting ${svc}.service"
  sudo systemctl start "${svc}"
  sleep 1
  if systemctl is-active --quiet "${svc}"; then
    info "${svc} is running"
  else
    error "${svc} failed to start — recent logs:"
    sudo journalctl -u "${svc}" -n 15 --no-pager
    exit 1
  fi
}

kill_stale_senders() {
  if pgrep -x "${SENDER_BIN}" > /dev/null; then
    warn "A ${SENDER_BIN} process is still running — killing it"
    sudo pkill -x "${SENDER_BIN}" || true
  fi
}

build_listener() {
  info "Compiling ${LISTENER_BIN}..."
  gcc -O2 -Wall -o "${SRC_DIR}/${LISTENER_BIN}" "${LISTENER_SRC}"
  info "Installing to ${INSTALL_DIR}/${LISTENER_BIN}"
  sudo cp "${SRC_DIR}/${LISTENER_BIN}" "${INSTALL_DIR}/${LISTENER_BIN}"
  sudo chmod +x "${INSTALL_DIR}/${LISTENER_BIN}"
}

build_sender() {
  info "Compiling ${SENDER_BIN}..."
  gcc -O2 -Wall -o "${SRC_DIR}/${SENDER_BIN}" "${SENDER_SRC}"
  info "Installing to ${INSTALL_DIR}/${SENDER_BIN}"
  sudo cp "${SRC_DIR}/${SENDER_BIN}" "${INSTALL_DIR}/${SENDER_BIN}"
  sudo chmod +x "${INSTALL_DIR}/${SENDER_BIN}"
}

# ── Main ───────────────────────────────────────────────────────────────────
case "${1:-all}" in
  -h|--help|help)
    usage
    ;;

  listener)
    stop_service "${LISTENER_SERVICE}"
    build_listener
    start_service "${LISTENER_SERVICE}"
    ;;

  sender)
    kill_stale_senders
    build_sender
    info "send_serial is invoked per-command by Klipper — no service to restart"
    ;;

  all|"")
    stop_service "${LISTENER_SERVICE}"
    kill_stale_senders
    build_listener
    build_sender
    start_service "${LISTENER_SERVICE}"
    ;;

  *)
    error "Unknown argument: $1"
    usage
    ;;
esac

info "Done."
