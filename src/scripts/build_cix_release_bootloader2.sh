#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
Usage: build_cix_release_bootloader2.sh [options]

Required:
  --tfa-dir <path>        Imported CIX TF-A V1.2 source tree
  --tee-dir <path>        Imported CIX OP-TEE V1.2 source tree
  --build-root <path>     Board build directory containing Keys/, certs/, Firmwares/
  --fiptool <path>        Host fiptool binary
  --output <path>         Destination bootloader2.img path

Optional:
  --cross-compile <pref>  AArch64 cross compiler prefix
  --jobs <n>              Parallel make jobs (default: 1)
  --mode <release|debug>  Build mode for TF-A (default: release)
  --verbose               Stream tool output directly
EOF
}

require_file() {
	local path="$1"
	if [[ ! -f "$path" ]]; then
		printf 'Missing required file: %s\n' "$path" >&2
		exit 1
	fi
}

require_dir() {
	local path="$1"
	if [[ ! -d "$path" ]]; then
		printf 'Missing required directory: %s\n' "$path" >&2
		exit 1
	fi
}

run() {
	if [[ "$VERBOSE" == "1" ]]; then
		"$@"
		return
	fi

	local output
	if ! output="$("$@" 2>&1)"; then
		[[ -n "$output" ]] && printf '%s\n' "$output" >&2
		return 1
	fi
}

TFA_DIR=
TEE_DIR=
BUILD_ROOT=
FIPTOOL=
OUTPUT=
CROSS_COMPILE=
JOBS=1
MODE=release
VERBOSE=0

if [[ "${V:-0}" == "1" ]]; then
	VERBOSE=1
fi

while [[ $# -gt 0 ]]; do
	case "$1" in
		--tfa-dir)
			TFA_DIR="$2"
			shift 2
			;;
		--tee-dir)
			TEE_DIR="$2"
			shift 2
			;;
		--build-root)
			BUILD_ROOT="$2"
			shift 2
			;;
		--fiptool)
			FIPTOOL="$2"
			shift 2
			;;
		--output)
			OUTPUT="$2"
			shift 2
			;;
		--cross-compile)
			CROSS_COMPILE="$2"
			shift 2
			;;
		--jobs)
			JOBS="$2"
			shift 2
			;;
		--mode)
			MODE="${2,,}"
			shift 2
			;;
		--verbose)
			VERBOSE=1
			shift
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			printf 'Unknown argument: %s\n\n' "$1" >&2
			usage >&2
			exit 1
			;;
	esac
done

if [[ -z "$TFA_DIR" || -z "$TEE_DIR" || -z "$BUILD_ROOT" || -z "$FIPTOOL" || -z "$OUTPUT" ]]; then
	usage >&2
	exit 1
fi

case "$MODE" in
	release|debug) ;;
	*)
		printf -- '--mode must be release or debug, got: %s\n' "$MODE" >&2
		exit 1
		;;
esac

require_dir "$TFA_DIR"
require_dir "$TEE_DIR"
require_dir "$BUILD_ROOT"
require_file "$FIPTOOL"

KEYS_DIR="${BUILD_ROOT}/Keys"
CERTS_DIR="${BUILD_ROOT}/certs"
FIRMWARE_DIR="${BUILD_ROOT}/Firmwares"
require_dir "$KEYS_DIR"
require_dir "$CERTS_DIR"
require_dir "$FIRMWARE_DIR"
require_file "${KEYS_DIR}/oem_privatekey.pem"
require_file "${KEYS_DIR}/oem_publickey.pem"

TEMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/cix-release-bl2.XXXXXX")"
TFA_BUILD_ROOT="${TEMP_ROOT}/tf-a"
TEE_OUTPUT="${TEE_DIR}/out/arm-plat-cix/core/tee-raw.bin"
CERT_CREATE_DIR="${TFA_DIR}/tools/cert_create"
CERT_CREATE_BIN="${CERT_CREATE_DIR}/cert_create"

cleanup() {
	rm -rf "$TEMP_ROOT"
	rm -rf "${TEE_DIR}/out" "${TEE_DIR}/tee.bin"
	if [[ -d "$CERT_CREATE_DIR" ]]; then
		(
			cd "$CERT_CREATE_DIR"
			make realclean >/dev/null 2>&1 || true
		)
	fi
}
trap cleanup EXIT

ensure_cert_create() {
	if [[ -x "$CERT_CREATE_BIN" ]]; then
		return
	fi
	printf '[cix-release] Building TF-A cert_create helper\n'
	run make -C "$CERT_CREATE_DIR" OPENSSL_DIR=/usr clean all
	require_file "$CERT_CREATE_BIN"
}

