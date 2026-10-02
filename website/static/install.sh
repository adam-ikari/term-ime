#!/usr/bin/env bash
# term-ime installer — downloads a prebuilt binary from GitHub Releases.
#
# Fully static glibc build, zero runtime shared-library deps. x86_64 and aarch64.
#
# Usage (user install, no sudo):
#   curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash
#   curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash -s -- --version v1.0.0 --prefix ~/.local
#
# Usage (system install, requires sudo):
#   curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash -s -- --prefix /usr/local
#
# Defaults: latest release, prefix ~/.local (no sudo needed).
set -euo pipefail

REPO="adam-ikari/term-ime"
PREFIX="${HOME}/.local"
VERSION=""
SUDO=""

print_usage() {
    cat <<'EOF'
term-ime installer

Options:
  --version <tag>   Release tag to install (default: latest)
  --prefix <dir>    Install prefix (default: ~/.local — no sudo needed)
  -h, --help        Show this help

Install modes:
  User install (default):
    curl -fsSL .../install.sh | bash
    # Installs to ~/.local/bin/term-ime — no sudo needed

  System install:
    curl -fsSL .../install.sh | bash -s -- --prefix /usr/local
    # Installs to /usr/local/bin/term-ime — may need sudo
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        --prefix)  PREFIX="$2";  shift 2 ;;
        -h|--help) print_usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; print_usage; exit 1 ;;
    esac
done

# Pick an HTTP client up front, before anything needs the network.
#
# curl is the documented one, but wget is an equally good substitute and this
# script also runs on BusyBox-ish systems where only wget exists. Checking here
# beats letting it blow up later with a bare "curl: command not found" from
# inside a pipeline, where the real cause is invisible.
if command -v curl >/dev/null 2>&1; then
    fetch()    { curl -fsSL "$1"; }
    fetch_to() { curl -fsSL -o "$2" "$1"; }
elif command -v wget >/dev/null 2>&1; then
    fetch()    { wget -q -O - "$1"; }
    fetch_to() { wget -q -O "$2" "$1"; }
else
    echo "error: need curl or wget to download term-ime" >&2
    echo "     install one with your package manager, e.g.:" >&2
    echo "     apt install curl    # or: apk add curl / dnf install curl" >&2
    exit 1
fi

# Resolve latest version via the GitHub API if not pinned.
if [[ -z "$VERSION" ]]; then
    VERSION="$(fetch "https://api.github.com/repos/${REPO}/releases/latest" \
        | grep -m1 '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')"
    if [[ -z "$VERSION" ]]; then
        echo "error: could not determine latest release" >&2
        exit 1
    fi
fi
echo ">> Installing term-ime ${VERSION}"

# Detect arch. Prebuilt binaries: x86_64 and aarch64 (see release.yml matrix).
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64|amd64)   ASSET_ARCH="linux-x86_64" ;;
    aarch64|arm64)  ASSET_ARCH="linux-aarch64" ;;
    *) echo "error: unsupported architecture: $ARCH (prebuilt binaries are x86_64/aarch64 only; build from source for other archs)" >&2; exit 1 ;;
esac

ASSET="term-ime-${ASSET_ARCH}.tar.gz"
# TERM_IME_DOWNLOAD_BASE lets CI / mirrors point at any HTTP(S) base holding
# the asset + .sha256 (default: the GitHub release for this version).
DOWNLOAD_BASE="${TERM_IME_DOWNLOAD_BASE:-https://github.com/${REPO}/releases/download/${VERSION}}"
URL="${DOWNLOAD_BASE}/${ASSET}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo ">> Downloading ${URL}"
fetch_to "$URL" "${TMP}/${ASSET}"

# Verify the checksum. A failure here is fatal, but distinguish the two cases:
# the sidecar genuinely not existing (older/mirror) is a soft skip, while a
# network error is NOT — otherwise one flaky TLS read silently downgrades an
# integrity check to nothing, which is exactly when you least want it.
SHA_URL="${URL}.sha256"
SHA_ERR="${TMP}/.sha_err"
if fetch_to "$SHA_URL" "${TMP}/${ASSET}.sha256" 2>"$SHA_ERR"; then
    echo ">> Verifying checksum"
    (cd "$TMP" && sha256sum -c "${ASSET}.sha256" --status)
elif grep -qiE '404|not found' "$SHA_ERR" 2>/dev/null; then
    echo "!! No checksum sidecar published for this asset; skipping verification"
elif [ "${TERM_IME_ALLOW_UNVERIFIED:-0}" = "1" ]; then
    echo "!! Checksum fetch failed and TERM_IME_ALLOW_UNVERIFIED=1; skipping verification" >&2
    cat "$SHA_ERR" >&2 || true
