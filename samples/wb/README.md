# Hawk text workbench

A local web application demonstrating `sed::`, `m4::`, `mus::`, `json::`,
`sqlite::`, and `http::`. The request handlers are Hawk scripts. Mustache
renders the page, plain JavaScript submits requests, and SQLite stores examples.
No Go packages, JavaScript libraries, or fonts are downloaded by the application.

## Run

Build Hawk with Go support and the SQLite module enabled. For an existing
`bld/` configuration, from the repository root:

```sh
make -C bld/lib
make -C bld/mod
make -C bld/bin hawk hawkgo GO_BUILD_OPTS=-a
samples/web-workbench/run.sh
```

Open **http://127.0.0.1:8080/app/**. The launcher initializes the database and
six starter examples, then starts `hawkgo --httpd`. Stop it with Ctrl-C.
`GO_BUILD_OPTS=-a` refreshes Go's cached C binding when a static Hawk library
has changed. A new build needs `--enable-hawkgo=yes`; SQLite requires its
existing development library, and `--enable-mod-sqlite=yes:static` can select
static module registration. The Go binding currently requires Go 1.24 or later.

The launcher accepts these environment variables:

| Variable | Default |
| --- | --- |
| `HAWK_BIN` | Repository `bld/bin/hawk` |
| `HAWKGO_BIN` | Repository `bld/bin/hawkgo` |
| `HAWK_MODLIBDIRS` | Repository `bld/lib/.libs:bld/mod/.libs` |
| `HAWK_INCDIRS` | Empty (normal relative include lookup) |
| `HAWK_WORKBENCH_LISTEN` | `127.0.0.1:8080` |
| `HAWK_WORKBENCH_DB` | This sample's `workbench.db` |

A custom database path must have an existing writable parent directory. Use an
absolute path. Extra launcher arguments are passed to `hawkgo --httpd`.
`HAWK_INCDIRS` is passed to both database setup and the HTTP server. Use the
platform path-list separator for multiple directories; absolute paths are
recommended because the launcher changes to the sample directory.

## Use

- **sed:** enter commands and input text, then Run. Line endings follow the
  existing sed module's preservation behavior.
- **m4:** enter optional definitions/prelude and m4 source. A newline separates
  a nonempty prelude from the source; `dnl` suppresses definition-line output.
- **mustache:** enter a template and JSON data. The browser checks JSON before
  submission. Output is displayed as text, including generated HTML.

Ctrl-Enter (Cmd-Enter on macOS) runs the current tool. Copy copies the output.
Select an example to edit it, Save to update it, or Save as new with a different
name to keep a copy. Names are unique within a tool. Search matches names and
tool names. The library shows the 200 most recently updated examples; older
entries remain in the database. Deleted starters are not restored on restart.

Each request gets a fresh Hawk runtime. Connections and prepared statements
are closed within the request. The launcher processes up to ten script requests
at a time; pass `--max-concurrent N` to change this limit. There is no application
state shared through Hawk global variables between requests; SQLite provides
persistence.

## Dark appearance

To use the dark stylesheet, change the stylesheet link in
`templates/header.mustache` to:

```html
<link rel="stylesheet" href="/workbench-dark.css">
```

`workbench-dark.css` imports the shared layout from `workbench.css` and overrides
its colors. Reload the page to apply it; the default stylesheet remains light.

## Monochrome appearance

For a minimalist black-and-white theme with square corners, thin borders, and
a solid offset shadow, use:

```html
<link rel="stylesheet" href="/workbench-mono.css">
```

It imports the same shared layout. Selection uses inverted colors; errors use
dashed borders and disabled controls use dashed borders or struck-through text.

## Layout and API

```text
scripts/workbench.hawk       handlers, tool execution, storage
scripts/setup.hawk           idempotent schema and starter setup
templates/*.mustache         page and header/footer partials
public/                      CSS, JavaScript, landing redirect
./workbench.db               local database (not distributed)
tests/http_test.go           optional HTTP integration test
```

Only `public/` is the static document root. Scripts, templates, and data stay
outside it. Script-relative application paths are derived from `@SCRIPTNAME`.

Each response uses the HTTP server's fresh `request["csp_nonce"]` in its
Content-Security-Policy header. The page template applies the same nonce to
`workbench.js`; additional script tags must also use `nonce="{{csp_nonce}}"`.
Stylesheets remain restricted to the same origin, and responses use `no-store`.

| Endpoint | Operation |
| --- | --- |
| `GET /app/` | Render the workbench page |
| `POST /api/run` | Run `{tool, program, input}` |
| `GET /api/examples` | List saved examples |
| `POST /api/examples` | Save `{tool, title, program, input, id?}` |
| `DELETE /api/examples?id=...` | Delete an example |

POST bodies use `application/json`. POST/DELETE requests require
`X-Workbench-Request: 1`; the browser supplies it. Responses use `ok`,
`output`/`error`, `example`, or `examples`. Invalid fields return 400, duplicate
names 409, tool failures 422, and missing entries 404. Database failures return
500. The existing JSON parser raises a runtime error for malformed JSON, so a
malformed API envelope or Mustache JSON sent directly to the API returns 500;
the browser validates Mustache JSON before sending it.

## Tests

The repository's `t/h-033.hawk` tests transformations, validation, persistence,
initialization, routes, and escaping. It uses its own temporary database.

```sh
make -C bld/t check TESTS='h-012.hawk h-033.hawk'
HAWK_BIN="$PWD/bld/bin/hawk" HAWKGO_BIN="$PWD/bld/bin/hawkgo" \
  HAWK_MODLIBDIRS="$PWD/bld/lib/.libs:$PWD/bld/mod/.libs" \
  go test -v ./samples/web-workbench/tests
```

The HTTP test uses temporary databases and ephemeral loopback ports, exercises
all three tools and saved-example edits, then restarts the server to check
persistence. It also checks custom entry selection and graceful shutdown.
The concurrency test sends 2,000 mixed requests from 50 clients with ten active
script runtimes, exercising module lookup during runtime creation and cleanup.
It skips when the binary environment variables are not supplied.

## Scope

This is a local workbench without authentication or process isolation. Sed and
m4 can perform their normal file operations, and Mustache can load partials.
Input/response size limits do not impose an execution timeout: an expansion
that does not terminate can require restarting the server. Keep the listener
on loopback; public hosting needs a separate restricted execution design.
