#!/bin/bash
set -e

COLOR_PRINT_RED="\033[1;31m"
COLOR_PRINT_GREEN="\033[1;32m"
COLOR_PRINT_YELLOW="\033[1;33m"
END_COLOR_PRINT="\033[0m"

PUBLIC_ADDRESS=""
MAIN_INSTALL_DIRECTORY="xcash-labs"
INSTALL_DIR="$HOME/${MAIN_INSTALL_DIRECTORY}/"
BLOCKCHAIN_DIR="$HOME/.XCASH-LABS/"
XCASH_DIR="${INSTALL_DIR}xcash-labs-core/"
WALLET_DIR="${INSTALL_DIR}xcash-wallets/"
LOGS_DIR="${INSTALL_DIR}logs/"
PID_DIR="${INSTALL_DIR}systemdpid/"
XCASH_BLOCKCHAIN_BOOTSTRAP_URL="https://xcashlabs.org/downloads/block-1.7z"

XCASH_URL="https://github.com/Xcash-Labs/xcash-labs-core.git"
XCASH_BRANCH="master"

WALLET_NAME="xcashklassic-wallet"
WALLET_PASSWORD=""
WALLET_SEED=""

CPU_THREADS=$(nproc)
PACKAGES="build-essential cmake pkg-config libssl-dev libzmq3-dev libunbound-dev libsodium-dev libunwind8-dev liblzma-dev libreadline6-dev libexpat1-dev libboost-chrono-dev libboost-date-time-dev libboost-filesystem-dev libboost-locale-dev libboost-program-options-dev libboost-regex-dev libboost-serialization-dev libboost-system-dev libboost-thread-dev git curl autoconf libtool gperf p7zip-full libzmq5 libcurl4-openssl-dev ccache"

function get_password()
{
  if [ "$EUID" -ne 0 ]; then
    sudo echo
  fi
}

function create_directories()
{
  echo -e "${COLOR_PRINT_YELLOW}Creating directories${END_COLOR_PRINT}"
  mkdir -p "$INSTALL_DIR" "$WALLET_DIR" "$LOGS_DIR" "$PID_DIR" "$BLOCKCHAIN_DIR"
  touch "${PID_DIR}xcash-daemon.pid"
}

function install_packages()
{
  echo -e "${COLOR_PRINT_YELLOW}Installing packages${END_COLOR_PRINT}"
  sudo apt update -y
  sudo apt install $PACKAGES -y
}

function download_xcash()
{
  echo -e "${COLOR_PRINT_YELLOW}Downloading XCash Labs Core${END_COLOR_PRINT}"
  cd "$INSTALL_DIR"

  if [ ! -d "$XCASH_DIR/.git" ]; then
    git clone "$XCASH_URL" "$XCASH_DIR"
  fi

  cd "$XCASH_DIR"
  git checkout "$XCASH_BRANCH"
  git pull
  git submodule update --init --force
}

function build_xcash()
{
  echo -e "${COLOR_PRINT_YELLOW}Building XCash Labs Core${END_COLOR_PRINT}"
  cd "$XCASH_DIR"

  echo "y" | make clean || true

  JOBS=$((CPU_THREADS / 2))
  if [ "$JOBS" -lt 1 ]; then
    JOBS=1
  fi

  make release -j "$JOBS"

  echo -e "${COLOR_PRINT_GREEN}Build complete${END_COLOR_PRINT}"
}

function install_blockchain()
{
  cd $HOME
  cd && test -f block-1.7z && sudo rm -rf block-1.7z*
  echo -e "${COLOR_PRINT_GREEN}Starting the Download${END_COLOR_PRINT}"
  wget -q --show-progress ${XCASH_BLOCKCHAIN_BOOTSTRAP_URL}
  echo -e "${COLOR_PRINT_GREEN}Starting Extraction${END_COLOR_PRINT}"
  sudo rm -r ${BLOCKCHAIN_DIR} &>/dev/null || true  
  7z x block-1.7z -bso0 -bse0
  sudo rm block-1.7z 
  echo -e "${COLOR_PRINT_GREEN}Installing The BlockChain Completed${END_COLOR_PRINT}"
  echo
}

