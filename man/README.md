# Hawk Manuals

These Markdown files are the maintained sources of Hawk's installed manual
pages. Follow the links here when browsing the source tree, or use `man` after
installation.

| Manual | Contents | Installed command |
| --- | --- | --- |
| [hawk(1)](hawk.1.md) | Interpreter invocation, options, cut, sed, m4, and shell modes | `man hawk` |
| [hawkgo(1)](hawkgo.1.md) | Go command-line runner and HTTP application server | `man hawkgo` |
| [hawk-language(7)](hawk-language.7.md) | Execution, types, declarations, operators, functions, and I/O | `man hawk-language` |
| [hawk-modules(7)](hawk-modules.7.md) | Module loading and commonly used module interfaces | `man hawk-modules` |

The [project README](../README.md) also covers building and embedding Hawk,
with additional examples and platform notes.

## Editing and generating

Edit the `.md` files, then run this target in a configured source or build
directory:

```sh
make man-pages
```

The target runs [tools/md2man.hawk](../tools/md2man.hawk) with the built C
interpreter, building the interpreter first if it is missing. Ordinary
`make` also generates the pages; `make install` and `make install-data`
generate any missing or outdated pages before installation. Each page depends
on its Markdown source, the converter script, the converter executable, and
the configured manual Makefile.

Generated pages stay under `man/` in the build directory. They are removed
by `make clean` and are not included in source distributions. Only the
Markdown sources and converter are maintained in the source tree.

When cross-compiling, provide a runnable host Hawk executable:

```sh
make MAN_HAWK=/absolute/path/to/host/hawk
make install-data MAN_HAWK=/absolute/path/to/host/hawk
```

The override names a single executable; use a wrapper script if it needs
additional arguments. `hawkgo(1)` is generated and installed only when
`--enable-hawkgo` is enabled. Neither Pandoc nor Lua is required.

For a single page, invoke the converter directly:

```sh
bld/bin/hawk --tolerant=off -v footer="Hawk Manual" \
    -f tools/md2man.hawk man/hawk.1.md > hawk.1
```

Title and section default to the input filename (`hawk.1.md` becomes `HAWK`,
section `1`). Override them with `-v title=NAME -v section=1`, which are
required when reading standard input. Optional metadata variables are `footer`,
`header`, and `date`. The converter uses the built-in `sys::` module only to
write error diagnostics to standard error.

## Authoring conventions

- Use standard manual headings such as `NAME`, `SYNOPSIS`, `DESCRIPTION`,
  `OPTIONS`, `EXAMPLES`, and `SEE ALSO` as level-one Markdown headings.
- Keep links between manuals relative, for example
  `[Language reference](hawk-language.7.md)`. The converter turns them
  into references such as `hawk-language(7)` in the generated pages.
- Use paragraphs, single-level bullet or numbered lists, `*italic*`,
  `**bold**`, inline code, and inline links. List items may wrap across lines
  but must contain a single paragraph. Use backslash escapes for literal
  Markdown punctuation.
- Fence examples with backticks or tildes. Tabs in examples expand to
  eight-column stops. Use fenced blocks rather than indented code.
- This is a focused converter, not a complete Markdown implementation.
  Avoid tables, HTML, images, block quotes, nested or task lists, reference
  links, underscore emphasis, and setext headings. Unsupported blocks and
  unfinished formatting produce errors.
- Version metadata comes from `Makefile.am`. Generation does not insert the
  current date, so identical sources, metadata, and converter produce
  identical output. Roff control characters are escaped automatically.
- Run `groff -man -Tutf8 man/hawk.1` or `man -l man/hawk.1` to inspect a page.
  Run `make -C bld/t check TESTS=regress-md2man.sh` to check conversion,
  escaping, malformed input, and all four manual sources.
