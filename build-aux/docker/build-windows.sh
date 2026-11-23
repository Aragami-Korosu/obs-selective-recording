#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Builds, checks and packages the Windows plugin and installer inside a throwaway Debian
# container. Start it with build-aux/docker/build.ps1; see docs/BUILDING.md.
#
# The plugin is cross-compiled with clang-cl and lld-link against the same pinned OBS 31.1.1
# headers and obs-deps Qt 6 that the CMake build uses. The installer is built with Inno Setup
# under Wine, then installed and uninstalled under Wine as a smoke test.
#
# Mounts:
#   /src      the repository (read-only)
#   /cache    downloads, kept between runs
#   /out      results
#   /signing  optional, read-only: signing.pfx and pfx-password.txt

set -euo pipefail
trap 'printf "error: line %s: %s\n" "$LINENO" "$BASH_COMMAND" >&2' ERR

readonly SRC=/src CACHE=/cache OUT=/out SIGNING=/signing
readonly WORK=/tmp/work
readonly XWIN_VERSION=0.10.0
readonly XWIN_SHA256=d870eb4b2f390878af6da1ccd3cf321d22fcb72720984853b4be732ae597fc88
readonly OBS_BIN_SHA256=9d8dceb77acd8af04af23f877061f63c9bef78ca73d2093d0ccba1bb9104173f
readonly INNO_VERSION=6.7.4
readonly INNO_SHA256=a549929319f9120716677c685535ca4671e4b937b5fdc87e2585f32d8eb30e8c
readonly OSSLSIGNCODE_VERSION=2.16
readonly OSSLSIGNCODE_SHA256=125903750deb7b633c57a81fe3c6ae945a271c30e382b9413a2af4e4d6248ee3
readonly GERSEMI_VERSION=0.21.0 # the version obsproject/tools installs for the template's checks

step() { printf '\n==> %s\n' "$*"; }
fail() { printf 'error: %s\n' "$*" >&2; exit 1; }

# fetch URL SHA256 FILE: downloads into the cache once and checks the checksum every time.
fetch() {
  local url=$1 sha=$2 file=$CACHE/downloads/$3
  mkdir -p "$CACHE/downloads"
  if [[ ! -f $file ]] || ! echo "$sha  $file" | sha256sum -c --status; then
    curl -fsSL --retry 3 -o "$file.part" "$url"
    mv "$file.part" "$file"
  fi
  echo "$sha  $file" | sha256sum -c --status || fail "checksum mismatch for $url"
  echo "$file"
}

# json FILE KEY...: prints a value from a JSON file.
json() {
  python3 -c 'import functools, json, sys
print(functools.reduce(lambda d, k: d[k], sys.argv[2:], json.load(open(sys.argv[1]))))' "$@"
}

step "Installing tools"
export DEBIAN_FRONTEND=noninteractive
rm -f /etc/apt/apt.conf.d/docker-clean # keep packages in the cache mount
mkdir -p "$CACHE/apt/partial"
dpkg --add-architecture i386
apt-get -o Dir::Cache::archives="$CACHE/apt" update -qq
apt-get -o Dir::Cache::archives="$CACHE/apt" install -y -qq --no-install-recommends \
  ca-certificates curl unzip zip xz-utils python3 python3-venv \
  clang-19 lld-19 llvm-19 clang-format-19 qt6-base-dev-tools \
  wine wine64 wine32:i386 xvfb xauth >/dev/null
export PATH="/usr/lib/llvm-19/bin:$PATH"

SPEC=$SRC/buildspec.json
NAME=$(json "$SPEC" name)
VERSION=$(json "$SPEC" version)
AUTHOR=$(json "$SPEC" author)
OBS_VERSION=$(json "$SPEC" dependencies obs-studio version)
OBS_SRC_SHA256=$(json "$SPEC" dependencies obs-studio hashes windows-x64)
QT_VERSION=$(json "$SPEC" dependencies qt6 version)
QT_SHA256=$(json "$SPEC" dependencies qt6 hashes windows-x64)
OUTPUT_NAME="$NAME-$VERSION-windows-x64"
rm -rf "$WORK" && mkdir -p "$WORK"
cd "$SRC"

step "Checking formatting"
mapfile -t cxx_files < <(find src -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \))
clang-format-19 --dry-run --Werror "${cxx_files[@]}"
python3 -m venv "$WORK/venv"
"$WORK/venv/bin/pip" install -q "gersemi==$GERSEMI_VERSION"
mapfile -t cmake_files < <(find CMakeLists.txt cmake -type f \( -name 'CMakeLists.txt' -o -name '*.cmake' \))
"$WORK/venv/bin/gersemi" --check "${cmake_files[@]}"
echo "formatting ok"

