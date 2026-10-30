#!/bin/zsh
set -euo pipefail

project_dir=${0:A:h:h}
python_runtime="$project_dir/.venv/bin/python"

if [[ ! -x "$python_runtime" ]]; then
  echo "Project Python environment not found: $python_runtime"
  read -r "?Press Return to close…"
  exit 1
fi

"$python_runtime" "$project_dir/tools/open_usb_app.py"
