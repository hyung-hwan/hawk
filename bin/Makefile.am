AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir) \
	-DLANGDIR=\"$(LANGDIR)\" \
	-DHOMELANGDIR=\"$(HOMELANGDIR)\" \
	-DSYSCONFDIR=\"$(SYSCONFDIR)\"

CFLAGS_COMMON =
LDFLAGS_COMMON = -L$(abs_builddir)/../lib -L$(libdir)

## specify the libhawk.la file instead of -lhawk. 
##Use -l for third party libraries for automatic inclusion to DEPENDENCIES .
LIBADD_COMMON = ../lib/libhawk.la

bin_PROGRAMS = hawk

langdir = $(LANGDIR)
lang_DATA = $(srcdir)/hawk-ko.po  $(srcdir)/hawk-zh_CN.po

EXTRA_DIST = hawk.pot hawk-ko.po hawk-zh_CN.po

##################################################
# hawk
##################################################
hawk_SOURCES = cut.c hawk.c m4.c sed.c sh.c main.c main.h
hawk_CPPFLAGS = $(CPPFLAGS_COMMON)
hawk_CFLAGS = $(CFLAGS_COMMON)
hawk_LDFLAGS = $(LDFLAGS_COMMON)
##
## -lm is omitted from dependency_libs in libhawk.la because
## libhawk contains some C++ code and postdeps_CXX contains -lm.
##
## Under the default libtool configuration, postdeps is empty.
## Under the C++ tagged libtool configuration, postdeps is set to 
## the value of postdeps_CXX.
##
## libtool doesn't include -lm in dependency_libs in libhawk.la 
## because libtool --tag=CXX is used for mixed C++ code.
##
## specify -lm to a C only target to satisify dependency regardless
## of different behavior depending on the libtool tag.
##
hawk_LDADD = $(LIBADD_COMMON) $(LIBM)
hawk_DEPENDENCIES = $(srcdir)/hawk.pot $(srcdir)/hawk-ko.po $(srcdir)/hawk-zh_CN.po $(LIBADD_COMMON)


$(srcdir)/hawk.pot: $(hawk_SOURCES)
	xgettext --keyword=_ --keyword=__ -o $@ $^
	xgettext --keyword=HAWK_T -o $@ -j $(top_srcdir)/lib/err.c

$(srcdir)/hawk-ko.po: $(srcdir)/hawk.pot
	if [ -f $@ ]; then msgmerge --update $@ $<; else msginit --input=$< --locale=ko --output=$@; fi

$(srcdir)/hawk-zh_CN.po: $(srcdir)/hawk.pot
	if [ -f $@ ]; then msgmerge --update $@ $<; else msginit --input=$< --locale=zh_CN --output=$@; fi

if ENABLE_HAWKGO

## the attempt to compose a proper procedure using a regular compiler failed.
## e.g ./configure GOC=opt/go/pkg/tool/linux_amd64/
## while go.m4 is included in autoconf 2.71, the support for the go language
## is very limited and the go language itself is pursuing the module based builder.
##
## Go sources are explicit dependencies of the custom build recipe. Do not
## create fake .o files: main.go/main.c and hawk.go/hawk.c share basenames,
## which would let make select a C compilation rule for the Go dependencies.

bin_PROGRAMS += hawkgo
hawkgo_SOURCES =
hawkgo_DEPENDENCIES = \
	$(top_srcdir)/hawk.go \
	$(top_srcdir)/hawk-inst.go \
	$(top_srcdir)/hawk-io.go \
	$(top_srcdir)/hawk-src.go \
	$(top_srcdir)/hawk-http.go \
	$(top_srcdir)/mod/hawk-http.h \
	$(top_srcdir)/hawk-httpd.go \
	$(top_srcdir)/hawk-httpd-req.go \
	$(top_srcdir)/go.mod \
	$(srcdir)/getopt.go \
	$(srcdir)/main.go \
	$(srcdir)/hawk.go \
	$(srcdir)/httpd.go
EXTRA_DIST += $(hawkgo_DEPENDENCIES)

## let the linker to move hawkgo.bin to the actual target
##hawkgo_LINK = mv -f hawkgo.bin hawkgo$(EXEEXT) || echo "FAILED TO LINK"
hawkgo_LINK =

if ENABLE_STATIC
CGO_CFLAGS_ADD=-static
CGO_LDFLAGS_ADD=-static
else
CGO_CFLAGS_ADD=
CGO_LDFLAGS_ADD=
endif

if ENABLE_DEBUG
##GO_BUILD_OPTS=-msan -asan -x
##GO_BUILD_OPTS=-race -gcflags="-m=3" -x
GO_BUILD_OPTS=-race -gcflags="-m"
else
GO_BUILD_OPTS=
endif

hawkgo$(EXEEXT): ../lib/libhawk.la $(hawkgo_DEPENDENCIES)
	##cp -pf $(top_srcdir)/go.mod $(builddir)/go.mod >/dev/null 2>&1 || true
	##chmod u+w $(builddir)/go.mod ## with `make distcheck`, the echo's redirection to the file fails without this permission change
	[ -f $(srcdir)/go.sum ] && cp -pf $(srcdir)/go.sum $(builddir)/go.sum >/dev/null 2>&1 || true
	## ---------------------------------------------------------------
	CC=$(CC) \
	CGO_CFLAGS="-I$(abs_top_srcdir)/lib -I$(abs_top_builddir)/lib -I$(abs_top_srcdir)/mod $(CFLAGS) $(CGO_CFLAGS_EXTRA) $(CGO_CFLAGS_ADD)" \
	CGO_LDFLAGS="-L$(abs_top_builddir)/lib -L$(abs_top_builddir)/lib/.libs -lhawk -ldl $(LIBM) $(PTY_LIBS) $(SOCKET_LIBS) $(CGO_LDFLAGS_EXTRA) $(CGO_LDFLAGS_ADD)"  \
	go build -C $(srcdir) -ldflags "-X 'main.BINDIR=$(bindir)' -X 'main.SBINDIR=$(sbindir)' -X 'main.LIBDIR=$(libdir)' -X 'main.SYSCONFDIR=$(sysconfdir)'" $(GO_BUILD_OPTS) -o $(abs_builddir)/$@ -modfile $(abs_top_srcdir)/go.mod $(abs_srcdir)/main.go $(abs_srcdir)/hawk.go $(abs_srcdir)/httpd.go $(abs_srcdir)/getopt.go
	## ---------------------------------------------------------------
	go clean -C $(srcdir) -x -modfile $(abs_top_srcdir)/go.mod

endif
