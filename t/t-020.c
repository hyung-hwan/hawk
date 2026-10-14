/* Mustache core conformance and embedding lifecycle. */
#include <hawk-mus.h>
#include <hawk-ecs.h>
#include <hawk-str.h>
#include <hawk-utl.h>
#include "tap.h"
#include <string.h>
#if defined(_WIN32)
#include <direct.h>
#elif defined(HAVE_UNISTD_H) && !defined(__OS2__) && !defined(__DOS__)
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef struct value_t value_t;
typedef struct field_t field_t;
struct field_t { const hawk_ooch_t* name; const value_t* value; };
struct value_t
{
	int type;
	const hawk_ooch_t* text;
	const field_t* fields;
	const value_t* const* items;
	hawk_oow_t count;
};
typedef struct spec_t spec_t;
struct spec_t
{
	const char* name;
	const hawk_ooch_t* text;
	const value_t* root;
	const value_t* partials;
	const hawk_ooch_t* expected;
};
#include "mus-spec.inc"

static int get (hawk_mus_t* mus, void* val, const hawk_ooch_t* name, hawk_oow_t len, void** result, void* ctx)
{
	const value_t* value = val;
	hawk_oow_t i;

	if (!value || value->type != HAWK_MUS_VAL_MAP) return 0;
	for (i = 0; i < value->count; i++)
	{
		if (hawk_comp_oochars_oocstr(name, len, value->fields[i].name, 0) == 0)
		{
			*result = (void*)value->fields[i].value;
			return 1;
		}
	}
	return 0;
}

static int type (hawk_mus_t* mus, void* value, void* ctx)
{
	return ((const value_t*)value)->type;
}

static int str (hawk_mus_t* mus, void* value, const hawk_ooch_t** ptr, hawk_oow_t* len, void* ctx)
{
	*ptr = ((const value_t*)value)->text;
	*len = hawk_count_oocstr(*ptr);
	return 0;
}

static int count (hawk_mus_t* mus, void* value, hawk_oow_t* size, void* ctx)
{
	*size = ((const value_t*)value)->count;
	return 0;
}

static int item (hawk_mus_t* mus, void* value, hawk_oow_t index, void** result, void* ctx)
{
	*result = (void*)((const value_t*)value)->items[index];
	return 0;
}

static const hawk_mus_data_t callbacks = { get, type, str, count, item };
static const value_t empty = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };

typedef struct source_t source_t;
struct source_t { const hawk_ooch_t* ptr; hawk_oow_t len, pos; };
typedef struct io_t io_t;
struct io_t
{
	hawk_ooecs_t buffer;
	const value_t* partials;
	const hawk_ooch_t* input;
	int opened, closed;
	int short_io, fail_write, fail_read, fail_close, busy_probe;
};

static hawk_ooi_t io (hawk_mus_t* mus, hawk_mus_io_cmd_t cmd, hawk_mus_io_arg_t* arg, hawk_ooch_t* data, hawk_oow_t size, void* ctx)
{
	io_t* state = ctx;
	source_t* source;
	void* value;
	hawk_oow_t len;

	if (cmd == HAWK_MUS_IO_OPEN)
	{
		if (arg->kind == HAWK_MUS_IO_OUTPUT) arg->handle = state;
		else
		{
			value = HAWK_NULL;
			if (arg->kind == HAWK_MUS_IO_PARTIAL &&
			    !get(mus, (void*)state->partials, arg->name, hawk_count_oocstr(arg->name), &value, HAWK_NULL)) return 0;
			source = hawk_mus_allocmem(mus, sizeof(*source));
			if (!source) return -1;
			source->ptr = arg->kind == HAWK_MUS_IO_TEMPLATE? state->input: ((const value_t*)value)->text;
			source->len = hawk_count_oocstr(source->ptr);
			source->pos = 0;
			arg->handle = source;
		}
		state->opened++;
		return 1;
	}
	if (cmd == HAWK_MUS_IO_CLOSE)
	{
		if (arg->handle != state) hawk_mus_freemem(mus, arg->handle);
		state->closed++;
		if (state->fail_close)
		{
			hawk_mus_seterrbfmt(mus, HAWK_NULL, HAWK_EOTHER, "injected close failure");
			return -1;
		}
		return 0;
	}
	if (state->short_io && size > 1) size = 1;
	if (cmd == HAWK_MUS_IO_READ)
	{
		if (state->fail_read) return -1;
		source = arg->handle;
		len = source->len - source->pos;
		if (size > len) size = len;
		hawk_copy_oochars(data, source->ptr + source->pos, size);
		source->pos += size;
		return size;
	}
	if (state->busy_probe)
	{
		state->busy_probe = hawk_mus_compilewithoochars(mus, HAWK_T("bad"), 3) <= -1 &&
		    hawk_mus_geterrnum(mus) == HAWK_EPERM? 2: -1;
	}
	if (state->fail_write) return state->fail_write == 1? 0: -1;
	return hawk_ooecs_ncat(&state->buffer, data, size) == (hawk_oow_t)-1? -1: (hawk_ooi_t)size;
}

