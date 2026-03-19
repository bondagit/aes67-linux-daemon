#!/usr/bin/env bash
# Requires bash 4+ (macOS: brew install bash)
if [[ "${BASH_VERSINFO[0]}" -lt 4 ]]; then
    # Try Homebrew bash on macOS
    for candidate in /opt/homebrew/bin/bash /usr/local/bin/bash; do
        if [[ -x "$candidate" ]] && "$candidate" --version 2>/dev/null | head -1 | grep -q 'version [4-9]'; then
            exec "$candidate" "$0" "$@"
        fi
    done
    echo "ERROR: bash 4+ is required (for associative arrays)."
    echo "  macOS: brew install bash"
    exit 1
fi

set -euo pipefail

##############################################################################
# AES67 Linux Daemon - Full Automatic Deployment Script
#
# Deploys to any Linux machine (tested on Raspberry Pi 5 CM / Raspbian)
# via SSH. Interactive wizard for configuration, saves responses for re-use.
#
# Usage:
#   ./deploy.sh [saved-config-file]
#
# Examples:
#   ./deploy.sh                          # Fresh interactive wizard
#   ./deploy.sh my-pi.deploy.conf        # Re-use saved config (Enter to accept)
##############################################################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAVED_CONFIG="${1:-}"
DEFAULT_CONFIG_FILE="last-deploy.conf"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

banner() {
    echo -e "${CYAN}"
    echo "============================================================"
    echo "  AES67 Linux Daemon - Remote Deployment"
    echo "============================================================"
    echo -e "${NC}"
}

info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }
step()  { echo -e "\n${BOLD}${CYAN}>>> $*${NC}"; }

##############################################################################
# Load saved config (provides defaults for the wizard)
##############################################################################
declare -A SAVED=()

