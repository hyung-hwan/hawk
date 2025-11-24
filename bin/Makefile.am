AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)
CFLAGS_COMMON =
LDFLAGS_COMMON = -L$(abs_builddir)/../lib -L$(libdir)

## specify the libhawk.la file instead of -lhawk. 
##Use -l for third party libraries for automatic inclusion to DEPENDENCIES .
LIBADD_COMMON = ../lib/libhawk.la

bin_PROGRAMS = hawk

##################################################
# hawk
##################################################
hawk_SOURCES = cut.c hawk.c sed.c main.c main.h
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



if ENABLE_HAWKGO

## the attempt to compose a proper procedure using a regular compiler failed.
## e.g ./configure GOC=opt/go/pkg/tool/linux_amd64/
## while go.m4 is included in autoconf 2.71, the support for the go language
## is very limited and the go language itself is pursuing the module based builder.
##
## the following is to trick autoconf/automake as if it's building go files with
## a real compiler whereas the actual building is done thru `go build`

bin_PROGRAMS += hawkgo
hawkgo_SOURCES = \
	../go.mod \
	../hawk.go \
	../hawk-inst.go \
	hawkgo.go
##hawkgo_DEPENDENCIES = 

## let the linker to move hawkgo.bin to the actual target
##hawkgo_LINK = mv -f hawkgo.bin hawkgo$(EXEEXT) || echo "FAILED TO LINK"
hawkgo_LINK =

if ENABLE_STATIC
CGO_CFLAGS_ADD="-static"
CGO_LDFLAGS_ADD="-static"
else
CGO_CFLAGS_ADD=""
CGO_LDFLAGS_ADD=""
endif

hawkgo$(EXEEXT): ../lib/libhawk.la $(hawkgo_OBJECTS)
	cp -pf $(top_srcdir)/go.mod $(builddir)/go.mod >/dev/null 2>&1 || true
	chmod u+w $(builddir)/go.mod ## with `make distcheck`, the echo's redirection to the file fails without this permission change
	[ -f $(srcdir)/go.sum ] && cp -pf $(srcdir)/go.sum $(builddir)/go.sum >/dev/null 2>&1 || true
	## ---------------------------------------------------------------
	CC=$(CC) \
	CGO_CFLAGS="-I$(abs_top_srcdir)/lib -I$(abs_top_builddir)/lib $(CFLAGS) $(CGO_CFLAGS_EXTRA) $(CGO_CFLAGS_ADD)" \
	CGO_LDFLAGS="-L$(abs_top_builddir)/lib -L$(abs_top_builddir)/lib/.libs -lhawk -ldl $(LIBM) $(CGO_LDFLAGS_EXTRA) $(CGO_LDFLAGS_ADD)"  \
	go build -C $(srcdir) -ldflags "-X 'main.BINDIR=$(bindir)' -X 'main.SBINDIR=$(sbindir)' -X 'main.LIBDIR=$(libdir)' -X 'main.SYSCONFDIR=$(sysconfdir)'" -x -o $(abs_builddir)/$@ -modfile $(abs_builddir)/go.mod  $(abs_srcdir)/hawkgo.go
	## ---------------------------------------------------------------
	go clean -C $(srcdir) -x -modfile $(abs_builddir)/go.mod

## the go to o recipe is fake to deceive make
.go.o:
	echo $< > $@

.mod.o:
	echo $< > $@

endif
