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
  version "1.1.6"
  depends_on :linux
  license "MIT"

  on_intel do
    url "https://github.com/adam-ikari/term-ime/releases/download/v1.1.6/term-ime-linux-x86_64.tar.gz"
    sha256 "5c9292087a4a69ce539ad6837e67d1ee477fa5b8765489fcc51c6bd2b6933737"
  end

  on_arm do
    url "https://github.com/adam-ikari/term-ime/releases/download/v1.1.6/term-ime-linux-aarch64.tar.gz"
    sha256 "08dc8451414ce224468b31d5c177cd2421974370b6550fa00cffe07d05db1dd4"
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
    # Runtime discovery decides ownership of a data dir by this schema, so a
    # bundle that merely has the directory still installs clean and then offers
    # no candidates.
    assert_predicate (share/"term-ime/rime-data/luna_pinyin_simp_fuzzy.schema.yaml"), :exist?
  end
end