function get_wallet_settings()
{
  echo -ne "${COLOR_PRINT_YELLOW}Create new wallet? Press ENTER for yes, type N to import existing seed: ${END_COLOR_PRINT}"
  read -r data

  WALLET_PASSWORD=$(< /dev/urandom tr -dc 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789' | head -c 32; echo)

  echo -ne "${COLOR_PRINT_YELLOW}Use generated wallet password? Press ENTER for yes, type N to enter custom password: ${END_COLOR_PRINT}"
  read -r pass_choice

  if [[ "$pass_choice" == "N" || "$pass_choice" == "n" ]]; then
    echo -ne "${COLOR_PRINT_YELLOW}Enter wallet password: ${END_COLOR_PRINT}"
    read -r WALLET_PASSWORD
  fi

  if [[ "$data" == "N" || "$data" == "n" ]]; then
    echo -ne "${COLOR_PRINT_YELLOW}Enter existing wallet mnemonic seed: ${END_COLOR_PRINT}"
    read -r WALLET_SEED
  fi
}

function create_or_import_wallet()
{
  echo -e "${COLOR_PRINT_GREEN}############################################################${END_COLOR_PRINT}"
  echo -e "${COLOR_PRINT_GREEN}      Creating/Importing X-CASH Wallet                  ${END_COLOR_PRINT}"
  echo -e "${COLOR_PRINT_GREEN}############################################################${END_COLOR_PRINT}"

  cd "${INSTALL_DIR}"

  echo -ne "${COLOR_PRINT_YELLOW}Starting local daemon${END_COLOR_PRINT}"
  sudo systemctl start xcash-daemon >/dev/null 2>&1
  sleep 20
  echo -ne "\r${COLOR_PRINT_GREEN}Starting local daemon${END_COLOR_PRINT}"
  echo

  sudo rm -f "${WALLET_DIR}${WALLET_NAME}"* >/dev/null 2>&1 || true

  if [ -z "$WALLET_SEED" ]; then
    echo -e "${COLOR_PRINT_YELLOW}Creating new wallet${END_COLOR_PRINT}"

    echo "exit" | "${XCASH_DIR}build/Linux/master/release/bin/xcash-wallet-cli" \
      --generate-new-wallet "${WALLET_DIR}${WALLET_NAME}" \
      --password "$WALLET_PASSWORD" \
      --mnemonic-language English \
      --restore-height 0 \
      --trusted-daemon \
      | stdbuf -oL tr '\r' '\n' \
      | stdbuf -o 0 grep -C 1 "Height" \
      | stdbuf -o 0 awk '{print "Processing: ",$1,$2,$3,$4}' ORS="\r"
  else
    echo -e "${COLOR_PRINT_YELLOW}Importing existing wallet${END_COLOR_PRINT}"

    (echo -ne "\n"; echo "${WALLET_PASSWORD}"; echo "exit") | "${XCASH_DIR}build/Linux/master/release/bin/xcash-wallet-cli" \
    --restore-deterministic-wallet \
    --electrum-seed "$WALLET_SEED" \
    --generate-new-wallet "${WALLET_DIR}${WALLET_NAME}" \
    --password "$WALLET_PASSWORD" \
    --mnemonic-language English \
    --restore-height 0 \
    --trusted-daemon \
    | stdbuf -oL tr '\r' '\n' \
    | stdbuf -o 0 grep -C 1 "Height" \
    | stdbuf -o 0 awk '{print "Processing: ",$1,$2,$3,$4}' ORS="\r"
  fi

  echo -ne "                                                                              \r"
  echo -e "${COLOR_PRINT_GREEN}Wallet Refresh Completed${END_COLOR_PRINT}"

  echo -ne "${COLOR_PRINT_YELLOW}Stopping local daemon${END_COLOR_PRINT}"
  sudo systemctl stop xcash-daemon >/dev/null 2>&1 || true
  sleep 10
  echo -ne "\r${COLOR_PRINT_GREEN}Stopping local daemon${END_COLOR_PRINT}"
  echo
}


function create_systemd_services()
{
  echo -e "${COLOR_PRINT_YELLOW}Creating systemd service files${END_COLOR_PRINT}"

  sudo bash -c "cat > /lib/systemd/system/xcash-daemon.service" <<EOF
# Wallet / Exchange node.
# Assumes xcashd and xcash-wallet-rpc
# are running on the same server. RPC is bound to localhost.
#
[Unit]
Description=X-Cash Daemon background process
After=network-online.target time-sync.target
StartLimitIntervalSec=60
StartLimitBurst=4

[Service]
Type=forking
LimitNOFILE=65535
TasksMax=infinity
User=${USER}
PIDFile=${PID_DIR}xcash-daemon.pid
Environment="GLIBC_TUNABLES=glibc.pthread.rseq=0"
ExecStart=${XCASH_DIR}build/Linux/master/release/bin/xcashd \
  --no-zmq \
  --no-dpops \
  --data-dir ${BLOCKCHAIN_DIR} \
  --rpc-bind-ip 127.0.0.1 \
  --p2p-bind-ip 0.0.0.0 \
  --rpc-bind-port 18281 \
  --restricted-rpc \
  --log-file ${LOGS_DIR}xcash-daemon-log.txt \
  --max-log-file-size 0 \
  --detach \
  --pidfile ${PID_DIR}xcash-daemon.pid

Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
EOF

  sudo bash -c "cat > /lib/systemd/system/xcash-rpc-wallet.service" <<EOF
# Wallet / Exchange node.
# Assumes xcash-wallet-rpc and xcashd are running on the same server.
# Wallet RPC is bound to localhost and intended for local access only.
#
[Unit]
After=network-online.target xcash-daemon.service
Requires=xcash-daemon.service

[Service]
Type=simple
User=${USER}
ExecStart=${XCASH_DIR}build/Linux/master/release/bin/xcash-wallet-rpc \
  --wallet-file ${WALLET_DIR}${WALLET_NAME} \
  --password ${WALLET_PASSWORD} \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 18285 \
  --daemon-address 127.0.0.1:18281 \
  --disable-rpc-login \
  --trusted-daemon \
  --log-file ${LOGS_DIR}xcash-wallet-rpc.log
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

  sudo systemctl daemon-reload
}

function start_services()
{
  echo -e "${COLOR_PRINT_YELLOW}Starting services${END_COLOR_PRINT}"

  sudo systemctl enable xcash-daemon
  sudo systemctl enable xcash-rpc-wallet

  sudo systemctl restart xcash-daemon
  sleep 10
  sudo systemctl is-active --quiet xcash-daemon

  sudo systemctl restart xcash-rpc-wallet
}

function get_current_xcash_wallet_data()
{
  echo
  echo -ne "${COLOR_PRINT_YELLOW}Getting current X-CASH wallet data${END_COLOR_PRINT}"

  sudo systemctl start xcash-daemon >/dev/null 2>&1
  sleep 20

  "${XCASH_DIR}build/Linux/master/release/bin/xcash-wallet-rpc" \
    --wallet-file "${WALLET_DIR}${WALLET_NAME}" \
    --password "${WALLET_PASSWORD}" \
    --rpc-bind-ip 127.0.0.1 \
    --rpc-bind-port 18288 \
    --daemon-address 127.0.0.1:18281 \
    --disable-rpc-login \
    --trusted-daemon \
    --log-file "${LOGS_DIR}xcash-wallet-rpc-temp.log" \
    >/dev/null 2>&1 &

  TEMP_WALLET_RPC_PID=$!

  for i in {1..30}; do
    data=$(curl -s -X POST http://127.0.0.1:18288/json_rpc \
      -H 'Content-Type: application/json' \
      -d '{"jsonrpc":"2.0","id":"0","method":"get_address"}')

    if echo "$data" | grep -q '"address"'; then
      break
    fi

    sleep 2
  done

  PUBLIC_ADDRESS=$(curl -s -X POST http://127.0.0.1:18288/json_rpc \
    -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":"0","method":"get_address"}' \
    | grep \"address\" | head -1 | sed s"|    \"address\": ||g" | sed s"|\"||g" | sed s"|,||g")

  WALLET_SEED=$(curl -s -X POST http://127.0.0.1:18288/json_rpc \
    -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":"0","method":"query_key","params":{"key_type":"mnemonic"}}' \
    | grep \"key\" | sed s"|    \"key\": ||g" | sed s"|\"||g")

  curl -s -X POST http://127.0.0.1:18288/json_rpc \
    -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":"0","method":"stop_wallet"}' >/dev/null 2>&1 || true

  kill "$TEMP_WALLET_RPC_PID" >/dev/null 2>&1 || true

  echo -ne "\r${COLOR_PRINT_GREEN}Getting current X-CASH wallet data${END_COLOR_PRINT}"
  echo
}

function print_summary()
{
  echo
  echo -e "${COLOR_PRINT_GREEN}############################################################${END_COLOR_PRINT}"
  echo -e "${COLOR_PRINT_GREEN}Installation complete${END_COLOR_PRINT}"
  echo -e "${COLOR_PRINT_GREEN}############################################################${END_COLOR_PRINT}"
  echo
  echo -e "${COLOR_PRINT_YELLOW}Wallet file:${END_COLOR_PRINT} ${WALLET_DIR}${WALLET_NAME}"
  echo -e "${COLOR_PRINT_YELLOW}Public address:${END_COLOR_PRINT} ${PUBLIC_ADDRESS}"
  echo -e "${COLOR_PRINT_YELLOW}Wallet seed:${END_COLOR_PRINT} ${WALLET_SEED}"
  echo -e "${COLOR_PRINT_YELLOW}Wallet RPC:${END_COLOR_PRINT} 127.0.0.1:18285"
  echo -e "${COLOR_PRINT_YELLOW}Daemon RPC:${END_COLOR_PRINT} 127.0.0.1:18281"
  echo -e "${COLOR_PRINT_YELLOW}Wallet password:${END_COLOR_PRINT} ${WALLET_PASSWORD}"
  echo
  echo -e "${COLOR_PRINT_RED}Save the wallet password securely.${END_COLOR_PRINT}"
}

get_password
create_directories
install_packages
download_xcash
build_xcash
install_blockchain
get_wallet_settings
create_systemd_services
create_or_import_wallet
get_current_xcash_wallet_data
start_services
print_summary
