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
  version "1.1.3"
  depends_on :linux
  license "MIT"

  on_intel do
    url "https://github.com/adam-ikari/term-ime/releases/download/v1.1.3/term-ime-linux-x86_64.tar.gz"
    sha256 "1780154e61c506fa48b2b56f42048ae85c5a9de685edd04d83d31e0151e1a8eb"
  end

  on_arm do
    url "https://github.com/adam-ikari/term-ime/releases/download/v1.1.3/term-ime-linux-aarch64.tar.gz"
    sha256 "470c1abe458ea078bfee572e0d718c928a6b1e8d116107cbf1898f6ab7b952ff"
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
