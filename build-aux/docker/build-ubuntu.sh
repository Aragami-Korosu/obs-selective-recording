#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
export CI=true
export CPUTYPE=x86_64
export CCACHE_DIR=/cache/ccache

mkdir -p /work/source /cache/apt/archives/partial /cache/ccache /out
printf 'Dir::Cache::archives "/cache/apt/archives";\n' > /etc/apt/apt.conf.d/02docker-cache

apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates \
  file \
  git \
  software-properties-common \
  sudo \
  tar \
  xz-utils \
  zsh

tar -C /src \
  --exclude='./.deps' \
  --exclude='./build_*' \
  --exclude='./release' \
  -cf - . | tar -C /work/source -xf -

cd /work/source
zsh .github/scripts/build-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo
zsh .github/scripts/package-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo --skip-deps --package

mapfile -d '' artifacts < <(
  find /work/source/release -maxdepth 1 -type f \
    \( -name '*.deb' -o -name '*.ddeb' -o -name '*-source.tar.xz' \) \
    -print0
)
if (( ${#artifacts[@]} == 0 )); then
  echo 'Ubuntu packaging completed without producing Debian or source artifacts' >&2
  exit 1
fi

cp -- "${artifacts[@]}" /out/
deb_package=$(find /out -maxdepth 1 -type f -name '*.deb' ! -name '*dbgsym*' -print -quit)
if [[ -z "$deb_package" ]]; then
  echo 'Ubuntu packaging completed without producing an installable .deb package' >&2
  exit 1
fi

dpkg -i "$deb_package"
dpkg-query -W -f='${Status}\n' obs-selective-recording | grep -Fxq 'install ok installed'
dpkg -L obs-selective-recording | grep -Eq '/obs-plugins/obs-selective-recording\.so$'
printf 'Ubuntu .deb install smoke test passed\n'

(cd /out && find . -maxdepth 1 -type f \
  \( -name '*.deb' -o -name '*.ddeb' -o -name '*-source.tar.xz' \) \
  -print0 | xargs -0 sha256sum > SHA256SUMS.txt)
printf 'Ubuntu packages and checksums are available in /out\n'
