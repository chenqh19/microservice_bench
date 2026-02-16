#!/usr/bin/env bash
# Sweep: for each request size (8KB–4MB), run wrk at RPS = 1GB/size .. 2GB/size, step 200MB/size.
# Run from compression/sweep/ (or set WRK/LUA_SCRIPT). Size is passed via REQUEST_SIZE (used by wrk_compress.lua).

sudo docker ps

set -e
# Exit whole script on Ctrl+C (SIGINT); otherwise only the current wrk run would stop
trap 'echo "Sweep interrupted."; exit 130' INT
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# From compression/sweep/: wrk2 at repo root = ../../wrk2; lua at compression/ = ../wrk_compress.lua
WRK="${WRK:-${SCRIPT_DIR}/../../wrk2/wrk}"
URL="${URL:-http://localhost:50060}"
DURATION="${DURATION:-20s}"
THREADS="${THREADS:-100}"
CONNECTIONS="${CONNECTIONS:-100}"
TASKSET="${TASKSET:-taskset -c 32-63}"

# 1GB, 2GB, 200MB in bytes
MB_400=$((400 * 1024 * 1024))
MB_2400=$((2400 * 1024 * 1024))
MB_200=$((200 * 1024 * 1024))

# Request sizes in bytes: 32KB, 64KB, 128KB, 256KB, 512KB, 1MB, 2MB
SIZES=(32768 65536 131072 262144 524288 1048576 2097152)

LUA_SCRIPT="${SCRIPT_DIR}/../wrk_compress.lua"
OUT_DIR="${SCRIPT_DIR}/data"
mkdir -p "$OUT_DIR"

for size in "${SIZES[@]}"; do
  export REQUEST_SIZE="$size"
  rps_min=$((MB_400 / size))
  rps_max=$((MB_2400 / size))
  rps_step=$((MB_200 / size))
  [ "$rps_step" -lt 1 ] && rps_step=1

  rps=$rps_min
  while [ "$rps" -le "$rps_max" ]; do
    echo "=== size=${size} rps=${rps} (range ${rps_min}-${rps_max} step ${rps_step}) ==="
    $TASKSET "$WRK" -t "$THREADS" -c "$CONNECTIONS" -D exp -d "$DURATION" -L \
      -s "$LUA_SCRIPT" "$URL" -R "$rps" || true
    sudo docker logs compression-compress_frontend-1 --tail 4 > "${OUT_DIR}/lat_${size}_${rps}" 2>&1 || true
    rps=$((rps + rps_step))
    sleep 2
  done
done

echo "Sweep done."