load_saved_config() {
    local file="$1"
    if [[ -f "$file" ]]; then
        info "Loading saved configuration from: $file"
        while IFS='=' read -r key value; do
            [[ "$key" =~ ^#.*$ || -z "$key" ]] && continue
            SAVED["$key"]="$value"
        done < "$file"
    fi
}

if [[ -n "$SAVED_CONFIG" && -f "$SAVED_CONFIG" ]]; then
    load_saved_config "$SAVED_CONFIG"
elif [[ -f "${SCRIPT_DIR}/${DEFAULT_CONFIG_FILE}" ]]; then
    load_saved_config "${SCRIPT_DIR}/${DEFAULT_CONFIG_FILE}"
fi

##############################################################################
# Interactive prompt helper — shows default, accepts Enter for default
##############################################################################
ask() {
    local var_name="$1"
    local prompt_text="$2"
    local default="${3:-}"
    local saved_val="${SAVED[$var_name]:-}"
    local effective_default="${saved_val:-$default}"
    local result

    if [[ -n "$effective_default" ]]; then
        echo -en "  ${BOLD}${prompt_text}${NC} [${GREEN}${effective_default}${NC}]: "
    else
        echo -en "  ${BOLD}${prompt_text}${NC}: "
    fi
    read -r result
    result="${result:-$effective_default}"
    eval "$var_name=\"$result\""
}

ask_password() {
    local var_name="$1"
    local prompt_text="$2"
    local saved_val="${SAVED[$var_name]:-}"
    local result

    if [[ -n "$saved_val" ]]; then
        echo -en "  ${BOLD}${prompt_text}${NC} [${GREEN}(saved)${NC}]: "
    else
        echo -en "  ${BOLD}${prompt_text}${NC}: "
    fi
    read -rs result
    echo
    result="${result:-$saved_val}"
    eval "$var_name=\"$result\""
}

ask_yesno() {
    local var_name="$1"
    local prompt_text="$2"
    local default="${3:-y}"
    local saved_val="${SAVED[$var_name]:-}"
    local effective_default="${saved_val:-$default}"
    local result

    echo -en "  ${BOLD}${prompt_text}${NC} [${GREEN}${effective_default}${NC}]: "
    read -r result
    result="${result:-$effective_default}"
    result=$(echo "$result" | tr '[:upper:]' '[:lower:]')
    [[ "$result" == "y" || "$result" == "yes" ]] && result="y" || result="n"
    eval "$var_name=\"$result\""
}

##############################################################################
# WIZARD
##############################################################################
banner

step "Step 1/9: SSH Connection"
echo
echo -e "  ${YELLOW}Uses your normal SSH config (~/.ssh/config, agent, keys, etc.)${NC}"
echo -e "  ${YELLOW}Make sure you can: ssh user@host  before running this.${NC}"
echo

ask TARGET_HOST   "Remote hostname or IP"          ""
ask TARGET_USER   "SSH username"                    "pi"
ask SSH_PORT      "SSH port"                        "22"

# Build SSH command — relies on user's normal SSH agent/config/keys
SSH_OPTS="-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -p ${SSH_PORT}"
REMOTE="${TARGET_USER}@${TARGET_HOST}"

run_remote() {
    ssh ${SSH_OPTS} "${REMOTE}" "$@"
}

step "Step 2/9: Build Options"
echo

ask_yesno USE_CLANG      "Use clang compiler (recommended for ARM)" "y"
ask_yesno WITH_AVAHI     "Enable mDNS/Avahi support"                "y"
ask_yesno WITH_STREAMER  "Enable HTTP audio streamer"               "y"
ask_yesno WITH_SYSTEMD   "Enable systemd integration"               "y"
ask_yesno INSTALL_KERNEL_MODULE "Install kernel module persistently (depmod)" "y"
ask BUILD_DIR "Remote build directory" "/home/${TARGET_USER}/aes67-linux-daemon"

step "Step 3/9: Network & Audio Configuration"
echo

echo -e "  ${YELLOW}Tip: Common interfaces: eth0, end0 (RPi5), enp0s31f6, etc.${NC}"
echo -e "  ${YELLOW}     You can check with 'ip link show' on the target.${NC}"
ask IFACE_NAME     "Network interface for AES67"   "eth0"
ask HTTP_BIND_ADDR "HTTP bind address (0.0.0.0 = all interfaces)" "0.0.0.0"
ask HTTP_PORT      "HTTP/Web UI port"              "8080"
ask RTSP_PORT      "RTSP port"                     "8854"
ask SAMPLE_RATE    "Sample rate (44100/48000/96000/192000/384000)" "48000"
ask PTP_DOMAIN     "PTP domain (0-127)"            "0"
ask RTP_MCAST_BASE "RTP multicast base address"    "239.1.0.1"
ask TIC_FRAME_SIZE "Tic frame size at 1fs (48=1ms)" "48"

step "Step 4/9: Streamer Configuration"
echo

if [[ "$WITH_STREAMER" == "y" ]]; then
    ask_yesno STREAMER_ENABLED  "Enable streamer at startup"    "n"
    ask STREAMER_CHANNELS       "Streamer channels"             "8"
else
    STREAMER_ENABLED="n"
    STREAMER_CHANNELS="8"
    info "Streamer disabled, skipping streamer config."
fi

step "Step 5/9: Shairport-Sync (AirPlay Receiver)"
echo
echo -e "  ${YELLOW}Shairport-sync receives AirPlay audio and outputs to ALSA.${NC}"
echo -e "  ${YELLOW}It can feed audio into the RAVENNA device for AES67 streaming.${NC}"
echo

ask_yesno WITH_SHAIRPORT   "Install shairport-sync (AirPlay receiver)" "y"

SHAIRPORT_NAME="${SHAIRPORT_NAME:-AES67 Bridge}"
SHAIRPORT_OUTPUT="${SHAIRPORT_OUTPUT:-plughw:RAVENNA}"
SHAIRPORT_AIRPLAY2="${SHAIRPORT_AIRPLAY2:-n}"
if [[ "$WITH_SHAIRPORT" == "y" ]]; then
    ask SHAIRPORT_NAME     "AirPlay device name"                       "$SHAIRPORT_NAME"
    ask SHAIRPORT_OUTPUT   "ALSA output device"                        "$SHAIRPORT_OUTPUT"
    ask_yesno SHAIRPORT_AIRPLAY2 "Enable AirPlay 2 support"           "$SHAIRPORT_AIRPLAY2"
    if [[ "$SHAIRPORT_AIRPLAY2" == "y" ]]; then
        echo -e "  ${RED}WARNING: AirPlay 2 requires NQPTP which conflicts with ptp4l (PTP ports 319/320).${NC}"
        echo -e "  ${RED}         AES67 PTP grandmaster will NOT work with AirPlay 2 enabled.${NC}"
        echo -e "  ${RED}         Recommend: use AirPlay 1 (answer 'n' above) for AES67 setups.${NC}"
        ask_yesno SHAIRPORT_AIRPLAY2 "Still enable AirPlay 2?" "n"
    fi
fi

step "Step 6/9: Raspberry Pi Hardware"
echo
echo -e "  ${YELLOW}Enable I2S audio output overlay in /boot/config.txt (or firmware/config.txt).${NC}"
echo -e "  ${YELLOW}Common DAC overlays: hifiberry-dac, hifiberry-dacplus, googlevoicehat-soundcard, etc.${NC}"
echo

ask_yesno ENABLE_I2S       "Enable I2S audio output on RPi"           "y"
I2S_OVERLAY="${I2S_OVERLAY:-hifiberry-dac}"
if [[ "$ENABLE_I2S" == "y" ]]; then
    ask I2S_OVERLAY        "I2S device tree overlay"                   "$I2S_OVERLAY"
fi

step "Step 7/9: JACK Audio Server & Routing"
echo
echo -e "  ${YELLOW}JACK provides low-latency audio routing between all devices.${NC}"
echo -e "  ${YELLOW}It sits on hw:RAVENNA (AES67) and can bridge additional ALSA devices${NC}"
echo -e "  ${YELLOW}(I2S DAC, USB audio, etc.) for a full multi-IO routing matrix.${NC}"
echo

ask_yesno WITH_JACK        "Enable JACK audio server"                  "y"
JACK_PERIOD_SIZE="${JACK_PERIOD_SIZE:-${TIC_FRAME_SIZE}}"
JACK_NPERIODS="${JACK_NPERIODS:-2}"
JACK_RAVENNA_DEVICE="${JACK_RAVENNA_DEVICE:-hw:RAVENNA}"
JACK_RAVENNA_IN="${JACK_RAVENNA_IN:-8}"
JACK_RAVENNA_OUT="${JACK_RAVENNA_OUT:-8}"
JACK_EXTRA_IO="${JACK_EXTRA_IO:-}"
JACK_AUTOCONNECT="${JACK_AUTOCONNECT:-y}"
if [[ "$WITH_JACK" == "y" ]]; then
    echo
    echo -e "  ${YELLOW}JACK uses a dummy (timing-only) backend. All ALSA devices${NC}"
    echo -e "  ${YELLOW}(RAVENNA, I2S, USB) are bridged in via alsa_in/alsa_out.${NC}"
    echo -e "  ${YELLOW}Period size should match TIC frame size (48=1ms @ 48kHz).${NC}"
    echo
    ask JACK_PERIOD_SIZE    "JACK period size (frames, match TIC)"     "$JACK_PERIOD_SIZE"
    ask JACK_NPERIODS       "JACK number of periods (2=low lat, 3=safe)" "$JACK_NPERIODS"
    echo
    echo -e "  ${YELLOW}RAVENNA bridge channels (AES67 network I/O in JACK):${NC}"
    echo
    ask JACK_RAVENNA_DEVICE "RAVENNA ALSA device"                      "$JACK_RAVENNA_DEVICE"
    ask JACK_RAVENNA_IN     "RAVENNA capture channels (from network)"  "$JACK_RAVENNA_IN"
    ask JACK_RAVENNA_OUT    "RAVENNA playback channels (to network)"   "$JACK_RAVENNA_OUT"
    echo
    echo -e "  ${YELLOW}Scanning remote ALSA devices (excluding RAVENNA)...${NC}"
    echo

    # Auto-detect ALSA playback devices via aplay -l, capture via arecord -l
    DETECTED_IO=""
    DETECTED_DISPLAY=""
    while IFS= read -r line; do
        # Parse lines like: "card 1: sndrpihifiberry [snd_rpi_hifiberry_dac], device 0: ..."
        if [[ "$line" =~ ^card\ ([0-9]+):\ ([a-zA-Z0-9_]+)\ \[(.+)\],\ device\ ([0-9]+): ]]; then
            CARD_NUM="${BASH_REMATCH[1]}"
            CARD_ID="${BASH_REMATCH[2]}"
            CARD_NAME="${BASH_REMATCH[3]}"
            DEV_NUM="${BASH_REMATCH[4]}"

            # Skip RAVENNA — it's bridged separately
            [[ "$CARD_ID" == "RAVENNA" || "$CARD_NAME" == *"RAVENNA"* || "$CARD_NAME" == *"Merging"* ]] && continue
            # Skip Loopback devices
            [[ "$CARD_ID" == "Loopback" || "$CARD_NAME" == *"Loopback"* ]] && continue

            HW_DEV="hw:${CARD_ID},${DEV_NUM}"
            # Sanitize name for JACK client: lowercase, replace spaces/special chars
            JACK_NAME=$(echo "${CARD_ID}" | tr '[:upper:]' '[:lower:]' | tr -cs '[:alnum:]' '-' | sed 's/-$//')

            # Count playback channels
            OUT_CH=0
            PLAY_INFO=$(run_remote "aplay -D '${HW_DEV}' --dump-hw-params /dev/null 2>&1" 2>/dev/null || true)
            if echo "$PLAY_INFO" | grep -q "CHANNELS"; then
                OUT_CH=$(echo "$PLAY_INFO" | grep "^CHANNELS" | head -1 | grep -oP '\d+' | tail -1)
                [[ -z "$OUT_CH" ]] && OUT_CH=0
            fi

            # Count capture channels
            IN_CH=0
            REC_INFO=$(run_remote "arecord -D '${HW_DEV}' --dump-hw-params /dev/null 2>&1" 2>/dev/null || true)
            if echo "$REC_INFO" | grep -q "CHANNELS"; then
                IN_CH=$(echo "$REC_INFO" | grep "^CHANNELS" | head -1 | grep -oP '\d+' | tail -1)
                [[ -z "$IN_CH" ]] && IN_CH=0
            fi

            # Skip if no channels at all
            [[ "$IN_CH" -eq 0 && "$OUT_CH" -eq 0 ]] && continue

            ENTRY="${JACK_NAME}:${HW_DEV}:${IN_CH}:${OUT_CH}"
            if [[ -n "$DETECTED_IO" ]]; then
                DETECTED_IO="${DETECTED_IO} ${ENTRY}"
            else
                DETECTED_IO="${ENTRY}"
            fi
            echo -e "    ${GREEN}Found:${NC} ${CARD_NAME} (${HW_DEV}) — ${IN_CH} in / ${OUT_CH} out"
        fi
    done < <(run_remote "aplay -l 2>/dev/null; arecord -l 2>/dev/null" 2>/dev/null | sort -u)

    if [[ -n "$DETECTED_IO" ]]; then
        echo
        echo -e "  ${GREEN}Auto-detected devices: ${DETECTED_IO}${NC}"
        # Use detected as default, but let user override
        JACK_EXTRA_IO="${JACK_EXTRA_IO:-$DETECTED_IO}"
    else
        echo -e "  ${YELLOW}No additional ALSA devices found (RAVENNA only).${NC}"
    fi

    echo
    echo -e "  ${YELLOW}Format: name:device:in_ch:out_ch separated by spaces${NC}"
    echo -e "  ${YELLOW}Edit the auto-detected list or leave empty for RAVENNA only.${NC}"
    echo
    ask JACK_EXTRA_IO       "Extra ALSA devices"                       "$JACK_EXTRA_IO"
    ask_yesno JACK_AUTOCONNECT "Auto-connect shairport to RAVENNA outputs" "$JACK_AUTOCONNECT"
fi

step "Step 8/9: System Tuning"
echo

ask_yesno DISABLE_PULSE    "Disable PulseAudio/PipeWire on target"     "y"
ask_yesno APPLY_SYSCTL     "Apply recommended sysctl tuning"           "y"
ask_yesno ENABLE_SERVICE   "Enable daemon to start on boot"            "y"
ask_yesno START_AFTER_DEPLOY "Start daemon after deployment"           "y"

step "Step 9/9: Low-Latency Audio Tuning"
echo
echo -e "  ${YELLOW}Tunes the system for real-time / low-latency audio processing.${NC}"
echo -e "  ${YELLOW}On Raspberry Pi: modifies cmdline.txt and config.txt.${NC}"
echo -e "  ${YELLOW}On all systems: sets CPU governor, IRQ tuning, sysctl tweaks.${NC}"
echo

ask_yesno APPLY_RT_TUNING   "Apply low-latency / real-time tuning"     "y"
if [[ "$APPLY_RT_TUNING" == "y" ]]; then
    ask_yesno RT_CPU_PERFORMANCE "Set CPU governor to 'performance'"    "y"
    ask_yesno RT_DISABLE_BT      "Disable Bluetooth on RPi (reduces jitter)" "y"
    ask_yesno RT_DISABLE_WIFI    "Disable WiFi on RPi (if using Ethernet only)" "n"
    ask_yesno RT_FORCE_TURBO     "Disable CPU frequency scaling on RPi (force_turbo)" "y"
    ask_yesno RT_ISOLCPU         "Isolate a CPU core for audio (isolcpus)" "n"
    if [[ "$RT_ISOLCPU" == "y" ]]; then
        ask RT_ISOLCPU_CORE "CPU core to isolate (e.g. 3 on RPi5)" "3"
    else
        RT_ISOLCPU_CORE=""
    fi
fi

##############################################################################
# Save configuration
##############################################################################
save_config() {
    local file="$1"
    {
        echo "# AES67 Daemon Deploy Config — saved $(date -Iseconds)"
        echo "# Re-run: ./deploy.sh ${file}"
        local key
        for key in TARGET_HOST TARGET_USER SSH_PORT \
                   USE_CLANG WITH_AVAHI WITH_STREAMER WITH_SYSTEMD \
                   INSTALL_KERNEL_MODULE BUILD_DIR \
                   IFACE_NAME HTTP_BIND_ADDR HTTP_PORT RTSP_PORT SAMPLE_RATE \
                   PTP_DOMAIN RTP_MCAST_BASE TIC_FRAME_SIZE \
                   STREAMER_ENABLED STREAMER_CHANNELS \
                   WITH_SHAIRPORT SHAIRPORT_NAME SHAIRPORT_OUTPUT SHAIRPORT_AIRPLAY2 \
                   ENABLE_I2S I2S_OVERLAY \
                   WITH_JACK JACK_RAVENNA_DEVICE JACK_RAVENNA_IN JACK_RAVENNA_OUT \
                   JACK_PERIOD_SIZE JACK_NPERIODS JACK_EXTRA_IO JACK_AUTOCONNECT \
                   DISABLE_PULSE APPLY_SYSCTL ENABLE_SERVICE START_AFTER_DEPLOY \
                   APPLY_RT_TUNING RT_CPU_PERFORMANCE RT_DISABLE_BT RT_DISABLE_WIFI \
                   RT_FORCE_TURBO RT_ISOLCPU RT_ISOLCPU_CORE; do
            echo "${key}=${!key}"
        done
    } > "$file"
    info "Configuration saved to: ${file}"
}

SAVE_PATH="${SCRIPT_DIR}/${DEFAULT_CONFIG_FILE}"
ask SAVE_PATH "Save config to" "$SAVE_PATH"
save_config "$SAVE_PATH"

##############################################################################
# Confirmation
##############################################################################
echo
echo -e "${BOLD}${CYAN}=== Deployment Summary ===${NC}"
echo -e "  Target:     ${BOLD}${TARGET_USER}@${TARGET_HOST}:${SSH_PORT}${NC}"
echo -e "  Build dir:  ${BUILD_DIR}"
echo -e "  Interface:  ${IFACE_NAME}"
echo -e "  HTTP port:  ${HTTP_PORT}"
echo -e "  Sample rate: ${SAMPLE_RATE} Hz"
echo -e "  PTP domain: ${PTP_DOMAIN}"
echo -e "  Compiler:   $([ "$USE_CLANG" == "y" ] && echo "clang" || echo "gcc")"
echo -e "  Avahi:      ${WITH_AVAHI}"
echo -e "  Streamer:   ${WITH_STREAMER}"
echo -e "  Systemd:    ${WITH_SYSTEMD}"
echo -e "  Shairport:  ${WITH_SHAIRPORT}$([ "$WITH_SHAIRPORT" == "y" ] && echo " (\"${SHAIRPORT_NAME}\" -> ${SHAIRPORT_OUTPUT})")"
echo -e "  I2S overlay: ${ENABLE_I2S}$([ "$ENABLE_I2S" == "y" ] && echo " (${I2S_OVERLAY})")"
echo -e "  JACK:       ${WITH_JACK:-n}$([ "${WITH_JACK:-n}" == "y" ] && echo " (dummy+bridges p=${JACK_PERIOD_SIZE} n=${JACK_NPERIODS} ravenna=${JACK_RAVENNA_IN}in/${JACK_RAVENNA_OUT}out$([ -n "${JACK_EXTRA_IO}" ] && echo " +extra"))")"
echo -e "  RT tuning:  ${APPLY_RT_TUNING:-n}$([ "${RT_ISOLCPU:-n}" == "y" ] && echo " (isolcpus=${RT_ISOLCPU_CORE})")"
echo
echo -en "  ${BOLD}Proceed with deployment? [Y/n]:${NC} "
read -r CONFIRM
CONFIRM="${CONFIRM:-y}"
if [[ "${CONFIRM,,}" != "y" ]]; then
    info "Deployment cancelled."
    exit 0
fi

##############################################################################
# DEPLOYMENT
##############################################################################

step "Verifying SSH connection..."
if ! run_remote "echo 'SSH OK'" 2>/dev/null; then
    error "Cannot connect to ${REMOTE} on port ${SSH_PORT}"
    exit 1
fi
info "SSH connection verified."

step "Installing system dependencies on remote..."
run_remote "bash -s" <<'REMOTE_DEPS'
set -e
export DEBIAN_FRONTEND=noninteractive

echo ">>> Updating package lists..."
sudo apt-get update -qq

PACKAGES=(
    psmisc
    build-essential
    cmake
    libboost-dev
    libboost-thread-dev
    libboost-filesystem-dev
    libboost-log-dev
    libboost-program-options-dev
    alsa-utils
    libasound2-dev
    linuxptp
    libsystemd-dev
    wget
    nodejs
    npm
)

echo ">>> Installing base packages..."
sudo apt-get install -y --no-upgrade "${PACKAGES[@]}"

# linux-headers — try multiple package names (varies by distro)
echo ">>> Installing kernel headers..."
sudo apt-get install -y --no-upgrade -qq "linux-headers-$(uname -r)" 2>/dev/null \
    || sudo apt-get install -y --no-upgrade -qq raspberrypi-kernel-headers 2>/dev/null \
    || echo "WARNING: Could not install kernel headers automatically. You may need to install them manually."

REMOTE_DEPS

# Conditional packages
if [[ "$USE_CLANG" == "y" ]]; then
    run_remote "sudo apt-get install -y --no-upgrade clang"
fi
if [[ "$WITH_AVAHI" == "y" ]]; then
    run_remote "sudo apt-get install -y --no-upgrade libavahi-client-dev"
fi
if [[ "$WITH_STREAMER" == "y" ]]; then
    run_remote "sudo apt-get install -y --no-upgrade libfaac-dev"
fi
if [[ "$WITH_SHAIRPORT" == "y" ]]; then
    run_remote "sudo apt-get install -y --no-upgrade git"
fi
if [[ "${WITH_JACK:-n}" == "y" ]]; then
    run_remote "sudo apt-get install -y --no-upgrade jackd2 libjack-jackd2-dev aj-snapshot jack-tools zita-ajbridge"
fi

info "Dependencies installed."

step "Preparing local source (submodules)..."
(cd "${SCRIPT_DIR}" && git submodule update --init --recursive 2>/dev/null || true)
info "Local submodules ready."

step "Syncing source code to remote..."

# Use rsync if available, fall back to tar+scp
if command -v rsync &>/dev/null; then
    rsync -az --delete \
        --exclude='.git' \
        --exclude='webui/node_modules' \
        --exclude='last-deploy.conf' \
        -e "ssh ${SSH_OPTS}" \
        "${SCRIPT_DIR}/" "${REMOTE}:${BUILD_DIR}/"
    info "Source synced via rsync."
else
    warn "rsync not found, using tar+scp (slower)..."
    TMPTAR=$(mktemp /tmp/aes67-deploy-XXXXXX.tar.gz)
    tar -czf "$TMPTAR" \
        --exclude='.git' \
        --exclude='webui/node_modules' \
        --exclude='last-deploy.conf' \
        -C "${SCRIPT_DIR}" .
    run_remote "mkdir -p ${BUILD_DIR}"
    scp -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -P "${SSH_PORT}" "${TMPTAR}" "${REMOTE}:${BUILD_DIR}/deploy.tar.gz"
    run_remote "cd ${BUILD_DIR} && tar -xzf deploy.tar.gz && rm deploy.tar.gz"
    rm -f "$TMPTAR"
    info "Source synced via tar+scp."
fi

step "Stopping existing services..."
run_remote "bash -s" <<'REMOTE_STOP'
set -e

# Stop services in reverse dependency order
for svc in shairport-sync aj-snapshot jack-autoconnect jackd jack-alsa-bridges aes67-daemon ptp4l-aes67 nqptp; do
    if sudo systemctl is-active "$svc" &>/dev/null; then
        echo ">>> Stopping $svc..."
        sudo systemctl stop "$svc"
    fi
done

# Kill any manually started ptp4l instances
sudo killall ptp4l 2>/dev/null || true

# Unload kernel module (must happen after daemon stops)
if lsmod | grep -q MergingRavennaALSA; then
    echo ">>> Unloading kernel module..."
    sudo rmmod MergingRavennaALSA 2>/dev/null || true
fi

echo ">>> All services stopped."
REMOTE_STOP
info "Services stopped."

step "Building kernel module..."
run_remote "bash -s" <<REMOTE_KM
set -e
cd ${BUILD_DIR}/3rdparty/ravenna-alsa-lkm/driver

# Always rebuild — source may have changed
echo ">>> Cleaning previous build..."
make clean 2>/dev/null || true
echo ">>> Building MergingRavennaALSA.ko..."
make
echo ">>> Kernel module built successfully."
REMOTE_KM
info "Kernel module ready."

step "Building WebUI..."
run_remote "bash -s" <<REMOTE_WEBUI
set -e
cd ${BUILD_DIR}/webui

echo ">>> Installing WebUI dependencies..."
npm install --no-audit --no-fund 2>&1 | tail -3

echo ">>> Building WebUI..."
npm run build

if [ -d dist ] && [ -f dist/index.html ]; then
    echo ">>> WebUI built successfully."
else
    echo "ERROR: WebUI build failed — dist/index.html not found."
    exit 1
fi
REMOTE_WEBUI
info "WebUI ready."

step "Building AES67 daemon..."

# Prepare cmake flags
CMAKE_FLAGS="-DBoost_NO_WARN_NEW_VERSIONS=1"
CMAKE_FLAGS+=" -DCPP_HTTPLIB_DIR=${BUILD_DIR}/3rdparty/cpp-httplib"
CMAKE_FLAGS+=" -DRAVENNA_ALSA_LKM_DIR=${BUILD_DIR}/3rdparty/ravenna-alsa-lkm"
CMAKE_FLAGS+=" -DENABLE_TESTS=OFF"
CMAKE_FLAGS+=" -DFAKE_DRIVER=OFF"
CMAKE_FLAGS+=" -DWITH_AVAHI=$( [ "$WITH_AVAHI" == "y" ] && echo "ON" || echo "OFF" )"
CMAKE_FLAGS+=" -DWITH_SYSTEMD=$( [ "$WITH_SYSTEMD" == "y" ] && echo "ON" || echo "OFF" )"
CMAKE_FLAGS+=" -DWITH_STREAMER=$( [ "$WITH_STREAMER" == "y" ] && echo "ON" || echo "OFF" )"

COMPILER_ENV=""
if [[ "$USE_CLANG" == "y" ]]; then
    COMPILER_ENV="export CC=/usr/bin/clang; export CXX=/usr/bin/clang++;"
fi

run_remote "bash -s" <<REMOTE_BUILD
set -e
${COMPILER_ENV}
cd ${BUILD_DIR}/daemon
if [ -f aes67-daemon ] && [ -f CMakeCache.txt ]; then
    echo ">>> Previous build found, running incremental build..."
    make -j\$(nproc)
    echo ">>> Incremental build done."
else
    echo ">>> Running cmake..."
    cmake ${CMAKE_FLAGS} .
    echo ">>> Building daemon (this may take a while on ARM)..."
    make -j\$(nproc)
    echo ">>> Daemon built successfully."
fi
REMOTE_BUILD
info "Daemon compiled."

step "Installing and loading kernel module..."
run_remote "bash -s" <<REMOTE_KM_INSTALL
set -e
cd ${BUILD_DIR}/3rdparty/ravenna-alsa-lkm/driver

# Module was already unloaded in the stop-services step

if [ "${INSTALL_KERNEL_MODULE}" == "y" ]; then
    echo ">>> Installing kernel module persistently..."
    sudo make modules_install 2>/dev/null || sudo cp MergingRavennaALSA.ko /lib/modules/\$(uname -r)/extra/
    sudo depmod -a
    echo "MergingRavennaALSA" | sudo tee /etc/modules-load.d/aes67.conf > /dev/null
    echo ">>> Loading kernel module..."
    sudo modprobe MergingRavennaALSA
    echo ">>> Module installed and loaded (will also load on boot)."
else
    echo ">>> Loading kernel module (non-persistent)..."
    sudo insmod MergingRavennaALSA.ko
fi

# Verify it's loaded
if lsmod | grep -q MergingRavennaALSA; then
    echo ">>> Kernel module verified: loaded."
else
    echo "ERROR: Kernel module failed to load!"
    exit 1
fi
REMOTE_KM_INSTALL
info "Kernel module installed and loaded."

step "Installing daemon and WebUI..."

# Generate daemon.conf
STREAMER_ENABLED_BOOL="false"
[[ "$STREAMER_ENABLED" == "y" ]] && STREAMER_ENABLED_BOOL="true"

DAEMON_CONF=$(cat <<DAEMONCONF
{
  "http_port": ${HTTP_PORT},
  "rtsp_port": ${RTSP_PORT},
  "http_base_dir": "/usr/local/share/aes67-daemon/webui/",
  "log_severity": 2,
  "playout_delay": 0,
  "tic_frame_size_at_1fs": ${TIC_FRAME_SIZE},
  "max_tic_frame_size": 1024,
  "sample_rate": ${SAMPLE_RATE},
  "rtp_mcast_base": "${RTP_MCAST_BASE}",
  "rtp_port": 5004,
  "ptp_domain": ${PTP_DOMAIN},
  "ptp_dscp": 48,
  "sap_mcast_addr": "239.255.255.255",
  "sap_interval": 30,
  "syslog_proto": "none",
  "syslog_server": "255.255.255.254:1234",
  "status_file": "/etc/status.json",
  "interface_name": "${IFACE_NAME}",
  "mdns_enabled": true,
  "custom_node_id": "",
  "ptp_status_script": "/usr/local/share/aes67-daemon/scripts/ptp_status.sh",
  "streamer_channels": ${STREAMER_CHANNELS},
  "streamer_files_num": 8,
  "streamer_file_duration": 1,
  "streamer_player_buffer_files_num": 1,
  "streamer_enabled": ${STREAMER_ENABLED_BOOL},
  "auto_sinks_update": true
}
DAEMONCONF
)

run_remote "bash -s" <<REMOTE_INSTALL
set -e
cd ${BUILD_DIR}

echo ">>> Stopping running daemon (if any)..."
sudo systemctl stop aes67-daemon 2>/dev/null || true

echo ">>> Creating aes67-daemon user..."
sudo useradd -g audio -M -l aes67-daemon -c "AES67 Linux daemon" 2>/dev/null || true

echo ">>> Installing daemon binary..."
sudo cp daemon/aes67-daemon /usr/local/bin/aes67-daemon

echo ">>> Creating directories..."
sudo install -d -o aes67-daemon /var/lib/aes67-daemon \
    /usr/local/share/aes67-daemon/scripts/ \
    /usr/local/share/aes67-daemon/webui/

echo ">>> Installing PTP status script..."
sudo install -o aes67-daemon daemon/scripts/ptp_status.sh /usr/local/share/aes67-daemon/scripts/

echo ">>> Installing WebUI..."
if [ -d webui/dist ]; then
    sudo cp -r webui/dist/* /usr/local/share/aes67-daemon/webui/
fi

echo ">>> Writing daemon configuration..."
cat > /tmp/daemon.conf <<'INNERCONF'
${DAEMON_CONF}
INNERCONF
sudo install -o aes67-daemon -m 644 /tmp/daemon.conf /etc/daemon.conf
rm /tmp/daemon.conf

echo ">>> Writing initial status file..."
if [ ! -f /etc/status.json ]; then
    echo '{"sources": [], "sinks": []}' | sudo tee /etc/status.json > /dev/null
    sudo chown aes67-daemon /etc/status.json
fi

echo ">>> Daemon installed."
REMOTE_INSTALL
info "Daemon and WebUI installed."

if [[ "$WITH_SYSTEMD" == "y" ]]; then
    step "Installing ptp4l service (PTP grandmaster)..."
    run_remote "bash -s" <<REMOTE_PTP4L
set -e

echo ">>> Creating ptp4l systemd service for AES67..."
sudo tee /etc/systemd/system/ptp4l-aes67.service > /dev/null <<PTPSVC
[Unit]
Description=PTP4L Grandmaster Clock for AES67
After=network.target aes67-ptp-loopback.service
Wants=aes67-ptp-loopback.service
Before=aes67-daemon.service

[Service]
Type=simple
ExecStart=/usr/sbin/ptp4l -i ${IFACE_NAME} -l6 -E -H --step_threshold=0.00002 --first_step_threshold=0.00002 -m
Restart=on-failure
RestartSec=2
$([ "${RT_ISOLCPU:-n}" = "y" ] && [ -n "${RT_ISOLCPU_CORE}" ] && echo "CPUAffinity=${RT_ISOLCPU_CORE}")

[Install]
WantedBy=multi-user.target
PTPSVC

sudo systemctl daemon-reload
sudo systemctl enable ptp4l-aes67
sudo systemctl restart ptp4l-aes67

sleep 1
if sudo systemctl is-active ptp4l-aes67 &>/dev/null; then
    echo ">>> ptp4l is running as PTP grandmaster on ${IFACE_NAME}."
else
    echo "WARNING: ptp4l failed to start. Check: sudo journalctl -u ptp4l-aes67 -n 30"
fi
REMOTE_PTP4L
    info "ptp4l grandmaster service installed."

    step "Installing AES67 daemon systemd service..."
    run_remote "bash -s" <<REMOTE_SYSTEMD
set -e
cd ${BUILD_DIR}

echo ">>> Installing service file..."
sudo cp systemd/aes67-daemon.service /etc/systemd/system/aes67-daemon.service

# Add dependency on ptp4l
echo ">>> Adding ptp4l dependency..."
sudo mkdir -p /etc/systemd/system/aes67-daemon.service.d
sudo tee /etc/systemd/system/aes67-daemon.service.d/override.conf > /dev/null <<OVERRIDE
[Unit]
After=network.target systemd-modules-load.service ptp4l-aes67.service
Requires=systemd-modules-load.service
Wants=ptp4l-aes67.service

[Service]
ExecStart=
ExecStart=/usr/local/bin/aes67-daemon$([ -n "${HTTP_BIND_ADDR}" ] && echo " -a ${HTTP_BIND_ADDR}")
$([ "${RT_ISOLCPU:-n}" = "y" ] && [ -n "${RT_ISOLCPU_CORE}" ] && echo "CPUAffinity=${RT_ISOLCPU_CORE}")
OVERRIDE

sudo systemctl daemon-reload

if [ "${ENABLE_SERVICE}" == "y" ]; then
    echo ">>> Enabling service for auto-start..."
    sudo systemctl enable aes67-daemon
fi

echo ">>> Systemd service installed."
REMOTE_SYSTEMD
    info "Systemd service installed."
fi

if [[ "$DISABLE_PULSE" == "y" ]]; then
    step "Disabling PulseAudio/PipeWire..."
    run_remote "bash -s" <<'REMOTE_PULSE'
set -e
# PulseAudio
if systemctl --user is-active pulseaudio.service &>/dev/null 2>&1; then
    systemctl --user stop pulseaudio.service 2>/dev/null || true
    systemctl --user disable pulseaudio.service 2>/dev/null || true
    systemctl --user mask pulseaudio.service 2>/dev/null || true
fi
if systemctl --user is-active pulseaudio.socket &>/dev/null 2>&1; then
    systemctl --user stop pulseaudio.socket 2>/dev/null || true
    systemctl --user disable pulseaudio.socket 2>/dev/null || true
    systemctl --user mask pulseaudio.socket 2>/dev/null || true
fi
# PipeWire
if systemctl --user is-active pipewire.service &>/dev/null 2>&1; then
    systemctl --user stop pipewire.service 2>/dev/null || true
    systemctl --user disable pipewire.service 2>/dev/null || true
    systemctl --user mask pipewire.service 2>/dev/null || true
fi
if systemctl --user is-active pipewire-pulse.service &>/dev/null 2>&1; then
    systemctl --user stop pipewire-pulse.service 2>/dev/null || true
    systemctl --user disable pipewire-pulse.service 2>/dev/null || true
    systemctl --user mask pipewire-pulse.service 2>/dev/null || true
fi
echo ">>> PulseAudio/PipeWire disabled."
REMOTE_PULSE
    info "PulseAudio/PipeWire disabled."
fi

if [[ "$APPLY_SYSCTL" == "y" ]]; then
    step "Applying sysctl tuning..."
    run_remote "bash -s" <<'REMOTE_SYSCTL'
set -e
SYSCTL_CONF="/etc/sysctl.d/99-aes67.conf"
sudo tee "$SYSCTL_CONF" > /dev/null <<SYSCTLEOF
# AES67 daemon performance tuning
net.ipv4.igmp_max_memberships=66
kernel.perf_cpu_time_max_percent=0
kernel.sched_rt_runtime_us=1000000

# Low-latency audio tuning
vm.swappiness=10
net.core.rmem_max=16777216
net.core.wmem_max=16777216
net.core.rmem_default=1048576
net.core.wmem_default=1048576
net.core.netdev_max_backlog=5000
kernel.timer_migration=0
SYSCTLEOF
sudo sysctl -p "$SYSCTL_CONF"
echo ">>> Sysctl tuning applied."
REMOTE_SYSCTL
    info "Sysctl tuning applied."
fi

if [[ "$ENABLE_I2S" == "y" ]]; then
    step "Enabling I2S audio overlay (${I2S_OVERLAY})..."
    run_remote "bash -s" <<REMOTE_I2S
set -e

# Find the config.txt location (varies by OS version)
if [ -f /boot/firmware/config.txt ]; then
    CONFIG_TXT="/boot/firmware/config.txt"
elif [ -f /boot/config.txt ]; then
    CONFIG_TXT="/boot/config.txt"
else
    echo "WARNING: Could not find config.txt — skipping I2S setup."
    exit 0
fi

echo ">>> Using \${CONFIG_TXT}"

# Disable onboard audio if not already
if grep -q "^dtparam=audio=on" "\${CONFIG_TXT}"; then
    echo ">>> Disabling onboard audio..."
    sudo sed -i 's/^dtparam=audio=on/dtparam=audio=off/' "\${CONFIG_TXT}"
elif ! grep -q "^dtparam=audio=" "\${CONFIG_TXT}"; then
    echo "dtparam=audio=off" | sudo tee -a "\${CONFIG_TXT}" > /dev/null
fi

# Add I2S overlay if not already present
if grep -q "^dtoverlay=${I2S_OVERLAY}" "\${CONFIG_TXT}"; then
    echo ">>> I2S overlay '${I2S_OVERLAY}' already configured."
else
    echo ">>> Adding I2S overlay '${I2S_OVERLAY}'..."
    echo "dtoverlay=${I2S_OVERLAY}" | sudo tee -a "\${CONFIG_TXT}" > /dev/null
    echo ">>> I2S overlay added. A reboot is needed to activate it."
fi

echo ">>> I2S configuration done."
REMOTE_I2S
    info "I2S overlay configured."
fi

if [[ "${APPLY_RT_TUNING:-n}" == "y" ]]; then
    step "Applying low-latency / real-time tuning..."

    # --- RPi cmdline.txt and config.txt tuning ---
    run_remote "bash -s" <<REMOTE_RT_RPI
set -e

# Detect Raspberry Pi
IS_RPI="n"
if [ -f /proc/device-tree/model ] && grep -qi "raspberry" /proc/device-tree/model 2>/dev/null; then
    IS_RPI="y"
    echo ">>> Raspberry Pi detected: \$(cat /proc/device-tree/model | tr -d '\0')"
elif [ -f /sys/firmware/devicetree/base/model ] && grep -qi "raspberry" /sys/firmware/devicetree/base/model 2>/dev/null; then
    IS_RPI="y"
    echo ">>> Raspberry Pi detected: \$(cat /sys/firmware/devicetree/base/model | tr -d '\0')"
fi

if [ "\${IS_RPI}" = "y" ]; then
    # --- cmdline.txt tuning ---
    if [ -f /boot/firmware/cmdline.txt ]; then
        CMDLINE_TXT="/boot/firmware/cmdline.txt"
    elif [ -f /boot/cmdline.txt ]; then
        CMDLINE_TXT="/boot/cmdline.txt"
    else
        echo "WARNING: Could not find cmdline.txt — skipping kernel boot param tuning."
        CMDLINE_TXT=""
    fi

    if [ -n "\${CMDLINE_TXT}" ]; then
        echo ">>> Tuning \${CMDLINE_TXT} for low-latency audio..."
        sudo cp "\${CMDLINE_TXT}" "\${CMDLINE_TXT}.bak.\$(date +%Y%m%d%H%M%S)"

        CMDLINE=\$(cat "\${CMDLINE_TXT}" | tr '\n' ' ' | sed 's/[[:space:]]*$//')

        # Add preempt=full if not present
        if ! echo "\${CMDLINE}" | grep -q "preempt="; then
            CMDLINE="\${CMDLINE} preempt=full"
            echo ">>>   Added: preempt=full"
        else
            echo ">>>   preempt= already set, not modifying."
        fi

        # Add threadirqs if not present
        if ! echo "\${CMDLINE}" | grep -q "threadirqs"; then
            CMDLINE="\${CMDLINE} threadirqs"
            echo ">>>   Added: threadirqs"
        fi

        # Add skew_tick=1 to spread timer interrupts
        if ! echo "\${CMDLINE}" | grep -q "skew_tick="; then
            CMDLINE="\${CMDLINE} skew_tick=1"
            echo ">>>   Added: skew_tick=1"
        fi

        # Isolate CPU core if requested
        if [ "${RT_ISOLCPU}" = "y" ] && [ -n "${RT_ISOLCPU_CORE}" ]; then
            if ! echo "\${CMDLINE}" | grep -q "isolcpus="; then
                CMDLINE="\${CMDLINE} isolcpus=${RT_ISOLCPU_CORE}"
                echo ">>>   Added: isolcpus=${RT_ISOLCPU_CORE}"
            fi
            if ! echo "\${CMDLINE}" | grep -q "nohz_full="; then
                CMDLINE="\${CMDLINE} nohz_full=${RT_ISOLCPU_CORE}"
                echo ">>>   Added: nohz_full=${RT_ISOLCPU_CORE}"
            fi
            if ! echo "\${CMDLINE}" | grep -q "rcu_nocbs="; then
                CMDLINE="\${CMDLINE} rcu_nocbs=${RT_ISOLCPU_CORE}"
                echo ">>>   Added: rcu_nocbs=${RT_ISOLCPU_CORE}"
            fi
        fi

        echo "\${CMDLINE}" | sudo tee "\${CMDLINE_TXT}" > /dev/null
        echo ">>> cmdline.txt updated (reboot required to take effect)."
    fi

    # --- config.txt tuning ---
    if [ -f /boot/firmware/config.txt ]; then
        CONFIG_TXT="/boot/firmware/config.txt"
    elif [ -f /boot/config.txt ]; then
        CONFIG_TXT="/boot/config.txt"
    else
        echo "WARNING: Could not find config.txt — skipping RPi config tuning."
        CONFIG_TXT=""
    fi

    if [ -n "\${CONFIG_TXT}" ]; then
        echo ">>> Tuning \${CONFIG_TXT} for low-latency audio..."
        sudo cp "\${CONFIG_TXT}" "\${CONFIG_TXT}.bak.\$(date +%Y%m%d%H%M%S)"

        # force_turbo — disables dynamic frequency scaling
        if [ "${RT_FORCE_TURBO}" = "y" ]; then
            if grep -q "^force_turbo=" "\${CONFIG_TXT}"; then
                sudo sed -i 's/^force_turbo=.*/force_turbo=1/' "\${CONFIG_TXT}"
            else
                echo "force_turbo=1" | sudo tee -a "\${CONFIG_TXT}" > /dev/null
            fi
            echo ">>>   Set: force_turbo=1"
        fi

        # arm_boost — boost ARM clock
        if grep -q "^arm_boost=" "\${CONFIG_TXT}"; then
            sudo sed -i 's/^arm_boost=.*/arm_boost=1/' "\${CONFIG_TXT}"
        elif ! grep -q "arm_boost" "\${CONFIG_TXT}"; then
            echo "arm_boost=1" | sudo tee -a "\${CONFIG_TXT}" > /dev/null
        fi
        echo ">>>   Set: arm_boost=1"

        # Disable Bluetooth to reduce USB/UART jitter
        if [ "${RT_DISABLE_BT}" = "y" ]; then
            if ! grep -q "^dtoverlay=disable-bt" "\${CONFIG_TXT}"; then
                echo "dtoverlay=disable-bt" | sudo tee -a "\${CONFIG_TXT}" > /dev/null
                echo ">>>   Added: dtoverlay=disable-bt"
            fi
            # Also disable hciuart service
            sudo systemctl disable hciuart 2>/dev/null || true
            sudo systemctl stop hciuart 2>/dev/null || true
        fi

        # Disable WiFi to reduce interrupts/jitter
        if [ "${RT_DISABLE_WIFI}" = "y" ]; then
            if ! grep -q "^dtoverlay=disable-wifi" "\${CONFIG_TXT}"; then
                echo "dtoverlay=disable-wifi" | sudo tee -a "\${CONFIG_TXT}" > /dev/null
                echo ">>>   Added: dtoverlay=disable-wifi"
            fi
        fi

        echo ">>> config.txt updated (reboot required to take effect)."
    fi
else
    echo ">>> Not a Raspberry Pi — skipping cmdline.txt/config.txt tuning."
fi
REMOTE_RT_RPI
    info "RPi boot configuration tuned."

    # --- CPU governor and general RT tuning (all platforms) ---
    if [[ "${RT_CPU_PERFORMANCE:-n}" == "y" ]]; then
        step "Setting CPU governor to 'performance'..."
        run_remote "bash -s" <<'REMOTE_RT_CPUGOV'
set -e

# Install cpufrequtils if available
sudo apt-get install -y --no-upgrade -qq cpufrequtils 2>/dev/null || true

# Set governor immediately on all CPUs
for cpu_gov in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    if [ -f "$cpu_gov" ]; then
        echo "performance" | sudo tee "$cpu_gov" > /dev/null
    fi
done
echo ">>> CPU governor set to 'performance' on all cores."

# Make it persist across reboots
if command -v cpufreq-set &>/dev/null; then
    echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils > /dev/null
    echo ">>> Persisted via /etc/default/cpufrequtils."
fi

# Also create a systemd service for extra persistence (some boards ignore cpufrequtils)
sudo tee /etc/systemd/system/cpu-performance.service > /dev/null <<'CPUSVC'
[Unit]
Description=Set CPU Governor to Performance
After=multi-user.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/bash -c 'for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > "$g" 2>/dev/null || true; done'

[Install]
WantedBy=multi-user.target
CPUSVC
sudo systemctl daemon-reload
sudo systemctl enable cpu-performance
echo ">>> cpu-performance.service installed for boot persistence."
REMOTE_RT_CPUGOV
        info "CPU governor set to performance."
    fi

    # --- Disable unnecessary services for lower latency ---
    step "Disabling unnecessary services for low latency..."
    run_remote "bash -s" <<'REMOTE_RT_SERVICES'
set -e

# Disable services that cause scheduling jitter
for svc in triggerhappy ModemManager bluetooth; do
    if systemctl list-unit-files "${svc}.service" &>/dev/null 2>&1; then
        sudo systemctl disable "${svc}" 2>/dev/null || true
        sudo systemctl stop "${svc}" 2>/dev/null || true
        echo ">>>   Disabled: ${svc}"
    fi
done

# Set audio group limits for realtime scheduling
if ! grep -q "^@audio" /etc/security/limits.d/audio.conf 2>/dev/null; then
    sudo tee /etc/security/limits.d/audio.conf > /dev/null <<'LIMITSEOF'
# Real-time audio scheduling limits
@audio   -  rtprio     99
@audio   -  memlock    unlimited
@audio   -  nice       -20
LIMITSEOF
    echo ">>> Real-time scheduling limits set for @audio group."
fi

echo ">>> Low-latency service tuning done."
REMOTE_RT_SERVICES
    info "Unnecessary services disabled, RT limits configured."
fi

step "Configuring network interface (${IFACE_NAME})..."
run_remote "bash -s" <<REMOTE_NET
set -e

IFACE="${IFACE_NAME}"

# Bring interface up
echo ">>> Ensuring \${IFACE} is up..."
sudo ip link set "\${IFACE}" up

# Wait for link to come up (carrier detect)
echo ">>> Waiting for link on \${IFACE}..."
for i in \$(seq 1 15); do
    STATE=\$(cat /sys/class/net/\${IFACE}/operstate 2>/dev/null || echo "unknown")
    if [ "\${STATE}" = "up" ]; then
        echo ">>> Link is up."
        break
    fi
    if [ "\${i}" -eq 15 ]; then
        echo "WARNING: \${IFACE} operstate is '\${STATE}' after 15s — continuing anyway (cable plugged in?)"
    fi
    sleep 1
done

# Show current IP
IP=\$(ip -4 addr show "\${IFACE}" 2>/dev/null | grep -oP 'inet \K[0-9.]+' | head -1 || true)
if [ -n "\${IP}" ]; then
    echo ">>> \${IFACE} has IP: \${IP}"
else
    echo "WARNING: \${IFACE} has no IPv4 address. AES67 needs an IP on this interface."
    echo "         Configure one manually or via DHCP."
fi

# Enable multicast on the interface
echo ">>> Enabling multicast on \${IFACE}..."
sudo ip link set "\${IFACE}" multicast on

# Add multicast route if not present
if ! ip route show | grep -q "239.0.0.0/8.*dev \${IFACE}"; then
    echo ">>> Adding multicast route for \${IFACE}..."
    sudo ip route add 239.0.0.0/8 dev "\${IFACE}" 2>/dev/null || true
fi

# Create persistent PTP loopback service: uses tc to mirror locally-generated
# PTP packets (from ptp4l) back to the same interface's ingress, so the
# RAVENNA kernel module (which hooks NF_INET_PRE_ROUTING) can see them.
echo ">>> Installing PTP loopback service..."
sudo tee /usr/local/bin/aes67-ptp-loopback.sh > /dev/null <<'LOOPSCRIPT'
#!/bin/bash
# Mirror outgoing PTP packets back to ingress for local grandmaster support
IFACE="\$1"
if [ -z "\$IFACE" ]; then
    echo "Usage: \$0 <interface>"
    exit 1
fi
tc qdisc del dev "\$IFACE" ingress 2>/dev/null || true
tc qdisc add dev "\$IFACE" ingress
tc qdisc del dev "\$IFACE" root 2>/dev/null || true
tc qdisc add dev "\$IFACE" root handle 1: prio
tc filter add dev "\$IFACE" parent 1: protocol ip u32 \
    match ip protocol 17 0xff \
    match ip dport 319 0xffff \
    action mirred ingress mirror dev "\$IFACE"
tc filter add dev "\$IFACE" parent 1: protocol ip u32 \
    match ip protocol 17 0xff \
    match ip dport 320 0xffff \
    action mirred ingress mirror dev "\$IFACE"
echo "PTP loopback active on \$IFACE"
LOOPSCRIPT
sudo chmod +x /usr/local/bin/aes67-ptp-loopback.sh

sudo tee /etc/systemd/system/aes67-ptp-loopback.service > /dev/null <<LOOPSVC
[Unit]
Description=PTP Packet Loopback for AES67 (tc mirror)
After=network.target
Before=ptp4l-aes67.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/local/bin/aes67-ptp-loopback.sh ${IFACE_NAME}
ExecStop=/sbin/tc qdisc del dev ${IFACE_NAME} root 2>/dev/null; /sbin/tc qdisc del dev ${IFACE_NAME} ingress 2>/dev/null

[Install]
WantedBy=multi-user.target
LOOPSVC

sudo systemctl daemon-reload
sudo systemctl enable aes67-ptp-loopback
sudo systemctl restart aes67-ptp-loopback
echo ">>> PTP loopback service installed and started."

echo ">>> Network interface ready."
REMOTE_NET
info "Network interface configured."

if [[ "$START_AFTER_DEPLOY" == "y" && "$WITH_SYSTEMD" == "y" ]]; then
    step "Starting ptp4l grandmaster..."
    run_remote "sudo systemctl restart ptp4l-aes67" || true
    sleep 2
    STATUS=$(run_remote "sudo systemctl is-active ptp4l-aes67" 2>/dev/null || echo "unknown")
    if [[ "$STATUS" == "active" ]]; then
        info "ptp4l is running as grandmaster."
    else
        warn "ptp4l status: ${STATUS}"
        warn "Check logs with: ssh ${REMOTE} 'sudo journalctl -u ptp4l-aes67 -n 50'"
    fi

    step "Starting AES67 daemon..."
    run_remote "sudo systemctl restart aes67-daemon" || true
    sleep 2
    STATUS=$(run_remote "sudo systemctl is-active aes67-daemon" 2>/dev/null || echo "unknown")
    if [[ "$STATUS" == "active" ]]; then
        info "Daemon is running!"
    else
        warn "Daemon status: ${STATUS}"
        warn "Check logs with: ssh ${REMOTE} 'sudo journalctl -u aes67-daemon -n 50'"
    fi

    # Give PTP time to lock
    step "Waiting for PTP lock..."
    for i in $(seq 1 15); do
        PTP_STATUS=$(run_remote "curl -s http://localhost:${HTTP_PORT}/api/ptp/status" 2>/dev/null || echo '{}')
        if echo "$PTP_STATUS" | grep -q '"locked"'; then
            info "PTP locked! $PTP_STATUS"
            break
        fi
        if [[ $i -eq 15 ]]; then
            warn "PTP did not lock within 15s: $PTP_STATUS"
            warn "This may take longer — check the web UI."
        fi
        sleep 1
    done
fi

if [[ "$WITH_SHAIRPORT" == "y" ]]; then
    step "Installing shairport-sync dependencies..."

    SHAIRPORT_PKGS="autoconf automake libtool libpopt-dev libconfig-dev"
    SHAIRPORT_PKGS+=" libasound2-dev avahi-daemon libavahi-client-dev"
    SHAIRPORT_PKGS+=" libssl-dev libsoxr-dev"
    if [[ "$SHAIRPORT_AIRPLAY2" == "y" ]]; then
        SHAIRPORT_PKGS+=" libplist-dev libsodium-dev uuid-dev libgcrypt-dev xxd"
        SHAIRPORT_PKGS+=" libavutil-dev libavcodec-dev libavformat-dev libplist-utils"
    fi
    run_remote "sudo apt-get install -y --no-upgrade ${SHAIRPORT_PKGS}"
    info "Shairport-sync dependencies installed."

    if [[ "$SHAIRPORT_AIRPLAY2" == "y" ]]; then
        step "Building NQPTP (required for AirPlay 2)..."
        run_remote "bash -s" <<'REMOTE_NQPTP'
set -e
if [ -f /usr/local/bin/nqptp ]; then
    echo ">>> NQPTP binary already installed, skipping build."
else
    echo ">>> Cloning NQPTP..."
    cd /tmp
    rm -rf nqptp
    git clone https://github.com/mikebrady/nqptp.git
    cd nqptp
    autoreconf -fi
    ./configure
    make
    sudo make install
    echo ">>> NQPTP built and installed."
fi

# Always ensure service file exists and is running
echo ">>> Ensuring NQPTP systemd service..."
printf '%s\n' \
  "[Unit]" \
  "Description=NQPTP - Not Quite PTP" \
  "After=network.target" \
  "" \
  "[Service]" \
  "Type=simple" \
  "ExecStart=/usr/local/bin/nqptp" \
  "Restart=on-failure" \
  "" \
  "[Install]" \
  "WantedBy=multi-user.target" \
  | sudo tee /etc/systemd/system/nqptp.service > /dev/null
sudo systemctl daemon-reload
sudo systemctl enable nqptp
sudo systemctl start nqptp
echo ">>> NQPTP service running."
REMOTE_NQPTP
        info "NQPTP ready."
    fi

    step "Building shairport-sync..."
    SHAIRPORT_CONFIGURE="--sysconfdir=/etc --with-alsa --with-soxr --with-avahi --with-ssl=openssl --with-systemd"
    if [[ "${WITH_JACK:-n}" == "y" ]]; then
        SHAIRPORT_CONFIGURE+=" --with-jack"
    fi
    if [[ "$SHAIRPORT_AIRPLAY2" == "y" ]]; then
        SHAIRPORT_CONFIGURE+=" --with-airplay-2"
    fi

    run_remote "bash -s" <<REMOTE_SHAIRPORT_BUILD
set -e
SHAIRPORT_DIR="/home/${TARGET_USER}/shairport-sync"
SHAIRPORT_CONF_MARKER="/usr/local/share/.shairport-build-config"
SHAIRPORT_WANT="${SHAIRPORT_CONFIGURE}"
SHAIRPORT_HAVE=""
[ -f "\${SHAIRPORT_CONF_MARKER}" ] && SHAIRPORT_HAVE=\$(cat "\${SHAIRPORT_CONF_MARKER}")
if [ -f /usr/local/bin/shairport-sync ] && [ "\${SHAIRPORT_WANT}" = "\${SHAIRPORT_HAVE}" ]; then
    echo ">>> shairport-sync already built with matching config, skipping."
else
    [ -f /usr/local/bin/shairport-sync ] && echo ">>> Config changed, rebuilding shairport-sync..."
    echo ">>> Cloning shairport-sync..."
    rm -rf "\${SHAIRPORT_DIR}"
    git clone https://github.com/mikebrady/shairport-sync.git "\${SHAIRPORT_DIR}"
    cd "\${SHAIRPORT_DIR}"
    autoreconf -fi
    echo ">>> Configuring with: ${SHAIRPORT_CONFIGURE}"
    ./configure ${SHAIRPORT_CONFIGURE}
    echo ">>> Building shairport-sync (this may take a while on ARM)..."
    make -j\$(nproc)
    sudo make install
    echo "${SHAIRPORT_CONFIGURE}" | sudo tee "\${SHAIRPORT_CONF_MARKER}" > /dev/null
    echo ">>> shairport-sync built and installed."
fi
REMOTE_SHAIRPORT_BUILD
    info "Shairport-sync compiled."

    step "Configuring shairport-sync..."

    # Build config and service files locally, then send them
    if [[ "${WITH_JACK:-n}" == "y" ]]; then
        SHAIRPORT_CONF_CONTENT="general =
{
  name = \"${SHAIRPORT_NAME}\";
  output_backend = \"jack\";
  mdns_backend = \"avahi\";
  interpolation = \"soxr\";
};

jack =
{
  client_name = \"shairport-sync\";
  auto_client_disconnect = \"no\";
  soxr_resample_quality = \"very high\";
};"
    else
        SHAIRPORT_CONF_CONTENT="general =
{
  name = \"${SHAIRPORT_NAME}\";
  output_backend = \"alsa\";
  mdns_backend = \"avahi\";
  interpolation = \"soxr\";
};

alsa =
{
  output_device = \"${SHAIRPORT_OUTPUT}\";
  output_rate = ${SAMPLE_RATE};
  output_format = \"S16_LE\";
};"
    fi

    SP_AFTER="sound.target network-online.target avahi-daemon.service"
    SP_WANTS="avahi-daemon.service"
    if [[ "${WITH_JACK:-n}" == "y" ]]; then
        SP_AFTER="${SP_AFTER} jackd.service"
        SP_WANTS="${SP_WANTS} jackd.service"
    fi
    if [[ "$SHAIRPORT_AIRPLAY2" == "y" ]]; then
        SP_AFTER="${SP_AFTER} nqptp.service"
        SP_WANTS="${SP_WANTS} nqptp.service"
    fi

    SP_USER_LINES=""
    if [[ "${WITH_JACK:-n}" == "y" ]]; then
        # Run as jack user so shairport-sync can connect to JACK server
        SP_USER_LINES="User=jack
Group=audio
Environment=JACK_NO_AUDIO_RESERVATION=1
LimitRTPRIO=99
LimitMEMLOCK=infinity"
        SP_AFTER="${SP_AFTER} jack-ravenna-bridge.service"
    fi

    SP_CPU_AFF=""
    if [[ "${RT_ISOLCPU:-n}" == "y" && -n "${RT_ISOLCPU_CORE}" ]]; then
        SP_CPU_AFF="CPUAffinity=${RT_ISOLCPU_CORE}"
    fi

    SP_SERVICE="[Unit]
Description=Shairport Sync - AirPlay Audio Receiver
After=${SP_AFTER}
Wants=${SP_WANTS}

[Service]
Type=simple
${SP_USER_LINES}
ExecStart=/usr/local/bin/shairport-sync
Restart=on-failure
${SP_CPU_AFF}

[Install]
WantedBy=multi-user.target"

    run_remote "bash -s" <<REMOTE_SHAIRPORT_CONF
set -e

echo ">>> Stopping shairport-sync (if running)..."
sudo systemctl stop shairport-sync 2>/dev/null || true

echo ">>> Writing shairport-sync configuration..."
cat > /tmp/shairport-sync.conf <<'INNERSSCONF'
${SHAIRPORT_CONF_CONTENT}
INNERSSCONF
sudo cp /tmp/shairport-sync.conf /etc/shairport-sync.conf
rm /tmp/shairport-sync.conf

echo ">>> Creating shairport-sync systemd service..."
cat > /tmp/shairport-sync.service <<'INNERSPSVC'
${SP_SERVICE}
INNERSPSVC
sudo cp /tmp/shairport-sync.service /etc/systemd/system/shairport-sync.service
rm /tmp/shairport-sync.service

sudo systemctl daemon-reload
sudo systemctl enable shairport-sync
sudo systemctl start shairport-sync

# Verify
sleep 1
if sudo systemctl is-active shairport-sync &>/dev/null; then
    echo ">>> shairport-sync is running as '${SHAIRPORT_NAME}'."
else
    echo "WARNING: shairport-sync failed to start. Check: sudo journalctl -u shairport-sync -n 30"
fi
REMOTE_SHAIRPORT_CONF
    info "Shairport-sync configured and started."
fi

##############################################################################
# JACK Audio Server & Multi-IO Routing
##############################################################################
if [[ "${WITH_JACK:-n}" == "y" ]]; then
    step "Installing JACK audio server and routing infrastructure..."

    # --- Create a dedicated jack user (in audio group) ---
    run_remote "bash -s" <<'REMOTE_JACK_USER'
set -e
# Ensure a 'jack' user exists in the audio group for running jackd
if ! id jack &>/dev/null; then
    sudo useradd -r -g audio -G audio -M -s /usr/sbin/nologin -c "JACK Audio Server" jack
    echo ">>> Created 'jack' system user in audio group."
else
    echo ">>> 'jack' user already exists."
fi
REMOTE_JACK_USER

    # --- jackd systemd service (dummy driver — all devices are bridges) ---
    step "Creating jackd systemd service (dummy backend)..."
    run_remote "bash -s" <<REMOTE_JACK_SERVICE
set -e

echo ">>> Installing jackd systemd service (dummy backend)..."
sudo tee /etc/systemd/system/jackd.service > /dev/null <<'JACKSVC'
[Unit]
Description=JACK Audio Server (dummy backend, low-latency)
After=sound.target aes67-daemon.service
Wants=aes67-daemon.service
Before=shairport-sync.service

[Service]
Type=simple
User=jack
Group=audio
LimitRTPRIO=99
LimitMEMLOCK=infinity
LimitNICE=-20
Environment=JACK_NO_AUDIO_RESERVATION=1
ExecStartPre=/bin/sleep 2
ExecStart=/usr/bin/jackd -R -P89 -ddummy -r${SAMPLE_RATE} -p${JACK_PERIOD_SIZE} -C0 -P0
Restart=on-failure
RestartSec=3
$([ "${RT_ISOLCPU:-n}" = "y" ] && [ -n "${RT_ISOLCPU_CORE}" ] && echo "CPUAffinity=${RT_ISOLCPU_CORE}")

[Install]
WantedBy=multi-user.target
JACKSVC

sudo systemctl daemon-reload
sudo systemctl enable jackd
echo ">>> jackd.service installed (dummy @ ${SAMPLE_RATE}Hz, period=${JACK_PERIOD_SIZE})."
REMOTE_JACK_SERVICE
    info "jackd systemd service created (dummy backend)."

    # --- RAVENNA ALSA bridge (always, when JACK enabled) ---
    step "Creating RAVENNA ALSA bridge for JACK..."
    run_remote "bash -s" <<REMOTE_JACK_RAV_BRIDGE
set -e

echo ">>> Installing RAVENNA bridge scripts..."
sudo tee /usr/local/bin/jack-ravenna-bridge.sh > /dev/null <<RAVSCRIPT
#!/bin/bash
# RAVENNA JACK bridge — playback path only.
# Uses zita-j2a for robust ALSA-to-JACK bridging (handles xruns gracefully).
#
# NOTE: RAVENNA capture (AES67 network -> local) is managed exclusively by
# the aes67-daemon, which holds the ALSA capture device open. Received AES67
# sink audio is available through the daemon's API and ALSA capture, but
# cannot be simultaneously bridged into JACK.
#
# The playback path (JACK -> AES67 network) works via zita-j2a:
#   JACK clients -> ravenna-out:playback_* -> zita-j2a -> hw:RAVENNA -> AES67 network
export JACK_NO_AUDIO_RESERVATION=1
for i in \\\$(seq 1 30); do jack_lsp &>/dev/null && break; sleep 1; done
if ! jack_lsp &>/dev/null; then echo "ERROR: JACK not available"; exit 1; fi

echo ">>> Starting ravenna-out (${JACK_RAVENNA_OUT}ch playback to AES67 via zita-j2a)..."
exec zita-j2a -j ravenna-out -d ${JACK_RAVENNA_DEVICE} -c ${JACK_RAVENNA_OUT} -r ${SAMPLE_RATE} -p ${JACK_PERIOD_SIZE} -n 3
RAVSCRIPT
sudo chmod +x /usr/local/bin/jack-ravenna-bridge.sh

sudo tee /usr/local/bin/jack-ravenna-bridge-stop.sh > /dev/null <<'RAVSTOP'
#!/bin/bash
pkill -f 'zita-j2a -j ravenna-out' 2>/dev/null || true
RAVSTOP
sudo chmod +x /usr/local/bin/jack-ravenna-bridge-stop.sh

echo ">>> Installing jack-ravenna-bridge.service..."
sudo tee /etc/systemd/system/jack-ravenna-bridge.service > /dev/null <<'RAVSVC'
[Unit]
Description=JACK ALSA Bridge for RAVENNA (AES67)
After=jackd.service aes67-daemon.service
Requires=jackd.service
Before=jack-autoconnect.service

[Service]
Type=simple
User=jack
Group=audio
LimitRTPRIO=99
LimitMEMLOCK=infinity
Environment=JACK_NO_AUDIO_RESERVATION=1
RuntimeDirectory=jack
ExecStart=/usr/local/bin/jack-ravenna-bridge.sh
ExecStop=/usr/local/bin/jack-ravenna-bridge-stop.sh
Restart=on-failure
RestartSec=3
$([ "${RT_ISOLCPU:-n}" = "y" ] && [ -n "${RT_ISOLCPU_CORE}" ] && echo "CPUAffinity=${RT_ISOLCPU_CORE}")

[Install]
WantedBy=multi-user.target
RAVSVC

sudo systemctl daemon-reload
sudo systemctl enable jack-ravenna-bridge
echo ">>> jack-ravenna-bridge.service installed."
echo ">>>   ravenna-out:playback_* = JACK audio TO AES67 network (via zita-j2a)"
echo ">>>   NOTE: AES67 capture (network -> local) is managed by the daemon directly."
REMOTE_JACK_RAV_BRIDGE
    info "RAVENNA ALSA bridge service created."

    # --- Extra ALSA bridge services for additional I/O devices ---
    if [[ -n "${JACK_EXTRA_IO}" ]]; then
        step "Creating ALSA bridge services for extra I/O devices..."
        run_remote "bash -s" <<REMOTE_JACK_BRIDGES
set -e

# Parse extra I/O spec: "name:device:in_ch:out_ch name2:device2:in_ch2:out_ch2"
EXTRA_IO="${JACK_EXTRA_IO}"
BRIDGE_SERVICES=""

for entry in \${EXTRA_IO}; do
    IFS=':' read -r BNAME BDEV BIN BOUT <<< "\${entry}"
    if [ -z "\${BNAME}" ] || [ -z "\${BDEV}" ]; then
        echo "WARNING: Skipping malformed bridge entry: \${entry}"
        continue
    fi
    BIN=\${BIN:-0}
    BOUT=\${BOUT:-0}
    echo ">>> Configuring bridge: \${BNAME} (\${BDEV}, in=\${BIN}, out=\${BOUT})"

    # Build ExecStart lines for capture (alsa_in) and playback (alsa_out)
    EXEC_LINES=""
    EXEC_STOP_LINES=""

    if [ "\${BIN}" -gt 0 ] 2>/dev/null; then
        EXEC_LINES="\${EXEC_LINES}
ExecStart=/usr/bin/alsa_in -j \${BNAME}-in -d \${BDEV} -c \${BIN} -r ${SAMPLE_RATE} -p ${JACK_PERIOD_SIZE} -n ${JACK_NPERIODS} -q 1"
        EXEC_STOP_LINES="\${EXEC_STOP_LINES}
ExecStop=/usr/bin/pkill -f 'alsa_in -j \${BNAME}-in' || true"
    fi
    if [ "\${BOUT}" -gt 0 ] 2>/dev/null; then
        EXEC_LINES="\${EXEC_LINES}
ExecStart=/usr/bin/alsa_out -j \${BNAME}-out -d \${BDEV} -c \${BOUT} -r ${SAMPLE_RATE} -p ${JACK_PERIOD_SIZE} -n ${JACK_NPERIODS} -q 1"
        EXEC_STOP_LINES="\${EXEC_STOP_LINES}
ExecStop=/usr/bin/pkill -f 'alsa_out -j \${BNAME}-out' || true"
    fi

    if [ -z "\${EXEC_LINES}" ]; then
        echo "WARNING: Bridge \${BNAME} has 0 inputs and 0 outputs, skipping."
        continue
    fi

    sudo tee /etc/systemd/system/jack-bridge-\${BNAME}.service > /dev/null <<BRIDGESVC
[Unit]
Description=JACK ALSA Bridge: \${BNAME} (\${BDEV})
After=jackd.service
Requires=jackd.service

[Service]
Type=forking
User=jack
Group=audio
Environment=JACK_NO_AUDIO_RESERVATION=1
LimitRTPRIO=99
LimitMEMLOCK=infinity
ExecStartPre=/bin/sleep 1
\${EXEC_LINES}
\${EXEC_STOP_LINES}
Restart=on-failure
RestartSec=3
$([ "${RT_ISOLCPU:-n}" = "y" ] && [ -n "${RT_ISOLCPU_CORE}" ] && echo "CPUAffinity=${RT_ISOLCPU_CORE}")
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
BRIDGESVC

    BRIDGE_SERVICES="\${BRIDGE_SERVICES} jack-bridge-\${BNAME}.service"
    sudo systemctl daemon-reload
    sudo systemctl enable jack-bridge-\${BNAME}
    echo ">>> Bridge service jack-bridge-\${BNAME}.service created."
done

# Create a target that groups all bridges
if [ -n "\${BRIDGE_SERVICES}" ]; then
    sudo tee /etc/systemd/system/jack-alsa-bridges.service > /dev/null <<ALLBRIDGES
[Unit]
Description=All JACK ALSA Bridges
After=jackd.service
Requires=jackd.service
Wants=\${BRIDGE_SERVICES}

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/true

[Install]
WantedBy=multi-user.target
ALLBRIDGES
    sudo systemctl daemon-reload
    sudo systemctl enable jack-alsa-bridges
    echo ">>> jack-alsa-bridges.service (umbrella) created."
fi
REMOTE_JACK_BRIDGES
        info "ALSA bridge services created."
    fi

    # --- Auto-connect script + aj-snapshot for persistent routing ---
    step "Setting up JACK auto-connect routing..."
    run_remote "bash -s" <<REMOTE_JACK_ROUTING
set -e

# Install the auto-connect helper script
echo ">>> Installing JACK auto-connect script..."
sudo tee /usr/local/bin/jack-autoconnect.sh > /dev/null <<'CONNECTSCRIPT'
#!/bin/bash
# JACK auto-connect: waits for clients and makes default connections.
# Runs once at boot after all JACK clients are up.
# aj-snapshot then maintains persistent routing on top of this.

export JACK_NO_AUDIO_RESERVATION=1

wait_for_port() {
    local port="\$1"
    local timeout=\${2:-30}
    for i in \$(seq 1 \$timeout); do
        if jack_lsp 2>/dev/null | grep -q "\${port}"; then
            return 0
        fi
        sleep 1
    done
    echo "WARNING: Timed out waiting for JACK port: \${port}"
    return 1
}

echo ">>> Waiting for JACK server..."
for i in \$(seq 1 30); do
    if jack_lsp &>/dev/null; then
        break
    fi
    sleep 1
done

if ! jack_lsp &>/dev/null; then
    echo "ERROR: JACK server not available after 30s"
    exit 1
fi

echo ">>> JACK server is running. Available ports:"
jack_lsp

# Wait for RAVENNA bridge ports
wait_for_port "ravenna-out:playback_1" 30
wait_for_port "ravenna-in:capture_1" 30

CONNECTSCRIPT

# Append auto-connect rules for shairport if requested
if [ "${JACK_AUTOCONNECT}" = "y" ] && [ "${WITH_SHAIRPORT}" = "y" ]; then
    sudo tee -a /usr/local/bin/jack-autoconnect.sh > /dev/null <<'SHAIRPORTCONNECT'

# Auto-connect shairport-sync to RAVENNA playback (to AES67 network)
echo ">>> Waiting for shairport-sync JACK client..."
if wait_for_port "shairport-sync:out_L" 60; then
    echo ">>> Connecting shairport-sync -> ravenna-out (AES67 network)..."
    jack_connect "shairport-sync:out_L" "ravenna-out:playback_1" 2>/dev/null || true
    jack_connect "shairport-sync:out_R" "ravenna-out:playback_2" 2>/dev/null || true
    echo ">>> shairport-sync connected to RAVENNA outputs 1-2."
else
    echo ">>> shairport-sync not found, skipping auto-connect."
fi
SHAIRPORTCONNECT
fi

# Close the script
sudo tee -a /usr/local/bin/jack-autoconnect.sh > /dev/null <<'CONNECTEND'

echo ">>> Auto-connect complete. Current connections:"
jack_lsp -c

# Save routing state for aj-snapshot
if command -v aj-snapshot &>/dev/null; then
    aj-snapshot -f /etc/jack-routing.xml 2>/dev/null || true
    echo ">>> Routing snapshot saved to /etc/jack-routing.xml"
fi

echo ">>> JACK auto-connect finished."
CONNECTEND

sudo chmod +x /usr/local/bin/jack-autoconnect.sh

# Create auto-connect systemd service
echo ">>> Installing jack-autoconnect.service..."
sudo tee /etc/systemd/system/jack-autoconnect.service > /dev/null <<'ACSERVICE'
[Unit]
Description=JACK Auto-Connect Routing
After=jackd.service jack-ravenna-bridge.service jack-alsa-bridges.service shairport-sync.service
Wants=jackd.service jack-ravenna-bridge.service

[Service]
Type=oneshot
User=jack
Group=audio
Environment=JACK_NO_AUDIO_RESERVATION=1
ExecStart=/usr/local/bin/jack-autoconnect.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
ACSERVICE

# Create aj-snapshot service for persistent routing restoration
echo ">>> Installing aj-snapshot.service..."
sudo tee /etc/systemd/system/aj-snapshot.service > /dev/null <<'AJSERVICE'
[Unit]
Description=JACK Routing Snapshot Daemon (aj-snapshot)
After=jack-autoconnect.service jackd.service
Wants=jackd.service

[Service]
Type=simple
User=jack
Group=audio
Environment=JACK_NO_AUDIO_RESERVATION=1
ExecStartPre=/bin/bash -c 'for i in \$(seq 1 30); do jack_lsp &>/dev/null && break; sleep 1; done'
ExecStart=/usr/bin/aj-snapshot -d /etc/jack-routing.xml
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
AJSERVICE

sudo systemctl daemon-reload
sudo systemctl enable jack-autoconnect
sudo systemctl enable aj-snapshot

# Create an initial empty routing file if none exists
if [ ! -f /etc/jack-routing.xml ]; then
    echo '<?xml version="1.0" encoding="UTF-8"?><jack-session></jack-session>' | sudo tee /etc/jack-routing.xml > /dev/null
    sudo chown jack:audio /etc/jack-routing.xml
fi

echo ">>> JACK routing infrastructure installed."
REMOTE_JACK_ROUTING
    info "JACK auto-connect and aj-snapshot routing installed."

    # --- Start JACK and all dependent services ---
    if [[ "$START_AFTER_DEPLOY" == "y" ]]; then
        step "Starting JACK audio server..."
        run_remote "sudo systemctl restart jackd" || true
        sleep 3
        JACK_STATUS=$(run_remote "sudo systemctl is-active jackd" 2>/dev/null || echo "unknown")
        if [[ "$JACK_STATUS" == "active" ]]; then
            info "jackd is running (dummy backend)."
            # Start RAVENNA bridge
            run_remote "sudo systemctl restart jack-ravenna-bridge" || true
            sleep 2
            info "RAVENNA bridge started."
            # Start extra bridges if configured
            if [[ -n "${JACK_EXTRA_IO}" ]]; then
                run_remote "sudo systemctl restart jack-alsa-bridges" || true
                sleep 2
                info "ALSA bridges started."
            fi
            # Restart shairport-sync now that JACK is running (it may have started earlier)
            if [[ "${WITH_SHAIRPORT:-n}" == "y" ]]; then
                run_remote "sudo systemctl restart shairport-sync" || true
                sleep 2
                info "shairport-sync restarted (connected to JACK)."
            fi
            # Run auto-connect
            run_remote "sudo systemctl restart jack-autoconnect" || true
            # Start aj-snapshot daemon
            run_remote "sudo systemctl restart aj-snapshot" || true
            info "JACK routing active."
            # Show JACK port status
            echo
            echo -e "  ${BOLD}JACK Ports:${NC}"
            run_remote "JACK_NO_AUDIO_RESERVATION=1 sudo -u jack jack_lsp -c 2>/dev/null" || true
            echo
        else
            warn "jackd status: ${JACK_STATUS}"
            warn "Check logs: ssh ${REMOTE} 'sudo journalctl -u jackd -n 50'"
        fi
    fi

    # --- Install jack-plumbing helper script for runtime use ---
    step "Installing JACK routing helper tools..."
    run_remote "bash -s" <<'REMOTE_JACK_TOOLS'
set -e

# jack-route: convenience script for managing JACK connections
sudo tee /usr/local/bin/jack-route > /dev/null <<'JACKROUTE'
#!/bin/bash
# jack-route: manage JACK audio routing
# Usage:
#   jack-route list              - show all ports and connections
#   jack-route connect A B       - connect port A to port B
#   jack-route disconnect A B    - disconnect port A from port B
#   jack-route save [file]       - save current routing
#   jack-route restore [file]    - restore saved routing
#   jack-route monitor           - watch for port changes

export JACK_NO_AUDIO_RESERVATION=1
ROUTING_FILE="${1:-/etc/jack-routing.xml}"

case "${1:-list}" in
    list|ls|status)
        echo "=== JACK Ports & Connections ==="
        jack_lsp -c 2>/dev/null || echo "ERROR: Cannot connect to JACK server"
        ;;
    connect|c)
        if [ -z "$2" ] || [ -z "$3" ]; then
            echo "Usage: jack-route connect <output-port> <input-port>"
            echo "Example: jack-route connect shairport-sync:out_L ravenna-out:playback_1"
            exit 1
        fi
        jack_connect "$2" "$3" && echo "Connected: $2 -> $3"
        # Auto-save after connect
        aj-snapshot -f /etc/jack-routing.xml 2>/dev/null || true
        ;;
    disconnect|dc|d)
        if [ -z "$2" ] || [ -z "$3" ]; then
            echo "Usage: jack-route disconnect <output-port> <input-port>"
            exit 1
        fi
        jack_disconnect "$2" "$3" && echo "Disconnected: $2 -> $3"
        aj-snapshot -f /etc/jack-routing.xml 2>/dev/null || true
        ;;
    save|s)
        FILE="${2:-/etc/jack-routing.xml}"
        aj-snapshot -f "$FILE" && echo "Routing saved to $FILE"
        ;;
    restore|r)
        FILE="${2:-/etc/jack-routing.xml}"
        aj-snapshot -r "$FILE" && echo "Routing restored from $FILE"
        ;;
    monitor|mon|m)
        echo "Monitoring JACK port changes (Ctrl+C to stop)..."
        jack_lsp -c
        echo "---"
        # Use jack_connect_checker or polling fallback
        while true; do
            sleep 2
            jack_lsp -c 2>/dev/null
            echo "--- $(date +%H:%M:%S) ---"
        done
        ;;
    *)
        echo "Usage: jack-route {list|connect|disconnect|save|restore|monitor}"
        ;;