step "Fetching dependencies"
OBS_SRC_ZIP=$(fetch "https://github.com/obsproject/obs-studio/archive/refs/tags/$OBS_VERSION.zip" "$OBS_SRC_SHA256" "obs-studio-$OBS_VERSION.zip")
QT_ZIP=$(fetch "https://github.com/obsproject/obs-deps/releases/download/$QT_VERSION/windows-deps-qt6-$QT_VERSION-x64.zip" "$QT_SHA256" "windows-deps-qt6-$QT_VERSION-x64.zip")
OBS_BIN_ZIP=$(fetch "https://github.com/obsproject/obs-studio/releases/download/$OBS_VERSION/OBS-Studio-$OBS_VERSION-Windows-x64.zip" "$OBS_BIN_SHA256" "OBS-Studio-$OBS_VERSION-Windows-x64.zip")
XWIN_TGZ=$(fetch "https://github.com/Jake-Shadle/xwin/releases/download/$XWIN_VERSION/xwin-$XWIN_VERSION-x86_64-unknown-linux-musl.tar.gz" "$XWIN_SHA256" "xwin-$XWIN_VERSION.tar.gz")
INNO_EXE=$(fetch "https://github.com/jrsoftware/issrc/releases/download/is-${INNO_VERSION//./_}/innosetup-$INNO_VERSION.exe" "$INNO_SHA256" "innosetup-$INNO_VERSION.exe")
SIGN_ZIP=$(fetch "https://github.com/mtrojnar/osslsigncode/releases/download/$OSSLSIGNCODE_VERSION/osslsigncode-$OSSLSIGNCODE_VERSION-windows-amd64.zip" "$OSSLSIGNCODE_SHA256" "osslsigncode-$OSSLSIGNCODE_VERSION-windows-amd64.zip")

VERTICAL_JSON="$SRC/packaging/windows/bundled-vertical-canvas.json"
VERTICAL_VERSION=$(json "$VERTICAL_JSON" version)
VERTICAL_ZIP=$(fetch "$(json "$VERTICAL_JSON" package url)" "$(json "$VERTICAL_JSON" package sha256)" "vertical-canvas-$VERTICAL_VERSION-windows-programdata.zip")
VERTICAL_SRC=$(fetch "$(json "$VERTICAL_JSON" source url)" "$(json "$VERTICAL_JSON" source sha256)" "vertical-canvas-$VERTICAL_VERSION-source.tar.xz")

# The MSVC C/C++ runtime and Windows SDK headers and libraries, downloaded from Microsoft. They
# are unpacked inside the container, where the file system is case-sensitive and xwin can add
# lower-case links for headers that Windows code includes in any case; only the downloads are
# cached.
step "Preparing the MSVC runtime and Windows SDK (accepting Microsoft's license)"
XWIN=$WORK/xwin
tar -xzf "$XWIN_TGZ" -C "$WORK"
mkdir -p "$CACHE/xwin-dl" "$WORK/xwin-cache/dl"
cp -r "$CACHE/xwin-dl/." "$WORK/xwin-cache/dl/"
# xwin unpacks in parallel and occasionally trips over its own files ("File exists"); retry once.
splat() {
  rm -rf "$XWIN" "$WORK/xwin-cache/unpack"
  "$WORK/xwin-$XWIN_VERSION-x86_64-unknown-linux-musl/xwin" --accept-license --log-level warn \
    --cache-dir "$WORK/xwin-cache" splat --output "$XWIN"
}
splat || splat
cp -rn "$WORK/xwin-cache/dl/." "$CACHE/xwin-dl/"

