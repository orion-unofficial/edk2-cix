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
  --enable-tf-a-fixes     Build BL31 with custom TF-A fixes enabled
  --cache-root <path>     Persistent cache root for cert_create, BL31, and tee-raw.bin
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
ENABLE_TF_A_FIXES=0
VERBOSE=0
CACHE_ROOT=

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CACHE_PLAN_HELPER="${REPO_ROOT}/scripts/cix_release_cache.py"

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
		--cache-root)
			CACHE_ROOT="$2"
			shift 2
			;;
		--enable-tf-a-fixes)
			ENABLE_TF_A_FIXES=1
			shift
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
STMM_PATH="${FIRMWARE_DIR}/BL32_AP_EFI_STMM.fd"
HOST_CC="${CC:-cc}"
CROSS_GCC="${CROSS_COMPILE}gcc"
ACTIVE_CERT_CREATE_BIN="$CERT_CREATE_BIN"
ACTIVE_BL31_BIN=
ACTIVE_TEE_BIN="$TEE_OUTPUT"
CERT_CREATE_CACHE_BIN=
BL31_CACHE_BIN=
TEE_CACHE_BIN=

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

install_cache_file() {
	local source_path="$1"
	local dest_path="$2"
	local dest_dir tmp_dir

	dest_dir="$(dirname "$dest_path")"
	mkdir -p "$dest_dir"
	tmp_dir="$(mktemp -d "${dest_dir}/.tmp.XXXXXX")"
	cp -f "$source_path" "${tmp_dir}/$(basename "$dest_path")"
	mv -f "${tmp_dir}/$(basename "$dest_path")" "$dest_path"
	rmdir "$tmp_dir"
}

load_cache_plan() {
	if [[ -z "$CACHE_ROOT" ]]; then
		return
	fi

	local cache_cmd
	cache_cmd=(
		python3 "$CACHE_PLAN_HELPER"
		--cache-root "$CACHE_ROOT"
		--tfa-dir "$TFA_DIR"
		--tee-dir "$TEE_DIR"
		--helper-script "$0"
		--mode "$MODE"
		--cross-compiler "$CROSS_GCC"
		--host-compiler "$HOST_CC"
		--shell
	)
	if [[ "$ENABLE_TF_A_FIXES" == "1" ]]; then
		cache_cmd+=(--enable-tf-a-fixes)
	fi
	if [[ -f "$STMM_PATH" ]]; then
		cache_cmd+=(--stmm-path "$STMM_PATH")
	fi
	eval "$("${cache_cmd[@]}")"
}

ensure_cert_create() {
	if [[ -n "$CERT_CREATE_CACHE_BIN" && -x "$CERT_CREATE_CACHE_BIN" ]]; then
		printf '[cix-release] Reusing cached TF-A cert_create helper\n'
		ACTIVE_CERT_CREATE_BIN="$CERT_CREATE_CACHE_BIN"
		return
	fi
	if [[ -z "$CERT_CREATE_CACHE_BIN" && -x "$CERT_CREATE_BIN" ]]; then
		ACTIVE_CERT_CREATE_BIN="$CERT_CREATE_BIN"
		return
	fi
	printf '[cix-release] Building TF-A cert_create helper\n'
	run make -C "$CERT_CREATE_DIR" OPENSSL_DIR=/usr clean all
	require_file "$CERT_CREATE_BIN"
	if [[ -n "$CERT_CREATE_CACHE_BIN" ]]; then
		install_cache_file "$CERT_CREATE_BIN" "$CERT_CREATE_CACHE_BIN"
		printf '[cix-release] Cached TF-A cert_create helper\n'
		ACTIVE_CERT_CREATE_BIN="$CERT_CREATE_CACHE_BIN"
		return
	fi
	ACTIVE_CERT_CREATE_BIN="$CERT_CREATE_BIN"
}

