#!/bin/sh

[ $# -lt 1 ] || HAWK_MAN_COMPILER="$1"
HAWK_MAN_COMPILER="${HAWK_MAN_COMPILER:-${HAWK_BIN:-hawk}}"
set -u

source_dir=$(cd "${srcdir:-$(dirname "$0")}/.." && pwd) || exit 1
converter="$source_dir/tools/md2man.hawk"
tmp_dir="./hawk-regress-md2man-$$"
umask 077
if ! mkdir "$tmp_dir"
then
	echo "Bail out! unable to create test directory"
	exit 1
fi
trap 'rm -f "$tmp_dir"/*; rmdir "$tmp_dir"' 0
trap 'exit 1' 1 2 3 15

test_no=0
failed=0
ok()
{
	test_no=$((test_no + 1))
	echo "ok $test_no - $1"
}
not_ok()
{
	test_no=$((test_no + 1))
	failed=1
	echo "not ok $test_no - $1"
	while IFS= read -r error_line
	do
		printf '# %s\n' "$error_line"
	done < "$tmp_dir/error"
}
convert()
{
	"$HAWK_MAN_COMPILER" --tolerant=off -f "$converter" "$@"
}
contains()
{
	if grep -F -x -e "$2" "$tmp_dir/output" > /dev/null
	then
		ok "$1"
	else
		not_ok "$1"
	fi
}
reject()
{
	printf '%s\n' "$2" > "$tmp_dir/bad.1.md"
	if convert "$tmp_dir/bad.1.md" > "$tmp_dir/output" 2> "$tmp_dir/error"
	then
		not_ok "$1"
	elif grep -F -e "md2man:" "$tmp_dir/error" > /dev/null
	then
		ok "$1"
	else
		not_ok "$1"
	fi
}

cat > "$tmp_dir/fixture.7.md" <<'EOF'
# NAME

fixture - **bold**, *italic*, and `code`.
This is a continued paragraph.

## Details

- first item
  continued item
- **bold `code` still bold**
1. numbered item
2) another item

