#!/usr/bin/env bash
# Assemble LICENSES/ from the vendored deps. Every file is a verbatim copy of the
# upstream license text (or of the license header embedded in a header-only dep),
# so the shipped text is auditable against its source instead of retyped.
#
# Two components have no standalone license file: rapidjson and utf8-cpp ship the
# grant only inside a header, and X11 keysymdef.h likewise. Those are sliced out of
# the named headers by exact line range -- if the range drifts, the slice stops
# containing the expected marker and the script fails rather than shipping a
# truncated license.
#
# LGPL-3.0.txt is checked in rather than regenerated: rime-essay's data is LGPL-3.0
# and no dep in this tree carries that text, so it cannot be derived from the build
# environment (a runner without /usr/share/common-licenses must still ship it).
set -euo pipefail

cd "$(dirname "$0")/.."
OUT=LICENSES
mkdir -p "$OUT"

copy() { # copy <source> <outfile>
    [ -f "$1" ] || { echo "missing source: $1" >&2; exit 1; }
    cp "$1" "$OUT/$2"
}

# Verifies the slice really is the license block: the caller names a line that
# must appear in it, so a drifted line range fails instead of silently shipping
# a truncated license.
slice() { # slice <source> <first> <last> <outfile> <must-contain>
    [ -f "$1" ] || { echo "missing source: $1" >&2; exit 1; }
    sed -n "$2,$3p" "$1" > "$OUT/$4"
    grep -qF "$5" "$OUT/$4" || {
        echo "slice of $1 drifted: $OUT/$4 does not contain '$5'" >&2
        exit 1
    }
}

# LGPL-3.0.txt survives the rebuild (see header): everything else is regenerated
# from the deps so a license bump cannot leave a stale text behind.
tmp_lgpl=$(mktemp)
[ -f "$OUT/LGPL-3.0.txt" ] || { echo "LICENSES/LGPL-3.0.txt is checked in; do not delete it" >&2; exit 1; }
cp "$OUT/LGPL-3.0.txt" "$tmp_lgpl"
rm -f "$OUT"/*.txt
mv "$tmp_lgpl" "$OUT/LGPL-3.0.txt"

# --- statically linked into the binary ---------------------------------------
copy deps/librime/LICENSE                                   librime-BSD-3-Clause.txt
copy deps/librime/deps/opencc/LICENSE                       opencc-Apache-2.0.txt
copy deps/librime/deps/opencc/deps/marisa-0.2.6/COPYING.md  marisa-BSD-2-Clause.txt
copy deps/librime/include/COPYING.darts-clone               darts-clone-BSD-2-Clause.txt
copy deps/utf8proc/LICENSE.md                               utf8proc-MIT.txt
copy deps/ftxui/LICENSE                                     ftxui-MIT.txt
copy deps/spdlog/LICENSE                                    spdlog-MIT.txt
copy deps/json/LICENSE.MIT                                  nlohmann-json-MIT.txt
copy deps/libuv/LICENSE                                     libuv-MIT.txt
copy deps/libuv/LICENSE-extra                               libuv-extra.txt
copy deps/sml/LICENSE.md                                    boost-sml-BSL-1.0.txt
copy deps/librime/deps/yaml-cpp/LICENSE                     yaml-cpp-MIT.txt
copy deps/librime/deps/leveldb/LICENSE                      leveldb-BSD-3-Clause.txt
slice deps/librime/deps/opencc/deps/rapidjson-1.1.0/rapidjson/rapidjson.h \
      1 13 rapidjson-MIT.txt "and limitations under the License"
slice deps/librime/include/utf8.h \
      1 25 utf8-cpp-BSL-1.0.txt "DEALINGS IN THE SOFTWARE."
slice deps/librime/include/X11/keysymdef.h \
      1 47 X11-keysymdef.txt "*****/"
# --- shipped rime data --------------------------------------------------------
copy deps/librime/dict/LICENSE                              rime-dict-data.txt
copy deps/json/LICENSES/GPL-3.0-only.txt                    GPL-3.0.txt
copy deps/json/LICENSES/GPL-3.0-only.txt                    GPL-3.0.txt

echo "wrote $(find "$OUT" -type f | wc -l) files into $OUT/"
