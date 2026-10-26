set -euo pipefail

tool_root="$(mktemp -d)"
mkdir -p "$tool_root/bin"
git clone --quiet https://github.com/diasurgical/bannertool.git "$tool_root/bannertool"
git -C "$tool_root/bannertool" checkout --quiet --detach 16d8c5a0ce02a5e06e64ab42275132fca57c04a2
git -C "$tool_root/bannertool" submodule update --init --recursive
make -C "$tool_root/bannertool" -j2
install -m755 "$tool_root/bannertool/output/linux-x86_64/bannertool" "$tool_root/bin/bannertool"

git clone --quiet --depth 1 --branch makerom-v0.18.4 https://github.com/3DSGuy/Project_CTR.git "$tool_root/Project_CTR"
test "$(git -C "$tool_root/Project_CTR" rev-parse HEAD)" = c0488dcb6c3048e6a519716f2b21e2c65859ca75
make -C "$tool_root/Project_CTR/makerom" deps -j2
make -C "$tool_root/Project_CTR/makerom" program -j2
install -m755 "$tool_root/Project_CTR/makerom/bin/makerom" "$tool_root/bin/makerom"

export PATH="$tool_root/bin:$PATH"
python3 tools/localization/check-japanese.py
make test
g++ -std=c++17 -Wall -Wextra -Werror -Icommon -I3rd-party/json tools/localization/i18n-japanese-test.cpp common/i18n.cpp -o "$tool_root/i18n-japanese-test"
"$tool_root/i18n-japanese-test"
make 3ds -j2
python3 tools/localization/package-release.py
