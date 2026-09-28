# Homebrew formula for term-ime.
#
#   brew tap adam-ikari/tap
#   brew install term-ime
#
# The archive layout (term-ime/{bin,share,README.md}) mirrors install.sh's
# PREFIX layout (PREFIX/bin + PREFIX/share/term-ime/...), so the binary's
# data-path resolution (../share/term-ime/rime-data, ../share/term-ime/
# translations) works unchanged under Homebrew's prefix.
#
# Prebuilt binaries are fully static (ldd: "not a dynamic executable"), so
# this formula just unpacks them — no build deps. x86_64 + aarch64 both
# published since v1.1.2 (release.yml matrix).
class TermIme < Formula
  desc "Linux TTY 终端输入法 — librime 拼音带进 SSH/Docker/WSL 纯终端"
  homepage "https://adam-ikari.github.io/term-ime/"
  version "1.1.2"
  depends_on :linux
  license "MIT"

  on_intel do
    url "https://github.com/adam-ikari/term-ime/releases/download/v1.1.2/term-ime-linux-x86_64.tar.gz"
    sha256 "a2a5c83304fd6cddc79047f66a67f7a042a3226fc25067bc09c2289903ef74b7"
  end

  on_arm do
    url "https://github.com/adam-ikari/term-ime/releases/download/v1.1.2/term-ime-linux-aarch64.tar.gz"
    sha256 "100af48382e05efab224d0f870a32fa2817968de6b2d4302b5604287d4fe6152"
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