static int init_io (hawk_mus_t* mus, io_t* state, const value_t* partials)
{
	state->partials = partials;
	return hawk_ooecs_init(&state->buffer, hawk_mus_getgem(mus), 0);
}

static void conformance (void)
{
	hawk_mus_t* mus;
	io_t state;
	hawk_oow_t i;
	int n;

	mus = hawk_mus_openstd(0, HAWK_NULL);
	if (!mus) { bail_out("cannot open mustache engine"); return; }
	for (i = 0; i < sizeof(specs) / sizeof(specs[0]); i++)
	{
		memset(&state, 0, sizeof(state));
		state.short_io = 1;
		if (init_io(mus, &state, specs[i].partials) <= -1) { bail_out("cannot create output buffer"); break; }
		n = hawk_mus_compilewithoochars(mus, specs[i].text, hawk_count_oocstr(specs[i].text));
		if (n == 0) n = hawk_mus_render(mus, (void*)specs[i].root, &callbacks, HAWK_NULL, io, &state);
		if (n <= -1) fprintf(stderr, "%s: %s\n", specs[i].name, hawk_mus_geterrbmsg(mus));
		else if (hawk_comp_oocstr(HAWK_OOECS_PTR(&state.buffer)? HAWK_OOECS_PTR(&state.buffer): HAWK_T(""), specs[i].expected, 0) != 0)
			fprintf(stderr, "%s: output differs (%lu chars)\n", specs[i].name, (unsigned long)HAWK_OOECS_LEN(&state.buffer));
		OK(n == 0 && hawk_comp_oocstr(HAWK_OOECS_PTR(&state.buffer)? HAWK_OOECS_PTR(&state.buffer): HAWK_T(""), specs[i].expected, 0) == 0 &&
		   state.opened == state.closed, specs[i].name);
		hawk_ooecs_fini(&state.buffer);
	}
	hawk_mus_close(mus);
}

static int render_text (hawk_mus_t* mus, const hawk_ooch_t* expected)
{
	hawk_mus_iostd_t output;
	int n;

	memset(&output, 0, sizeof(output));
	output.type = HAWK_MUS_IOSTD_OOCS;
	n = hawk_mus_renderstd(mus, (void*)&empty, &callbacks, HAWK_NULL, &output);
	if (n == 0)
	{
		n = hawk_comp_oocstr(output.u.oocs.ptr? output.u.oocs.ptr: HAWK_T(""), expected, 0) == 0? 0: -1;
		hawk_mus_freemem(mus, output.u.oocs.ptr);
	}
	return n;
}

