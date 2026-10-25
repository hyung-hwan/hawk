AUTOMAKE_OPTIONS = nostdinc

EXTRA_DIST = README.md hawk.1.md hawkgo.1.md \
	hawk-language.7.md hawk-modules.7.md

nodist_man_MANS = hawk.1 hawk-language.7 hawk-modules.7
if ENABLE_HAWKGO
nodist_man_MANS += hawkgo.1
endif

CLEANFILES = hawk.1 hawkgo.1 hawk-language.7 hawk-modules.7 \
	hawk.1.tmp hawkgo.1.tmp hawk-language.7.tmp hawk-modules.7.tmp

## Override with a runnable host Hawk when cross-compiling.
## This variable names one executable, not a shell command.
MAN_HAWK = $(top_builddir)/bin/hawk$(EXEEXT)

man-pages: $(nodist_man_MANS)

hawk.1: $(srcdir)/hawk.1.md
hawkgo.1: $(srcdir)/hawkgo.1.md
hawk-language.7: $(srcdir)/hawk-language.7.md
hawk-modules.7: $(srcdir)/hawk-modules.7.md

## for each man page file (.1 and .7), the following common rules show apply
## together with the individual dependency above.
## each conversion uses its own temporary file so parallel builds cannot
## interfere with another page.
$(nodist_man_MANS): $(top_srcdir)/tools/md2man.hawk Makefile $(MAN_HAWK)
	set -e; trap 'rm -f "$@.tmp"' 0; \
	set --; \
	if test "$(MAN_HAWK)" = "$(top_builddir)/bin/hawk$(EXEEXT)"; then \
		set -- --modlibdirs "$(abs_top_builddir)/lib/.libs"; \
	fi; \
	"$(MAN_HAWK)" "$$@" --tolerant=off \
		-v footer="Hawk $(PACKAGE_VERSION)" -v header="Hawk Manual" \
		-f "$(top_srcdir)/tools/md2man.hawk" "$(srcdir)/$@.md" \
		> "$@.tmp"; \
	mv -f "$@.tmp" "$@"