step "Preparing OBS and Qt"
DEPS=$WORK/deps
mkdir -p "$DEPS/obs" "$DEPS/qt" "$DEPS/obs-bin" "$DEPS/stub"
unzip -q "$OBS_SRC_ZIP" "obs-studio-$OBS_VERSION/libobs/*" "obs-studio-$OBS_VERSION/frontend/api/*" -d "$DEPS/obs"
OBS_SRC_DIR=$DEPS/obs/obs-studio-$OBS_VERSION
unzip -q "$QT_ZIP" 'include/*' 'lib/*.lib' -d "$DEPS/qt"
unzip -q -j "$OBS_BIN_ZIP" 'bin/64bit/obs.dll' 'bin/64bit/obs-frontend-api.dll' -d "$DEPS/obs-bin"
cat >"$DEPS/stub/obsconfig.h" <<'EOF'
#pragma once
#define OBS_RELEASE_CANDIDATE 0
#define OBS_BETA 0
EOF
# libobs's util_uint64.h uses MSVC's _udiv128 intrinsic, which clang-cl 19 doesn't provide.
# This is the same single 128-by-64-bit division instruction MSVC emits for it.
cat >"$DEPS/stub/clang-cl-compat.h" <<'EOF'
#pragma once
#if defined(__clang__) && defined(_M_X64) && !__has_builtin(_udiv128)
static __inline unsigned __int64 _udiv128(unsigned __int64 high, unsigned __int64 low, unsigned __int64 divisor,
					  unsigned __int64 *remainder)
{
	unsigned __int64 quotient, rest;
	__asm__("divq %4" : "=a"(quotient), "=d"(rest) : "a"(low), "d"(high), "r"(divisor));
	*remainder = rest;
	return quotient;
}
#endif
EOF
# Import libraries for libobs and the frontend API, made from the released DLLs' export tables.
for lib in obs obs-frontend-api; do
  {
    echo "LIBRARY $lib.dll"
    echo "EXPORTS"
    llvm-readobj --coff-exports "$DEPS/obs-bin/$lib.dll" | awk '$1 == "Name:" && NF == 2 { print $2 }'
  } >"$DEPS/$lib.def"
  llvm-dlltool -m i386:x86-64 -d "$DEPS/$lib.def" -l "$DEPS/$lib.lib"
done

step "Compiling $NAME $VERSION"
BUILD=$WORK/build
mkdir -p "$BUILD/obj" "$BUILD/gen"
QT=$DEPS/qt
DEFINES=(
  -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -DNDEBUG
  -DWIN32 -D_WINDOWS -DWIN64 -D_WIN64 -D_ENABLE_EXTENDED_ALIGNED_STORAGE
  -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_NO_DEBUG
)
INCLUDES=(
  "-I$SRC/src" "-I$BUILD/gen"
  -imsvc "$XWIN/crt/include" -imsvc "$XWIN/sdk/include/ucrt" -imsvc "$XWIN/sdk/include/um"
  -imsvc "$XWIN/sdk/include/shared" -imsvc "$XWIN/sdk/include/winrt"
  -imsvc "$SRC/third-party/obs-websocket" -imsvc "$DEPS/stub" -imsvc "$OBS_SRC_DIR/libobs"
  -imsvc "$OBS_SRC_DIR/frontend/api" -imsvc "$QT/include" -imsvc "$QT/include/QtCore"
  -imsvc "$QT/include/QtGui" -imsvc "$QT/include/QtWidgets"
)
# MSVC options from cmake/windows/compilerconfig.cmake (RelWithDebInfo), plus the clang warning
# set the macOS and Linux builds treat as errors.
CFLAGS=(
  # In clang-cl, /W3 is clang's -Wall (and -Wall would mean -Weverything).
  --target=x86_64-pc-windows-msvc /nologo /c /MD /O2 /Ob1 /Zi /Gy /Oi /W3 /WX /utf-8 /Brepro
  -Wextra -Wno-missing-field-initializers -Wno-unknown-pragmas -Wno-missing-braces
  -Wunreachable-code -Wunreachable-code-loop-increment -Wsign-compare -Wshorten-64-to-32 -Wcomma
  -Wrange-loop-analysis -Wnewline-eof -Wconstant-conversion -Wbool-conversion -Wenum-conversion
  -Winfinite-recursion -Wmove -Wformat-security -Wvla -Wunused-parameter -Wunused-variable
  -Wno-unused-command-line-argument -Wno-microsoft-include -Wno-nonportable-include-path
  "/FI$DEPS/stub/clang-cl-compat.h"
)
CXXFLAGS=("${CFLAGS[@]}" /std:c++17 /EHsc /permissive- /Zc:__cplusplus)

# Qt's meta-object code for every class with Q_OBJECT.
mapfile -t moc_headers < <(grep -rl 'Q_OBJECT' src --include='*.hpp')
for header in "${moc_headers[@]}"; do
  /usr/lib/qt6/libexec/moc "${DEFINES[@]}" -D_WIN32 "-I$SRC/src" "$SRC/$header" -o "$BUILD/gen/moc_$(basename "${header%.hpp}").cpp"
done

sed -e "s/@CMAKE_PROJECT_NAME@/$NAME/" -e "s/@CMAKE_PROJECT_VERSION@/$VERSION/" \
  src/plugin-support.c.in >"$BUILD/gen/plugin-support.c"