static void errors (void)
{
	hawk_mus_t* mus;
	io_t state;
	hawk_oow_t limit = 4;
	hawk_oow_t i;
	const hawk_ooch_t* bad[] = { HAWK_T("{{"), HAWK_T("{{}}"), HAWK_T("{{#a}}"),
		HAWK_T("{{/a}}"), HAWK_T("{{#a}}{{/b}}"), HAWK_T("{{=a=}}"),
		HAWK_T("{{=a b c=}}"), HAWK_T("{{{a}}"), HAWK_T("{{=<% =%>=}}") };
	static const value_t recursive = { HAWK_MUS_VAL_SCALAR, HAWK_T("{{>loop}}"), HAWK_NULL, HAWK_NULL, 0 };
	static const field_t fields[] = { { HAWK_T("loop"), &recursive } };
	static const value_t partials = { HAWK_MUS_VAL_MAP, HAWK_T(""), fields, HAWK_NULL, 1 };
	int n;

	mus = hawk_mus_openstd(16, HAWK_NULL);
	if (!mus) { bail_out("cannot open engine"); return; }
	OK(hawk_mus_compilewithoochars(mus, HAWK_T("saved"), 5) == 0, "compile reusable template");
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		n = hawk_mus_compilewithoochars(mus, bad[i], hawk_count_oocstr(bad[i]));
		OK(n <= -1 && hawk_mus_geterrnum(mus) == HAWK_EINVAL && render_text(mus, HAWK_T("saved")) == 0,
		   "invalid template preserves previous template");
	}
	OK(hawk_mus_compilewithoochars(mus, HAWK_NULL, (hawk_oow_t)-1) <= -1, "invalid source length is rejected");
	OK(hawk_mus_compilewithoochars(mus, HAWK_T("x"), (hawk_oow_t)-1) <= -1 &&
	   hawk_mus_geterrnum(mus) == HAWK_ENOMEM, "owned template sizing rejects overflow before reading input");
	OK(hawk_mus_setopt(mus, HAWK_MUS_OPT_MAX_DEPTH, &limit) == 0 &&
	   hawk_mus_compilewithoochars(mus, HAWK_T("{{#a}}{{#b}}{{#c}}{{#d}}{{#e}}{{/e}}{{/d}}{{/c}}{{/b}}{{/a}}"), 60) <= -1,
	   "compiler enforces section depth");
	memset(&state, 0, sizeof(state));
	init_io(mus, &state, &partials);
	hawk_mus_compilewithoochars(mus, HAWK_T("{{>loop}}"), 9);
	n = hawk_mus_render(mus, (void*)&empty, &callbacks, HAWK_NULL, io, &state);
	OK(n <= -1 && hawk_mus_geterrnum(mus) == HAWK_EBUFFULL && state.opened == state.closed,
	   "recursive partials fail at the configured heap frame limit");
	hawk_ooecs_fini(&state.buffer);
	hawk_mus_compilewithoochars(mus, HAWK_T("saved"), 5);
	memset(&state, 0, sizeof(state));
	init_io(mus, &state, &empty);
	state.fail_write = 1;
	n = hawk_mus_render(mus, (void*)&empty, &callbacks, HAWK_NULL, io, &state);
	OK(n <= -1 && hawk_mus_geterrnum(mus) == HAWK_EIOERR && state.opened == state.closed,
	   "zero-progress output fails and closes its stream");
	state.fail_write = 0;
	state.fail_close = 1;
	n = hawk_mus_render(mus, (void*)&empty, &callbacks, HAWK_NULL, io, &state);
	OK(n <= -1 && hawk_mus_geterrnum(mus) == HAWK_EOTHER, "successful rendering reports close failure");
	state.fail_write = 2;
	n = hawk_mus_render(mus, (void*)&empty, &callbacks, HAWK_NULL, io, &state);
	OK(n <= -1 && hawk_mus_geterrnum(mus) == HAWK_EIOERR, "close failure does not overwrite output failure");
	state.fail_write = state.fail_close = 0;
	state.busy_probe = 1;
	n = hawk_mus_render(mus, (void*)&empty, &callbacks, HAWK_NULL, io, &state);
	OK(n == 0 && state.busy_probe == 2, "callbacks cannot replace an active template");
	hawk_ooecs_fini(&state.buffer);
	OK(render_text(mus, HAWK_T("saved")) == 0, "rendering remains reusable after failures");
	state.input = HAWK_T("template");
	state.fail_read = 1;
	state.busy_probe = 0;
	n = hawk_mus_compile(mus, HAWK_T("input"), io, &state);
	OK(n <= -1 && state.opened == state.closed && render_text(mus, HAWK_T("saved")) == 0,
	   "input failure closes its stream and preserves the parsed template");
	OK(hawk_mus_compilewithoochars(mus, HAWK_NULL, 0) == 0 && render_text(mus, HAWK_T("")) == 0,
	   "empty templates are valid");
	hawk_mus_reset(mus);
	OK(render_text(mus, HAWK_T("")) <= -1, "reset removes the compiled template");
	hawk_mus_close(mus);
}


typedef struct allocator_t allocator_t;
struct allocator_t
{
	hawk_mmgr_t* real;
	long budget;
	long live;
};

