#!/bin/sh
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export HOME=/root
export TERM=vt100

echo "================================================================"
echo "[minimal_rootfs] Booting 64-bit BusyBox Userland on Zircon"
echo "================================================================"
uname -a
id

echo "[minimal_rootfs] Running /bin/zx_test..."
/bin/zx_test || echo "[minimal_rootfs] ERROR: zx_test failed (exit $?)"

echo "[minimal_rootfs] Running /bin/mm_test..."
/bin/mm_test
echo "[minimal_rootfs] mm_test exit code: $?"

echo "[minimal_rootfs] Running /bin/proc_test..."
/bin/proc_test
echo "[minimal_rootfs] proc_test exit code: $?"

PIPE_OUT=$(echo "BusyBox pipeline OK" | tr 'a-z' 'A-Z')
echo "[minimal_rootfs] Pipeline test: $PIPE_OUT"

echo "[MINIMAL_ROOTFS: SHELL_READY]"
exec /bin/sh
