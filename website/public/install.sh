#!/usr/bin/env bash
# term-ime installer — downloads a prebuilt binary from GitHub Releases.
#
# Usage:
#   curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash
#   curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash -s -- --version v1.0.0 --prefix ~/.local
#
# Defaults: latest release, prefix /usr/local (needs sudo).
set -euo pipefail

REPO="adam-ikari/term-ime"
PREFIX="${HOME}/.local"
VERSION=""

print_usage() {
    cat <<'EOF'
term-ime installer

Options:
  --version <tag>   Release tag to install (default: latest)
  --prefix <dir>    Install prefix (default: ~/.local — no sudo needed)
  -h, --help        Show this help
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

# Resolve latest version via the GitHub API if not pinned.
if [[ -z "$VERSION" ]]; then
    VERSION="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" \
        | grep -m1 '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')"
    if [[ -z "$VERSION" ]]; then
        echo "error: could not determine latest release" >&2
        exit 1
    fi
fi
echo ">> Installing term-ime ${VERSION}"

# Detect arch.
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64|amd64) ASSET_ARCH="linux-x86_64" ;;
    aarch64|arm64) ASSET_ARCH="linux-arm64" ;;
    *) echo "error: unsupported architecture: $ARCH" >&2; exit 1 ;;
esac

ASSET="term-ime-${ASSET_ARCH}.tar.gz"
URL="https://github.com/${REPO}/releases/download/${VERSION}/${ASSET}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo ">> Downloading ${URL}"
curl -fsSL -o "${TMP}/${ASSET}" "$URL"

# Verify checksum if a .sha256 sidecar exists.
SHA_URL="${URL}.sha256"
if curl -fsSL -o "${TMP}/${ASSET}.sha256" "$SHA_URL"; then
    echo ">> Verifying checksum"
    (cd "$TMP" && sha256sum -c "${ASSET}.sha256" --status)
else
    echo "!! No checksum sidecar found; skipping verification"
fi

echo ">> Extracting"
tar -xzf "${TMP}/${ASSET}" -C "$TMP"

# Find the binary inside the extracted dir.
BIN="$(find "$TMP" -type f -name term-ime -perm -u+x | head -1)"
if [[ -z "$BIN" ]]; then
    echo "error: term-ime binary not found in archive" >&2
    exit 1
fi

# Install.
mkdir -p "${PREFIX}/bin"
if [[ -w "${PREFIX}/bin" ]]; then
    install -m 0755 "$BIN" "${PREFIX}/bin/term-ime"
else
    echo ">> ${PREFIX}/bin not writable; using sudo"
    sudo install -m 0755 "$BIN" "${PREFIX}/bin/term-ime"
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
            echo "" >> "$RC"
            echo "# term-ime" >> "$RC"
            echo "export PATH=\"${BIN_DIR}:\$PATH\"" >> "$RC"
            echo ">> Added ${BIN_DIR} to PATH in ${RC}"
            echo ">> Start a new shell or run: source ${RC}"
        else
            echo ">> Add to PATH manually: export PATH=\"${BIN_DIR}:\$PATH\""
        fi
        ;;
esac

INSTALLED="${BIN_DIR}/term-ime"
if [[ ":${PATH}:" == *":${BIN_DIR}:"* ]]; then INSTALLED="term-ime"; fi
echo ">> Installed: ${INSTALLED}"
echo ">> Run: term-ime"
