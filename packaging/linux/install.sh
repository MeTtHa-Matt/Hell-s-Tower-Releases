#!/bin/sh
set -eu

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
project_dir=$(CDPATH= cd "$script_dir/../.." && pwd)
build_dir="$project_dir/build"

if [ -z "${HOME:-}" ]; then
  echo "Erreur : HOME n'est pas défini." >&2
  exit 1
fi

data_home=${XDG_DATA_HOME:-"$HOME/.local/share"}
case "$data_home" in
  /*) ;;
  *)
    echo "Erreur : XDG_DATA_HOME doit être un chemin absolu." >&2
    exit 1
    ;;
esac

if [ ! -x "$build_dir/hells_tower" ] || [ ! -d "$build_dir/assets" ]; then
  echo "Erreur : build Linux introuvable dans $build_dir." >&2
  echo "Compilez d'abord le projet avec CMake, puis relancez ce script." >&2
  exit 1
fi

install_dir="$HOME/.local/opt/hells-tower"
applications_dir="$data_home/applications"
desktop_file="$applications_dir/hells_tower.desktop"

mkdir -p "$install_dir" "$applications_dir"
cp "$build_dir/hells_tower" "$install_dir/hells_tower"
chmod 755 "$install_dir/hells_tower"
cp -R "$build_dir/assets" "$install_dir/"

if [ -f "$build_dir/THIRD_PARTY_NOTICES.txt" ]; then
  cp "$build_dir/THIRD_PARTY_NOTICES.txt" "$install_dir/THIRD_PARTY_NOTICES.txt"
fi

escaped_exec=$(printf '%s' "$install_dir/hells_tower" |
  sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/`/\\`/g' -e 's/\$/\\$/g' -e 's/%/%%/g')

cat > "$desktop_file" <<EOF
[Desktop Entry]
Type=Application
Name=Hell's Tower
Comment=Grimpez les étages de la tour
Exec="$escaped_exec"
Path=$install_dir
Icon=$install_dir/assets/heart.png
Categories=Game;
Terminal=false
EOF
chmod 644 "$desktop_file"

printf 'Hell'\''s Tower est installé dans %s\n' "$install_dir"
printf 'Raccourci créé : %s\n' "$desktop_file"