esac
JACKROUTE
sudo chmod +x /usr/local/bin/jack-route

echo ">>> jack-route helper installed."
echo ">>>   Use 'jack-route list' to see all ports and connections."
echo ">>>   Use 'jack-route connect <out> <in>' to route audio."
REMOTE_JACK_TOOLS
    info "JACK routing tools installed."
fi

##############################################################################
# Done!
##############################################################################
echo
echo -e "${GREEN}${BOLD}============================================================${NC}"
echo -e "${GREEN}${BOLD}  Deployment complete!${NC}"
echo -e "${GREEN}${BOLD}============================================================${NC}"
echo
echo -e "  Web UI:  ${BOLD}http://${TARGET_HOST}:${HTTP_PORT}${NC}"
echo -e "  Config:  ${BOLD}/etc/daemon.conf${NC} (on remote)"
echo -e "  Logs:    ${BOLD}sudo journalctl -u aes67-daemon -f${NC}"
echo -e "  Status:  ${BOLD}sudo systemctl status aes67-daemon${NC}"
if [[ "${WITH_JACK:-n}" == "y" ]]; then
echo -e ""
echo -e "  ${BOLD}JACK Audio Routing (dummy backend + bridges):${NC}"
echo -e "    Server:  ${BOLD}jackd${NC} dummy @ ${SAMPLE_RATE}Hz p=${JACK_PERIOD_SIZE} n=${JACK_NPERIODS}"
echo -e "    RAVENNA: ${BOLD}ravenna-out:playback_1..${JACK_RAVENNA_OUT}${NC} (to AES67)"
echo -e "             ${BOLD}ravenna-in:capture_1..${JACK_RAVENNA_IN}${NC}  (from AES67)"
if [[ -n "${JACK_EXTRA_IO}" ]]; then
echo -e "    Bridges: ${BOLD}${JACK_EXTRA_IO}${NC}"
fi
echo -e "    Route:   ${BOLD}sudo -u jack jack-route list${NC}"
echo -e "    Route:   ${BOLD}sudo -u jack jack-route connect <out> <in>${NC}"
echo -e "    Save:    ${BOLD}sudo -u jack jack-route save${NC}"
echo -e "    Logs:    ${BOLD}sudo journalctl -u jackd -u jack-ravenna-bridge -f${NC}"
fi
if [[ "$WITH_SHAIRPORT" == "y" ]]; then
echo -e ""
if [[ "${WITH_JACK:-n}" == "y" ]]; then
echo -e "  AirPlay: ${BOLD}\"${SHAIRPORT_NAME}\"${NC} -> JACK (auto-connected to RAVENNA)"
else
echo -e "  AirPlay: ${BOLD}\"${SHAIRPORT_NAME}\"${NC} -> ${SHAIRPORT_OUTPUT}"
fi
echo -e "  Logs:    ${BOLD}sudo journalctl -u shairport-sync -f${NC}"
fi
if [[ "$ENABLE_I2S" == "y" ]]; then
echo -e ""
echo -e "  I2S:     ${BOLD}${I2S_OVERLAY}${NC} (reboot needed if first time)"
fi
if [[ "${APPLY_RT_TUNING:-n}" == "y" ]]; then
echo -e ""
echo -e "  ${YELLOW}RT tuning applied (cmdline.txt, config.txt, CPU governor, limits).${NC}"
echo -e "  ${YELLOW}A reboot is recommended to activate kernel boot parameters.${NC}"
fi
echo
echo -e "  To redeploy with same settings:"
echo -e "    ${CYAN}./deploy.sh ${SAVE_PATH}${NC}"
echo
