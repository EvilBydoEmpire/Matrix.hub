#!/bin/zsh
set -euo pipefail

project_dir=${0:A:h:h}
if (( $# != 2 )); then
  echo "Usage: $0 16|22|64 VERIFIED_DEVICE_IP"
  echo "Wi-Fi update only; checks hardware before flashing. See gift/INSTALL_WITH_AI.txt."
  exit 2
fi
exec "$project_dir/.venv/bin/python" "$project_dir/tools/matrix_hub.py" update --profile "$1" --host "$2"