CLANG_CL=(clang-19 --driver-mode=cl)
compile() {
  local source=$1 object
  object=$BUILD/obj/$(echo "${source#"$SRC"/}" | tr '/' '_').obj
  if [[ $source == *.c ]]; then
    "${CLANG_CL[@]}" "${CFLAGS[@]}" /TC "${DEFINES[@]}" "${INCLUDES[@]}" "/Fo$object" "$source"
  else
    "${CLANG_CL[@]}" "${CXXFLAGS[@]}" "${DEFINES[@]}" "${INCLUDES[@]}" "/Fo$object" "$source"
  fi
}
mapfile -t sources < <(find "$SRC/src" -name '*.cpp'; find "$BUILD/gen" -name '*.cpp' -o -name '*.c')
failed=0
jobs_max=$(nproc)
for ((i = 0; i < ${#sources[@]}; i += jobs_max)); do
  pids=()
  for source in "${sources[@]:i:jobs_max}"; do
    compile "$source" &
    pids+=($!)
  done
  for pid in "${pids[@]}"; do wait "$pid" || failed=1; done
done
((failed == 0)) || fail "compilation failed"

# Version resource, as cmake/windows/resources/resource.rc.in.
IFS=. read -r v_major v_minor v_patch <<<"$VERSION"
sed -e "s/\${PROJECT_VERSION_MAJOR}/$v_major/g" -e "s/\${PROJECT_VERSION_MINOR}/$v_minor/g" \
  -e "s/\${PROJECT_VERSION_PATCH}/$v_patch/g" -e "s/\${PROJECT_VERSION}/$VERSION/g" \
  -e "s/\${PROJECT_NAME}/$NAME/g" -e "s/\${PLUGIN_AUTHOR}/$AUTHOR/g" -e "s/\${CURRENT_YEAR}/$(date +%Y)/g" \
  cmake/windows/resources/resource.rc.in >"$BUILD/gen/$NAME.rc"
llvm-rc /FO "$BUILD/obj/$NAME.res" "$BUILD/gen/$NAME.rc"

step "Linking"
lld-link /nologo /DLL /MACHINE:X64 /DEBUG /OPT:REF /OPT:ICF /INCREMENTAL:NO /Brepro \
  "/OUT:$BUILD/$NAME.dll" "/PDB:$BUILD/$NAME.pdb" \
  "/LIBPATH:$XWIN/crt/lib/x86_64" "/LIBPATH:$XWIN/sdk/lib/um/x86_64" "/LIBPATH:$XWIN/sdk/lib/ucrt/x86_64" \
  "$BUILD"/obj/*.obj "$BUILD/obj/$NAME.res" \
  "$DEPS/obs.lib" "$DEPS/obs-frontend-api.lib" \
  "$QT/lib/Qt6Core.lib" "$QT/lib/Qt6Gui.lib" "$QT/lib/Qt6Widgets.lib" kernel32.lib user32.lib

step "Packaging"
RELEASE=$OUT/release
PLUGIN=$RELEASE/RelWithDebInfo/$NAME
rm -rf "$RELEASE" && mkdir -p "$PLUGIN/bin/64bit"
cp "$BUILD/$NAME.dll" "$BUILD/$NAME.pdb" "$PLUGIN/bin/64bit/"
cp -r data "$PLUGIN/data"

# Signing is optional: it runs when /signing has signing.pfx and pfx-password.txt.
export WINEPREFIX=$WORK/wine WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
SIGN_CMD=()
if [[ -f $SIGNING/signing.pfx && -f $SIGNING/pfx-password.txt ]]; then
  unzip -q -o "$SIGN_ZIP" -d "$WORK/osslsigncode"
  SIGN_EXE=$(find "$WORK/osslsigncode" -name osslsigncode.exe | head -n1)
  tr -d '\r\n' <"$SIGNING/pfx-password.txt" >"$WORK/pfx-password"
  SIGN_CMD=(sign -pkcs12 "Z:$SIGNING/signing.pfx" -readpass "Z:$WORK/pfx-password" -h sha256
    -t http://timestamp.digicert.com)
fi
sign() { # sign FILE (in place)
  wine "$SIGN_EXE" "${SIGN_CMD[@]}" -in "Z:$1" -out "Z:$1.signed" >/dev/null
  mv "$1.signed" "$1"
}

xvfb-run -a wineboot --init >/dev/null 2>&1
if ((${#SIGN_CMD[@]})); then
  step "Signing $NAME.dll"
  sign "$PLUGIN/bin/64bit/$NAME.dll"
fi

(cd "$RELEASE/RelWithDebInfo" && zip -qr "$RELEASE/$OUTPUT_NAME.zip" "$NAME")

step "Building the installer"
VERTICAL_WORK=$WORK/vertical
mkdir -p "$VERTICAL_WORK"
unzip -q "$VERTICAL_ZIP" -d "$VERTICAL_WORK/package"
VERTICAL_DIR=$VERTICAL_WORK/package/vertical-canvas
[[ -f $VERTICAL_DIR/bin/64bit/vertical-canvas.dll ]] || fail "unexpected Vertical Canvas package layout"
tar -xJf "$VERTICAL_SRC" -C "$VERTICAL_WORK" "vertical-canvas-$VERTICAL_VERSION-source/LICENSE"
cp "$VERTICAL_WORK/vertical-canvas-$VERTICAL_VERSION-source/LICENSE" "$VERTICAL_DIR/"
cp "$VERTICAL_SRC" "$RELEASE/$OUTPUT_NAME-vertical-canvas-$VERTICAL_VERSION-source.tar.xz"
VERTICAL_COMPONENTS=$(echo "$VERTICAL_VERSION.0" | cut -d. -f1-4 | tr . ,)

xvfb-run -a wine "$INNO_EXE" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER '/DIR=C:\inno' >/dev/null 2>&1
ISCC_ARGS=(
  /Q "/DProductVersion=$VERSION" "/DSourceDir=Z:$PLUGIN" "/DVerticalDir=Z:$VERTICAL_DIR"
  "/DVerticalVersion=$VERTICAL_VERSION" "/DVerticalVersionComponents=$VERTICAL_COMPONENTS"
  "/DOutputDir=Z:$RELEASE" "/DOutputName=$OUTPUT_NAME-installer"
)
if ((${#SIGN_CMD[@]})); then
  # Inno Setup signs the uninstaller while it builds, so it calls the signing tool itself.
  cat >"$WORK/sign.bat" <<EOF
@echo off
"Z:$SIGN_EXE" ${SIGN_CMD[*]} -in "%~1" -out "%~1.signed" >nul || exit /b 1
move /y "%~1.signed" "%~1" >nul
EOF
  ISCC_ARGS=(/DSign "/Ssigntool=cmd /c Z:$WORK/sign.bat \$f" "${ISCC_ARGS[@]}")
fi
xvfb-run -a wine 'C:\inno\ISCC.exe' "${ISCC_ARGS[@]}" "Z:$SRC/packaging/windows/installer.iss"
INSTALLER=$RELEASE/$OUTPUT_NAME-installer.exe
[[ -f $INSTALLER ]] || fail "the installer wasn't built"

step "Testing the installer"
# A clean Wine prefix has no OBS or Vertical, so a silent install must install both, and
# uninstalling must remove the plugin but keep Vertical.
export WINEPREFIX=$WORK/wine-test
xvfb-run -a wineboot --init >/dev/null 2>&1
TEST_ROOT=$WINEPREFIX/drive_c/test/plugins
mkdir -p "$WINEPREFIX/drive_c/test" # Inno Setup won't create the /LOG folder
xvfb-run -a wine "Z:$INSTALLER" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER \
  "/DIR=C:\\test\\plugins\\$NAME" '/LOG=C:\test\install.log' >/dev/null 2>&1 || true
for f in "$TEST_ROOT/$NAME/bin/64bit/$NAME.dll" "$TEST_ROOT/$NAME/data/locale/en-US.ini" \
  "$TEST_ROOT/vertical-canvas/bin/64bit/vertical-canvas.dll" "$TEST_ROOT/vertical-canvas/LICENSE"; do
  [[ -f $f ]] || { cat "$WINEPREFIX/drive_c/test/install.log" 2>/dev/null || true; fail "install test: missing $f"; }
done
cmp -s "$TEST_ROOT/$NAME/bin/64bit/$NAME.dll" "$PLUGIN/bin/64bit/$NAME.dll" || fail "install test: DLL differs"
xvfb-run -a wine "C:\\test\\plugins\\$NAME\\unins000.exe" /VERYSILENT /SUPPRESSMSGBOXES >/dev/null 2>&1 || true
for _ in $(seq 30); do [[ -d $TEST_ROOT/$NAME ]] || break; sleep 1; done
[[ ! -d $TEST_ROOT/$NAME ]] || fail "uninstall test: the plugin folder is still there"
[[ -f $TEST_ROOT/vertical-canvas/bin/64bit/vertical-canvas.dll ]] || fail "uninstall test: Vertical was removed"
echo "installer ok"

step "Done"
(cd "$RELEASE" && sha256sum ./*.zip ./*.exe ./*.tar.xz | tee "$RELEASE/SHA256SUMS.txt")
