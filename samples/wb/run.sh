#!/bin/sh
set -eu

sample_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(CDPATH= cd -- "$sample_dir/../.." && pwd)
launch_dir=$(pwd)
hawk_bin=${HAWK_BIN:-$source_dir/bld/bin/hawk}
hawkgo_bin=${HAWKGO_BIN:-$source_dir/bld/bin/hawkgo}
moddirs=${HAWK_MODLIBDIRS:-$source_dir/bld/lib/.libs:$source_dir/bld/mod/.libs}
incdirs=${HAWK_INCDIRS:-}
listen=${HAWK_WORKBENCH_LISTEN:-127.0.0.1:8080}

resolve_command()
{
	case "$1" in
		/*) printf '%s\n' "$1" ;;
		*/*) printf '%s/%s\n' "$launch_dir" "$1" ;;
		*) command -v "$1" ;;
	esac
}

hawk_bin=$(resolve_command "$hawk_bin")
hawkgo_bin=$(resolve_command "$hawkgo_bin")
if [ ! -x "$hawk_bin" ] || [ ! -x "$hawkgo_bin" ]
then
	printf '%s\n' 'Build hawk and hawkgo first, or set HAWK_BIN and HAWKGO_BIN.' >&2
	exit 1
fi

cd "$sample_dir"
"$hawk_bin" --modlibdirs="$moddirs" --incdirs="$incdirs" -f "$sample_dir/scripts/setup.hawk"
printf 'Open http://%s/app/\n' "$listen"
exec "$hawkgo_bin" --httpd --listen "$listen" \
	--root "$sample_dir/public" --script-root "$sample_dir/scripts" \
	--route /app/=workbench.hawk --route /api/=workbench.hawk \
	--modlibdirs "$moddirs" --incdirs "$incdirs" --max-concurrent 16 \
	--max-body-bytes 524288 --max-response-bytes 4194304 "$@"