static void* alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	allocator_t* state = mmgr->ctx;
	void* ptr;

	if (state->budget == 0) return HAWK_NULL;
	if (state->budget > 0) state->budget--;
	ptr = state->real->alloc(state->real, size);
	if (ptr) state->live++;
	return ptr;
}

static void* realloc_mem (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	allocator_t* state = mmgr->ctx;
	void* result;

	if (state->budget == 0) return HAWK_NULL;
	if (state->budget > 0) state->budget--;
	result = state->real->realloc(state->real, ptr, size);
	if (result && !ptr) state->live++;
	return result;
}

static void free_mem (hawk_mmgr_t* mmgr, void* ptr)
{
	allocator_t* state = mmgr->ctx;

	if (ptr) state->live--;
	state->real->free(state->real, ptr);
}

static void allocation_failures (void)
{
	hawk_mus_t* seed;
	hawk_mus_t* mus;
	allocator_t allocator;
	hawk_mmgr_t mmgr = { alloc, realloc_mem, free_mem, &allocator };
	io_t state;
	const spec_t* fixture = HAWK_NULL;
	hawk_oow_t i;
	int pass, fail, n, good, failed = 0;

	seed = hawk_mus_openstd(0, HAWK_NULL);
	if (!seed) { bail_out("cannot open allocator seed"); return; }
	allocator.real = hawk_mus_getmmgr(seed);
	for (i = 0; i < sizeof(specs) / sizeof(specs[0]); i++)
	{
		if (strcmp(specs[i].name, "partials/Recursion") == 0) fixture = &specs[i];
	}
	if (!fixture) { bail_out("recursive fixture missing"); hawk_mus_close(seed); return; }
	for (pass = 0; pass < 2; pass++)
	{
		for (fail = 0; fail < 48; fail++)
		{
			allocator.budget = -1;
			allocator.live = 0;
			mus = hawk_mus_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
			if (!mus) { bail_out("cannot open failure probe"); break; }
			good = hawk_mus_compilewithoochars(mus, HAWK_T("saved"), 5) == 0;
			memset(&state, 0, sizeof(state));
			if (init_io(mus, &state, fixture->partials) <= -1) good = 0;
			if (pass == 0)
			{
				allocator.budget = fail;
				n = hawk_mus_compilewithoochars(mus, fixture->text, hawk_count_oocstr(fixture->text));
				if (n <= -1)
				{
					failed++;
					good = good && hawk_mus_geterrnum(mus) == HAWK_ENOMEM;
					allocator.budget = -1;
					good = good && render_text(mus, HAWK_T("saved")) == 0;
				}
			}
			else
			{
				if (hawk_mus_compilewithoochars(mus, fixture->text, hawk_count_oocstr(fixture->text)) <= -1) good = 0;
				allocator.budget = fail;
				n = hawk_mus_render(mus, (void*)fixture->root, &callbacks, HAWK_NULL, io, &state);
				if (n <= -1)
				{
					failed++;
					good = good && hawk_mus_geterrnum(mus) == HAWK_ENOMEM;
				}
				good = good && state.opened == state.closed;
				allocator.budget = -1;
				hawk_ooecs_clear(&state.buffer);
				n = hawk_mus_render(mus, (void*)fixture->root, &callbacks, HAWK_NULL, io, &state);
				good = good && n == 0 && hawk_comp_oocstr(HAWK_OOECS_PTR(&state.buffer), fixture->expected, 0) == 0;
			}
			allocator.budget = -1;
			hawk_ooecs_fini(&state.buffer);
			hawk_mus_close(mus);
			OK(good && allocator.live == 0, pass? "render allocation failure cleans up and permits retry": "compile allocation failure preserves template and leaks nothing");
		}
	}
	OK(failed >= 10, "allocator probes exercise multiple actual failure sites");
	hawk_mus_close(seed);
}

