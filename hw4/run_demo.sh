#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"

SHM_NAME="/demo_queue"
TOTAL_BYTES="1048576"
MAX_PAYLOAD="256"

CONSUMER_TYPE="2"
CONSUMER_TARGET="10"

PRODUCER1_ID="1"
PRODUCER2_ID="2"
PRODUCER_MSG_COUNT="10"

mkdir -p "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

"$BUILD_DIR/cleanup_demo" "$SHM_NAME" || true

"$BUILD_DIR/producer_demo" "$SHM_NAME" "$TOTAL_BYTES" "$MAX_PAYLOAD" "$PRODUCER1_ID" "$PRODUCER_MSG_COUNT" &
P1=$!

sleep 0.3

"$BUILD_DIR/consumer_demo" "$SHM_NAME" "$TOTAL_BYTES" "$CONSUMER_TYPE" "$CONSUMER_TARGET" &
CONSUMER_PID=$!

sleep 0.1

"$BUILD_DIR/producer_demo" "$SHM_NAME" "$TOTAL_BYTES" "$MAX_PAYLOAD" "$PRODUCER2_ID" "$PRODUCER_MSG_COUNT" &
P2=$!

wait "$P1"
wait "$P2"
wait "$CONSUMER_PID"

"$BUILD_DIR/cleanup_demo" "$SHM_NAME" || true
