#!/bin/bash
# Start Docker-in-Docker with cgroup v1 nesting fix.
# Usage: sudo bash start-dind.sh
set -e

# 1. Load iptables kernel modules (required by dockerd networking)
modprobe ip_tables iptable_nat iptable_filter 2>/dev/null || true

# 2. Fix cgroup v1 nesting for DinD
#    In a privileged container on cgroup v1, each subsystem shows the full
#    host hierarchy. Inner Docker's runc expects the container's own cgroup
#    as root. We bind-mount each subsystem to the container's own cgroup dir.
if [ ! -f /sys/fs/cgroup/cgroup.controllers ]; then
    SELF_CGROUP_ID=$(grep ':memory:' /proc/1/cgroup | cut -d: -f3 | sed 's|^/docker/||')
    if [ -n "$SELF_CGROUP_ID" ]; then
        for subsys_dir in /sys/fs/cgroup/*/; do
            subsys_name=$(basename "$subsys_dir")
            [ -L "/sys/fs/cgroup/$subsys_name" ] && continue
            our_dir="$subsys_dir/docker/$SELF_CGROUP_ID"
            if [ -d "$our_dir" ]; then
                mount --bind "$our_dir" "$subsys_dir" 2>/dev/null || true
            fi
        done
    fi
fi

# 3. Start Docker daemon via the DinD init script
/usr/local/share/docker-init.sh
sleep 2

# 4. Ensure non-root users can access the socket
chmod 666 /var/run/docker.sock 2>/dev/null || true