static void stock_files (void)
{
	hawk_mus_t* mus;
	hawk_mus_iostd_t input;
	hawk_mus_iostd_t output;
	hawk_ntime_t now;
	FILE* file;
	char name[100];
	char main_path[120];
	char partial_path[120];
	char output_path[120];
	char content[160];
	hawk_ooch_t oo_main[120];
	hawk_ooch_t oo_output[120];
	hawk_ooch_t oo_partial[120];
	hawk_oow_t i;
	int n;

	mus = hawk_mus_openstd(0, HAWK_NULL);
	if (!mus) { bail_out("cannot open stock I/O probe"); return; }
	if (hawk_get_ntime(&now) <= -1) { bail_out("cannot get unique filename"); hawk_mus_close(mus); return; }
	snprintf(name, sizeof(name), "mus-020-%lu-%lu", (unsigned long)now.sec, (unsigned long)now.nsec);
	snprintf(main_path, sizeof(main_path), "%s-main", name);
	snprintf(partial_path, sizeof(partial_path), "%s-part.mustache", name);
	snprintf(output_path, sizeof(output_path), "%s-out", name);
	snprintf(content, sizeof(content), "file {{>%s-part}}", name);
	for (i = 0; main_path[i]; i++) oo_main[i] = (unsigned char)main_path[i];
	oo_main[i] = 0;
	for (i = 0; output_path[i]; i++) oo_output[i] = (unsigned char)output_path[i];
	oo_output[i] = 0;
	for (i = 0; partial_path[i]; i++) oo_partial[i] = (unsigned char)partial_path[i];
	oo_partial[i] = 0;
	file = fopen(main_path, "wb");
	if (!file) { bail_out("cannot create template fixture"); goto done; }
	fputs(content, file);
	fclose(file);
	file = fopen(partial_path, "wb");
	if (!file) { bail_out("cannot create partial fixture"); goto done; }
	fputs("partial", file);
	fclose(file);
	memset(&input, 0, sizeof(input));
	memset(&output, 0, sizeof(output));
	input.type = HAWK_MUS_IOSTD_FILE;
	input.u.file.path = oo_main;
	output.type = HAWK_MUS_IOSTD_OOCS;
	n = hawk_mus_compilestd(mus, &input);
	OK(n == 0 && render_text(mus, HAWK_T("file partial")) == 0,
	   "stock file compilation resolves partials with the .mustache suffix");
	output.type = HAWK_MUS_IOSTD_FILE;
	output.u.file.path = oo_output;
	n = hawk_mus_renderstd(mus, (void*)&empty, &callbacks, HAWK_NULL, &output);
	input.u.file.path = oo_output;
	OK(n == 0 && hawk_mus_compilestd(mus, &input) == 0 && render_text(mus, HAWK_T("file partial")) == 0,
	   "stock file output is flushed and can be read back");
	input.u.file.path = oo_partial;
	OK(hawk_mus_compilestd(mus, &input) == 0 && render_text(mus, HAWK_T("partial")) == 0,
	   "stock file descriptors remain reusable");
	input.type = HAWK_MUS_IOSTD_OOCS;
	input.u.oocs.ptr = HAWK_T("文字 {{missing}}");
	input.u.oocs.len = hawk_count_oocstr(input.u.oocs.ptr);
	OK(hawk_mus_compilestd(mus, &input) == 0 && render_text(mus, HAWK_T("文字 ")) == 0,
	   "stock string I/O preserves Unicode text in either character build");
	input.type = HAWK_MUS_IOSTD_FILE;
	input.u.file.path = HAWK_T("mus-020-does-not-exist-template");
	OK(hawk_mus_compilestd(mus, &input) <= -1 && render_text(mus, HAWK_T("文字 ")) == 0,
	   "missing stock file preserves the previous template");
	input.type = HAWK_MUS_IOSTD_OOCS;
	input.u.oocs.ptr = HAWK_NULL;
	input.u.oocs.len = 0;
	memset(&output, 0, sizeof(output));
	output.type = HAWK_MUS_IOSTD_OOCS;
	OK(hawk_mus_compilestd(mus, &input) == 0 &&
	   hawk_mus_renderstd(mus, (void*)&empty, &callbacks, HAWK_NULL, &output) == 0 &&
	   output.u.oocs.ptr != HAWK_NULL && output.u.oocs.len == 0,
	   "empty stock output is allocated and null terminated");
	if (output.u.oocs.ptr) hawk_mus_freemem(mus, output.u.oocs.ptr);
done:
	remove(main_path);
	remove(partial_path);
	remove(output_path);
	hawk_mus_close(mus);
}


static int nul_str (hawk_mus_t* mus, void* value, const hawk_ooch_t** ptr, hawk_oow_t* len, void* ctx)
{
	static const hawk_ooch_t text[] = { 'a', 0, 'b' };
	*ptr = text;
	*len = 3;
	return 0;
}