else
    echo "error: could not fetch the checksum sidecar:" >&2
    cat "$SHA_ERR" >&2 || true
    echo "!! Refusing to install unverified. Retry, or set TERM_IME_ALLOW_UNVERIFIED=1 to override." >&2
    exit 1
fi

echo ">> Extracting"
tar -xzf "${TMP}/${ASSET}" -C "$TMP"

# Find the binary inside the extracted dir. Primary name is the short command `ti`.
BIN="$(find "$TMP" -type f -name ti -perm -u+x | head -1)"
if [[ -z "$BIN" ]]; then
    # Back-compat: older archives shipped the binary as `term-ime`.
    BIN="$(find "$TMP" -type f -name term-ime -perm -u+x | head -1)"
fi
if [[ -z "$BIN" ]]; then
    echo "error: binary not found in archive" >&2
    exit 1
fi

# Install binary.
mkdir -p "${PREFIX}/bin"

# Detect if we need sudo for the target prefix.
if [[ ! -w "${PREFIX}/bin" ]]; then
    SUDO="sudo"
fi

${SUDO} install -m 0755 "$BIN" "${PREFIX}/bin/ti"
# `term-ime` kept as a compatibility alias for scripts/docs that predate the short command.
${SUDO} ln -sf ti "${PREFIX}/bin/term-ime"

# Install rime-data (shared data: schemas + essay.txt + dict) and translations.
EXTRACT_ROOT="$(dirname "$(dirname "$BIN")")"
SHARED_SRC="${EXTRACT_ROOT}/share/term-ime/rime-data"
if [[ -d "$SHARED_SRC" ]]; then
    DATA_DEST="${PREFIX}/share/term-ime/rime-data"
    mkdir -p "$DATA_DEST"
    ${SUDO} cp -r "$SHARED_SRC"/* "$DATA_DEST/"
    echo ">> Installed rime-data to ${DATA_DEST}"
fi

# UI translation catalogs (data/translations/*.json). Without these the panel
# falls back to built-in strings and any newer key renders as its raw id.
TRANS_SRC="${EXTRACT_ROOT}/share/term-ime/translations"
if [[ -d "$TRANS_SRC" ]]; then
    TRANS_DEST="${PREFIX}/share/term-ime/translations"
    mkdir -p "$TRANS_DEST"
    ${SUDO} cp -r "$TRANS_SRC"/* "$TRANS_DEST/"
    echo ">> Installed translations to ${TRANS_DEST}"
fi

# Ensure the install prefix is on PATH; if not, append to the user's shell rc.
BIN_DIR="${PREFIX}/bin"
case ":${PATH}:" in
    *":${BIN_DIR}:"*) ;;
    *)
        RC=""
        if [[ -n "${ZSH_VERSION:-}" ]] || [[ "$SHELL" == */zsh ]]; then
            RC="${HOME}/.zshrc"
        elif [[ -n "${BASH_VERSION:-}" ]] || [[ "$SHELL" == */bash ]]; then
            RC="${HOME}/.bashrc"
        fi
        if [[ -n "$RC" ]]; then
            # One grouped append rather than three separate ones: a shell rc is
            # read by every future shell, so a partially-written line (interrupted
            # mid-install) is worse than the trivial cost of one open().
            {
                echo ""
                echo "# term-ime"
                echo "export PATH=\"${BIN_DIR}:\$PATH\""
            } >> "$RC"
            echo ">> Added ${BIN_DIR} to PATH in ${RC}"
            echo ">> Start a new shell or run: source ${RC}"
        else
            echo ">> Add to PATH manually: export PATH=\"${BIN_DIR}:\$PATH\""
        fi
        ;;
esac

INSTALLED="${BIN_DIR}/ti"
if [[ ":${PATH}:" == *":${BIN_DIR}:"* ]]; then INSTALLED="ti"; fi
echo ">> Installed: ${INSTALLED} (alias: term-ime)"

# Verify the installed binary matches the platform it was built for: the Linux
# package is fully static, so a dynamic dependency means something went wrong.
echo ">> Verifying binary..."
if ! command -v file >/dev/null 2>&1; then
    echo ">> 'file' not installed; skipping the static-link check"
else
    if ! file "${PREFIX}/bin/ti" | grep -q "statically linked"; then
        echo "!! WARNING: Binary does not appear to be fully statically linked."
        echo "!! This may indicate a build issue. Please report at:"
        echo "!! https://github.com/${REPO}/issues"
        echo "!!"
        echo "!! Dynamic dependencies detected:"
        ldd "${PREFIX}/bin/ti" 2>/dev/null || true
    fi
fi

echo ">> Run: ti"