[Language](hawk-language.7.md#types) and [site](https://example.org/a(b)).
``a `tick` b`` and \*literal\*.

```sh
.SH INJECTED
'br
\fB literal \n and "quotes"
```

  ~~~~text
  indented example
  ~~~~~
EOF
if convert "$tmp_dir/fixture.7.md" > "$tmp_dir/output" 2> "$tmp_dir/error"
then
	ok "convert supported Markdown blocks"
else
	not_ok "convert supported Markdown blocks"
fi
cp "$tmp_dir/output" "$tmp_dir/reference"
contains "derive title and section from filename" '.TH "FIXTURE" "7" "" "" ""'
contains "second-level heading" '.SS "Details"'
contains "paragraph wrapping and inline fonts" '\&fixture \- \fBbold\fR, \fIitalic\fR, and \f(CRcode\fR. This is a continued paragraph.'
contains "continued list item" '\&first item continued item'
contains "restore bold after inline code" '\&\fBbold \f(CRcode\fB still bold\fR'
contains "numbered list label" '.IP "2." 4'
contains "manual reference and balanced link destination" '\&\fBhawk\-language\fR(7) and site\fR (https://example.org/a(b)). \f(CRa `tick` b\fR and *literal*.'
contains "protect dot at the start of code" '\&.SH INJECTED'
contains "protect apostrophe at the start of code" "\&'br"
contains "escape roff input and quotes" '\&\efB literal \en and \(dqquotes\(dq'
contains "tilde fence indentation and longer closing fence" '\&indented example'

printf '%s\n' '**[outer *inner* `code` outer](https://example.org) still outer**' > "$tmp_dir/link.1.md"
if convert "$tmp_dir/link.1.md" > "$tmp_dir/output" 2> "$tmp_dir/error"
then
	contains "link labels inherit surrounding emphasis" '\&\fBouter \f(BIinner\fB \f(CRcode\fB outer\fB (https://example.org) still outer\fR'
else
	not_ok "link labels inherit surrounding emphasis"
fi

if convert "$tmp_dir/fixture.7.md" > "$tmp_dir/output" 2> "$tmp_dir/error" &&
   cmp "$tmp_dir/reference" "$tmp_dir/output" > /dev/null 2>&1
then
	ok "generation is deterministic"
else
	not_ok "generation is deterministic"
fi
if convert -v title=FIXTURE -v section=7 < "$tmp_dir/fixture.7.md" > "$tmp_dir/output" 2> "$tmp_dir/error" &&
   cmp "$tmp_dir/reference" "$tmp_dir/output" > /dev/null 2>&1
then
	ok "standard input with explicit metadata"
else
	not_ok "standard input with explicit metadata"
fi
if "$HAWK_MAN_COMPILER" 'BEGIN { ORS="\r\n" } { print }' "$tmp_dir/fixture.7.md" > "$tmp_dir/crlf.7.md" 2> "$tmp_dir/error" &&
   convert -v title=FIXTURE "$tmp_dir/crlf.7.md" > "$tmp_dir/output" 2> "$tmp_dir/error" &&
   cmp "$tmp_dir/reference" "$tmp_dir/output" > /dev/null 2>&1
then
	ok "CRLF input matches LF input"
else
	not_ok "CRLF input matches LF input"
fi

printf '```\n\tcolumn\n```\n' > "$tmp_dir/tab.1.md"
if convert "$tmp_dir/tab.1.md" > "$tmp_dir/output" 2> "$tmp_dir/error"
then
	contains "expand code tabs to eight-column stops" '\&        column'
else
	not_ok "expand code tabs to eight-column stops"
fi
if convert -v 'title=A"B\C' "$tmp_dir/fixture.7.md" > "$tmp_dir/output" 2> "$tmp_dir/error"
then
	contains "escape metadata quotes and backslashes" '.TH "A\(dqB\eC" "7" "" "" ""'
else
	not_ok "escape metadata quotes and backslashes"
fi

reject "reject unclosed fence" '```
example'
reject "reject unclosed inline code" 'unfinished `code'
reject "reject unclosed emphasis" '**unfinished'
reject "reject unclosed link destination" '[label](destination'
reject "reject unsupported heading depth" '### Heading'
reject "reject tables" '| first | second |'
reject "reject block quotes" '> quoted'
reject "reject nested lists" '- parent
  - child'
reject "reject raw HTML" '<div>text</div>'
reject "reject task lists" '- [ ] task'
reject "reject strikethrough" '~~deleted~~'

printf '# NAME\nbad\000input\n' > "$tmp_dir/nul.1.md"
if convert "$tmp_dir/nul.1.md" > "$tmp_dir/output" 2> "$tmp_dir/error"
then
	not_ok "reject embedded NUL"
elif grep -F -e "control character" "$tmp_dir/error" > /dev/null
then
	ok "reject embedded NUL"
else
	not_ok "reject embedded NUL"
fi
if convert -v section=0 "$tmp_dir/fixture.7.md" > "$tmp_dir/output" 2> "$tmp_dir/error"
then
	not_ok "reject invalid manual section"
else
	ok "reject invalid manual section"
fi
if convert -v 'header=first
second' "$tmp_dir/fixture.7.md" > "$tmp_dir/output" 2> "$tmp_dir/error" ||
   [ -s "$tmp_dir/output" ]
then
	not_ok "reject metadata newline before producing roff"
else
	ok "reject metadata newline before producing roff"
fi
if convert "$tmp_dir/fixture.7.md" "$tmp_dir/fixture.7.md" > "$tmp_dir/output" 2> "$tmp_dir/error"
then
	not_ok "reject multiple input files"
else
	ok "reject multiple input files"
fi

for page in hawk.1 hawkgo.1 hawk-language.7 hawk-modules.7
do
	if convert "$source_dir/man/$page.md" > "$tmp_dir/$page" 2> "$tmp_dir/error"
	then
		ok "convert $page"
		if command -v groff > /dev/null 2>&1
		then
			if groff -ww -man -Tutf8 "$tmp_dir/$page" > "$tmp_dir/rendered" 2> "$tmp_dir/error" &&
			   [ ! -s "$tmp_dir/error" ]
			then
				ok "render $page without roff warnings"
			else
				not_ok "render $page without roff warnings"
			fi
		else
			ok "render $page # SKIP groff is unavailable"
		fi
	else
		not_ok "convert $page"
	fi
done

echo "1..$test_no"
exit "$failed"