static void character_and_instance_tests (void)
{
	hawk_mus_t* first;
	hawk_mus_t* second;
	hawk_mus_iostd_t output;
	hawk_mus_data_t data = callbacks;
	hawk_errinf_t error;
	static const hawk_uch_t uchars[] = { 0x65e5, 0x672c };
	static const hawk_ooch_t nul[] = { 'a', 0, 'b' };
	static const value_t zero = { HAWK_MUS_VAL_SCALAR, HAWK_T("0"), HAWK_NULL, HAWK_NULL, 0 };
	static const value_t empty_string = { HAWK_MUS_VAL_SCALAR, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
	static const field_t fields[] = { { HAWK_T("zero"), &zero }, { HAWK_T("empty"), &empty_string } };
	static const value_t model = { HAWK_MUS_VAL_MAP, HAWK_T(""), fields, HAWK_NULL, 2 };
	const hawk_ooch_t* source = HAWK_T("{{#zero}}Z{{/zero}}{{#empty}}E{{/empty}}");
	unsigned char* xtn;
	hawk_oow_t i;
	int n;

	OK(hawk_mus_open(HAWK_NULL, 0, HAWK_NULL, &error) == HAWK_NULL && error.num == HAWK_EINVAL,
	   "core open requires an allocator; stock open supplies one");
	first = hawk_mus_openstd(16, HAWK_NULL);
	second = hawk_mus_openstd(0, HAWK_NULL);
	if (!first || !second) { bail_out("cannot open independent instances"); return; }
	xtn = hawk_mus_getxtn(first);
	for (i = 0; i < 16 && xtn[i] == 0; i++);
	OK(i == 16, "application extension is zero initialized");
	OK(hawk_mus_compilewithbchars(first, "日本", 6) == 0 && render_text(first, HAWK_T("日本")) == 0,
	   "byte source compiles in either character build");
	OK(hawk_mus_compilewithuchars(second, uchars, 2) == 0 && render_text(second, HAWK_T("日本")) == 0,
	   "Unicode source compiles in either character build");
	hawk_mus_reset(first);
	OK(render_text(second, HAWK_T("日本")) == 0, "instances have independent template state");
#if defined(HAWK_OOCH_IS_UCH)
	OK(hawk_mus_compilewithbchars(second, "\xff", 1) <= -1 && render_text(second, HAWK_T("日本")) == 0,
	   "invalid source encoding preserves the prior Unicode template");
#endif
	memset(&output, 0, sizeof(output));
	output.type = HAWK_MUS_IOSTD_OOCS;
	n = hawk_mus_compilewithoochars(first, source, hawk_count_oocstr(source));
	if (n == 0) n = hawk_mus_renderstd(first, (void*)&model, &callbacks, HAWK_NULL, &output);
	OK(n == 0 && hawk_comp_oocstr(output.u.oocs.ptr, HAWK_T("ZE"), 0) == 0,
	   "numeric zero and empty strings are true in mustache sections");
	if (n == 0) hawk_mus_freemem(first, output.u.oocs.ptr);
	memset(&output, 0, sizeof(output));
	output.type = HAWK_MUS_IOSTD_OOCS;
	data.str = nul_str;
	n = hawk_mus_compilewithoochars(first, HAWK_T("{{.}}"), 5);
	if (n == 0) n = hawk_mus_renderstd(first, (void*)&zero, &data, HAWK_NULL, &output);
	OK(n == 0 && output.u.oocs.len == 3 && hawk_comp_oochars(output.u.oocs.ptr, 3, nul, 3, 0) == 0,
	   "length-bearing scalar output preserves embedded NUL");
	if (n == 0) hawk_mus_freemem(first, output.u.oocs.ptr);
	memset(&output, 0, sizeof(output));
	output.type = HAWK_MUS_IOSTD_OOCS;
	n = hawk_mus_compilewithoochars(first, nul, 3);
	if (n == 0) n = hawk_mus_renderstd(first, (void*)&empty, &callbacks, HAWK_NULL, &output);
	OK(n == 0 && output.u.oocs.len == 3 && hawk_comp_oochars(output.u.oocs.ptr, 3, nul, 3, 0) == 0,
	   "length-bearing literal templates preserve embedded NUL");
	if (n == 0) hawk_mus_freemem(first, output.u.oocs.ptr);
	hawk_mus_close(first);
	hawk_mus_close(second);
}


static void stock_partial_directories (void)
{
#if defined(_WIN32) || (defined(HAVE_UNISTD_H) && !defined(__OS2__) && !defined(__DOS__))
	hawk_mus_t* mus;
	hawk_mus_iostd_t input;
	hawk_ntime_t now;
	char dir[80], extra[100], main_path[100], near_path[100], far_path[256];
	char far_name[90], content[140], dirs[230];
	hawk_ooch_t oo_main[100], oo_dirs[230];
	FILE* file;
	hawk_oow_t i;
	int n;

	if (hawk_get_ntime(&now) <= -1) { bail_out("cannot get directory probe time"); return; }
	snprintf(dir, sizeof(dir), "mus-dir-%lu-%lu", (unsigned long)now.sec, (unsigned long)now.nsec);
	snprintf(extra, sizeof(extra), "%s/extra", dir);
#if defined(_WIN32)
	if (_mkdir(dir) <= -1 || _mkdir(extra) <= -1) { bail_out("cannot create partial directory"); return; }
#else
	if (mkdir(dir, 0700) <= -1 || mkdir(extra, 0700) <= -1) { bail_out("cannot create partial directory"); return; }
#endif
	snprintf(main_path, sizeof(main_path), "%s/main", dir);
	snprintf(near_path, sizeof(near_path), "%s/near.mustache", dir);
	snprintf(far_name, sizeof(far_name), "%s-far", dir);
	snprintf(far_path, sizeof(far_path), "%s/%s", extra, far_name);
	snprintf(content, sizeof(content), "{{>near}} {{>%s}}", far_name);
#if defined(_WIN32)
	snprintf(dirs, sizeof(dirs), "%s/missing;%s", dir, extra);
#else
	snprintf(dirs, sizeof(dirs), "%s/missing:%s", dir, extra);
#endif
	file = fopen(main_path, "wb");
	if (!file) { bail_out("cannot create directory template"); goto cleanup; }
	fputs(content, file);
	fclose(file);
	file = fopen(near_path, "wb");
	if (!file) { bail_out("cannot create nearby partial"); goto cleanup; }
	fputs("near", file);
	fclose(file);
	file = fopen(far_path, "wb");
	if (!file) { bail_out("cannot create distant partial"); goto cleanup; }
	fputs("far", file);
	fclose(file);
	for (i = 0; main_path[i]; i++) oo_main[i] = (unsigned char)main_path[i];
	oo_main[i] = 0;
	for (i = 0; dirs[i]; i++) oo_dirs[i] = (unsigned char)dirs[i];
	oo_dirs[i] = 0;
	mus = hawk_mus_openstd(0, HAWK_NULL);
	if (!mus) { bail_out("cannot open directory probe"); goto cleanup; }
	memset(&input, 0, sizeof(input));
	input.type = HAWK_MUS_IOSTD_FILE;
	input.u.file.path = oo_main;
	n = hawk_mus_setopt(mus, HAWK_MUS_OPT_PARTIALDIRS, oo_dirs);
	oo_dirs[0] = 'X';
	OK(n == 0 && hawk_mus_compilestd(mus, &input) == 0 && render_text(mus, HAWK_T("near far")) == 0,
	   "partials resolve beside the template and through a copied directory list");
	file = fopen(far_path, "wb");
	if (!file) { bail_out("cannot update partial"); hawk_mus_close(mus); goto cleanup; }
	fputs("updated", file);
	fclose(file);
	OK(render_text(mus, HAWK_T("near updated")) == 0, "partial changes are visible on the next rendering");
	remove(far_path);
	OK(render_text(mus, HAWK_T("near ")) == 0, "absent stock partials render empty without an error");
	hawk_mus_close(mus);
cleanup:
	remove(main_path);
	remove(near_path);
	remove(far_path);
#if defined(_WIN32)
	_rmdir(extra); _rmdir(dir);
#else
	rmdir(extra); rmdir(dir);
#endif
#else
	skip(3, "partial directory fixture requires mkdir/rmdir");
#endif
}

int main (void)
{
	setvbuf(stdout, HAWK_NULL, _IONBF, 0);
	no_plan();
	conformance();
	errors();
	allocation_failures();
	stock_files();
	character_and_instance_tests();
	stock_partial_directories();
	return exit_status();
}
