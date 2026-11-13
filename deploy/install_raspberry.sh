#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Installation script for Afore Autonomous EMS on Raspberry Pi (Linux Host)

set -euo pipefail

echo "=========================================================="
echo "    Afore Autonomous EMS - Raspberry Pi Installer         "
echo "=========================================================="

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: Please run this script with sudo (e.g. sudo bash deploy/install_raspberry.sh)"
    exit 1
fi

TARGET_USER="${SUDO_USER:-pi}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "==> 1. Installing system dependencies..."
apt-get update
apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    python3 \
    libpthread-stubs0-dev

echo "==> 2. Granting user '${TARGET_USER}' access to serial ports..."
usermod -a -G dialout "${TARGET_USER}"

echo "==> 3. Building read-only shadow daemon..."
BUILD_DIR="${REPO_ROOT}/build-rpi"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
cmake -DCMAKE_BUILD_TYPE=Release -DAFORE_SHADOW_BUILD=ON "${REPO_ROOT}"
make -j"$(nproc)" heliograph-daemon

echo "==> 4. Deploying binaries and configuration..."
mkdir -p /opt/afore-ems
mkdir -p /etc/afore-ems
mkdir -p /var/log/afore-ems

cp "${BUILD_DIR}/heliograph-daemon" /opt/afore-ems/heliograph-daemon
chmod +x /opt/afore-ems/heliograph-daemon

cp "${REPO_ROOT}/deploy/web_ui.py" /opt/afore-ems/web_ui.py
chmod +x /opt/afore-ems/web_ui.py
mkdir -p /opt/afore-ems/src/web/assets
cp -r "${REPO_ROOT}/src/web/assets"/* /opt/afore-ems/src/web/assets/

if [ ! -f /etc/afore-ems/config.json ]; then
    cp "${REPO_ROOT}/deploy/config.json.example" /etc/afore-ems/config.json
    echo "Created default /etc/afore-ems/config.json"
else
    echo "Preserved existing /etc/afore-ems/config.json"
fi

chown -R "${TARGET_USER}:${TARGET_USER}" /var/log/afore-ems

echo "==> 5. Installing systemd service..."
sed -i "s/User=pi/User=${TARGET_USER}/" "${REPO_ROOT}/deploy/afore-shadow.service"
cp "${REPO_ROOT}/deploy/afore-shadow.service" /etc/systemd/system/afore-shadow.service
systemctl daemon-reload

echo ""
echo "=========================================================="
echo "Installation complete!"
echo ""
echo "To start the service and web UI now:"
echo "    sudo systemctl enable --now afore-shadow.service"
echo ""
echo "Web interface will be available at:"
echo "    http://<RASPBERRY-IP>:8080/       (Live Dashboard)"
echo "    http://<RASPBERRY-IP>:8080/setup  (Setup Wizard)"
echo ""
echo "To inspect live logs:"
echo "    sudo journalctl -u afore-shadow.service -f"
echo "=========================================================="
