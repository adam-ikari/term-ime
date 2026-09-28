# Homebrew formula for term-ime.
#
# Tap usage (once the homebrew-tap repo exists):
#   brew tap adam-ikari/tap
#   brew install term-ime
#
# The archive layout (term-ime/{bin,share,README.md}) mirrors install.sh's
# PREFIX layout (PREFIX/bin + PREFIX/share/term-ime/...), so the binary's
# data-path resolution (../share/term-ime/rime-data, ../share/term-ime/
# translations) works unchanged under Homebrew's prefix.
#
# Prebuilt binaries are fully static (ldd: "not a dynamic executable"), so
# this formula just unpacks them — no build deps.
#
# TODO(aarch64): add an `on_arm do ... end` stanza mirroring on_intel once
# the first release publishing term-ime-linux-aarch64.tar.gz ships (the
# release.yml matrix is ready; no ARM asset exists yet).
class TermIme < Formula
  desc "Linux TTY 终端输入法 — librime 拼音带进 SSH/Docker/WSL 纯终端"
  homepage "https://adam-ikari.github.io/term-ime/"
  version "1.1.1"
  depends_on :linux
  license "MIT"

  on_intel do
    url "https://github.com/adam-ikari/term-ime/releases/download/v1.1.1/term-ime-linux-x86_64.tar.gz"
    sha256 "8a0ffa50ffb909d1e6155fcf4d792516b3c5147358942a07847923922a1f6655"
  end

  def install
    # Replicates install.sh: binary to bin/, rime-data + translations to
    # share/term-ime/. `term-ime` kept as a compatibility alias for ti.
    bin.install "term-ime/bin/ti" => "ti"
    bin.install_symlink "ti" => "term-ime"
    prefix.install "term-ime/share"
  end

  test do
    assert_predicate bin/"ti", :executable?
    assert_predicate (share/"term-ime/rime-data"), :directory?
  end
end