ensure_bl31() {
	if [[ -n "$BL31_CACHE_BIN" && -s "$BL31_CACHE_BIN" ]]; then
		printf '[cix-release] Reusing cached TF-A BL31 from curated CIX V1.2 sources\n'
		ACTIVE_BL31_BIN="$BL31_CACHE_BIN"
		return
	fi

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
			ENABLE_TF_A_FIXES="$ENABLE_TF_A_FIXES" \
			ARM_ROTPK_LOCATION=devel_rsa \
			ROT_KEY=plat/arm/board/common/rotpk/arm_rotprivk_rsa.pem \
			bl31

	local built_bl31
	built_bl31="${TFA_BUILD_ROOT}/sky1/${MODE}/bl31.bin"
	require_file "$built_bl31"
	if [[ -n "$BL31_CACHE_BIN" ]]; then
		install_cache_file "$built_bl31" "$BL31_CACHE_BIN"
		printf '[cix-release] Cached TF-A BL31\n'
		ACTIVE_BL31_BIN="$BL31_CACHE_BIN"
		return
	fi
	ACTIVE_BL31_BIN="$built_bl31"
}

ensure_tee() {
	if [[ -n "$TEE_CACHE_BIN" && -s "$TEE_CACHE_BIN" ]]; then
		printf '[cix-release] Reusing cached OP-TEE from curated CIX V1.2 sources\n'
		ACTIVE_TEE_BIN="$TEE_CACHE_BIN"
		return
	fi

	printf '[cix-release] Building OP-TEE from curated CIX V1.2 sources\n'
	run make -C "$TEE_DIR" clean
	rm -rf "${TEE_DIR}/out" "${TEE_DIR}/tee.bin"

	local tee_env
	tee_env=(
		"PLATFORM=cix"
		"PLATFORM_FLAVOR=sky1"
		"TA_SIGN_KEY=${KEYS_DIR}/oem_privatekey.pem"
		"ARCH=arm"
		"CROSS_COMPILE64=${CROSS_COMPILE}"
		"CFG_ARM64_core=y"
		"CFG_USER_TA_TARGETS=ta_arm64"
	)
	if [[ -f "$STMM_PATH" ]]; then
		tee_env+=("CFG_STMM_PATH=${STMM_PATH}")
	fi
	run env "${tee_env[@]}" make -C "$TEE_DIR" "-j${JOBS}" all
	require_file "$TEE_OUTPUT"
	if [[ -n "$TEE_CACHE_BIN" ]]; then
		install_cache_file "$TEE_OUTPUT" "$TEE_CACHE_BIN"
		printf '[cix-release] Cached OP-TEE tee-raw.bin\n'
		ACTIVE_TEE_BIN="$TEE_CACHE_BIN"
		return
	fi
	ACTIVE_TEE_BIN="$TEE_OUTPUT"
}

TF_A_DEBUG=0
if [[ "$MODE" == "debug" ]]; then
	TF_A_DEBUG=1
fi

load_cache_plan
ensure_cert_create
ensure_bl31
ensure_tee

printf '[cix-release] Generating bootloader2 certificates\n'
ensure_cert_create
rm -f \
	"${CERTS_DIR}/trusted_key.crt" \
	"${CERTS_DIR}/bl31_fw_key.crt" \
	"${CERTS_DIR}/tos_fw_key.crt" \
	"${CERTS_DIR}/bl31_fw_content.crt" \
	"${CERTS_DIR}/tos_fw_cert.crt"

run "$ACTIVE_CERT_CREATE_BIN" \
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
	--soc-fw "$ACTIVE_BL31_BIN" \
	--tos-fw "$ACTIVE_TEE_BIN"

printf '[cix-release] Packaging bootloader2.img\n'
mkdir -p "$(dirname "$OUTPUT")"
rm -f "$OUTPUT"
run "$FIPTOOL" create \
	--soc-fw "$ACTIVE_BL31_BIN" \
	--tos-fw "$ACTIVE_TEE_BIN" \
	--trusted-key-cert "${CERTS_DIR}/trusted_key.crt" \
	--soc-fw-key-cert "${CERTS_DIR}/bl31_fw_key.crt" \
	--tos-fw-key-cert "${CERTS_DIR}/tos_fw_key.crt" \
	--soc-fw-cert "${CERTS_DIR}/bl31_fw_content.crt" \
	--tos-fw-cert "${CERTS_DIR}/tos_fw_cert.crt" \
	"$OUTPUT"

require_file "$OUTPUT"