TF_A_DEBUG=0
if [[ "$MODE" == "debug" ]]; then
	TF_A_DEBUG=1
fi

ensure_cert_create

printf '[cix-release] Building TF-A BL31 from curated CIX V1.2 sources\n'
run env CROSS_COMPILE="$CROSS_COMPILE" \
	make -C "$TFA_DIR" "-j${JOBS}" \
		PLAT=sky1 SPD=opteed \
		DEBUG="${TF_A_DEBUG}" \
		BUILD_BASE="$TFA_BUILD_ROOT" \
		CIX_BOARD=evb \
		SMP=1 \
		TRUSTED_BOARD_BOOT=1 \
		ENABLE_FEAT_HCX=1 \
		ARM_ROTPK_LOCATION=devel_rsa \
		ROT_KEY=plat/arm/board/common/rotpk/arm_rotprivk_rsa.pem \
		bl31

BL31_BIN="${TFA_BUILD_ROOT}/sky1/${MODE}/bl31.bin"
require_file "$BL31_BIN"

printf '[cix-release] Building OP-TEE from curated CIX V1.2 sources\n'
run make -C "$TEE_DIR" clean
rm -rf "${TEE_DIR}/out" "${TEE_DIR}/tee.bin"

TEE_ENV=(
	"PLATFORM=cix"
	"PLATFORM_FLAVOR=sky1"
	"TA_SIGN_KEY=${KEYS_DIR}/oem_privatekey.pem"
	"ARCH=arm"
	"CROSS_COMPILE64=${CROSS_COMPILE}"
	"CFG_ARM64_core=y"
	"CFG_USER_TA_TARGETS=ta_arm64"
)
if [[ -f "${FIRMWARE_DIR}/BL32_AP_EFI_STMM.fd" ]]; then
	TEE_ENV+=("CFG_STMM_PATH=${FIRMWARE_DIR}/BL32_AP_EFI_STMM.fd")
fi
run env "${TEE_ENV[@]}" make -C "$TEE_DIR" "-j${JOBS}" all
require_file "$TEE_OUTPUT"

printf '[cix-release] Generating bootloader2 certificates\n'
ensure_cert_create
rm -f \
	"${CERTS_DIR}/trusted_key.crt" \
	"${CERTS_DIR}/bl31_fw_key.crt" \
	"${CERTS_DIR}/tos_fw_key.crt" \
	"${CERTS_DIR}/bl31_fw_content.crt" \
	"${CERTS_DIR}/tos_fw_cert.crt"

run "$CERT_CREATE_BIN" \
	--key-alg rsa --key-size 3072 \
	--hash-alg sha256 --tfw-nvctr 31 \
	--rot-key "${KEYS_DIR}/oem_privatekey.pem" \
	--trusted-world-key "${KEYS_DIR}/oem_privatekey.pem" \
	--non-trusted-world-key "${KEYS_DIR}/oem_privatekey.pem" \
	--scp-fw-key "${KEYS_DIR}/oem_privatekey.pem" \
	--soc-fw-key "${KEYS_DIR}/oem_privatekey.pem" \
	--tos-fw-key "${KEYS_DIR}/oem_privatekey.pem" \
	--nt-fw-key "${KEYS_DIR}/oem_privatekey.pem" \
	--trusted-key-cert "${CERTS_DIR}/trusted_key.crt" \
	--soc-fw-key-cert "${CERTS_DIR}/bl31_fw_key.crt" \
	--tos-fw-key-cert "${CERTS_DIR}/tos_fw_key.crt" \
	--soc-fw-cert "${CERTS_DIR}/bl31_fw_content.crt" \
	--tos-fw-cert "${CERTS_DIR}/tos_fw_cert.crt" \
	--soc-fw "$BL31_BIN" \
	--tos-fw "$TEE_OUTPUT"

printf '[cix-release] Packaging bootloader2.img\n'
mkdir -p "$(dirname "$OUTPUT")"
rm -f "$OUTPUT"
run "$FIPTOOL" create \
	--soc-fw "$BL31_BIN" \
	--tos-fw "$TEE_OUTPUT" \
	--trusted-key-cert "${CERTS_DIR}/trusted_key.crt" \
	--soc-fw-key-cert "${CERTS_DIR}/bl31_fw_key.crt" \
	--tos-fw-key-cert "${CERTS_DIR}/tos_fw_key.crt" \
	--soc-fw-cert "${CERTS_DIR}/bl31_fw_content.crt" \
	--tos-fw-cert "${CERTS_DIR}/tos_fw_cert.crt" \
	"$OUTPUT"

require_file "$OUTPUT"
