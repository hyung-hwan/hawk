AUTOMAKE_OPTION = foreign
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST =
SUBDIRS =

SUBDIRS += tools lib mod bin samples t
#if ENABLE_STATIC_MODULE
#SUBDIRS += tools mod lib bin samples t
#else
#SUBDIRS += tools lib mod bin samples t
#endif

DIST_SUBDIRS = $(SUBDIRS)

distclean-local:
	@rm -rf $(top_srcdir)/autom4te.cache

clean-local:
	rm -rf $(builddir)/go.mod $(builddir)/go.sum
	go clean -C $(srcdir) -x -modfile $(abs_builddir)/go.mod -cache

######################################################################
if ENABLE_HAWKGO
bin_PROGRAMS = hawkgo
hawkgo_SOURCES = \
	hawk.go \
	hawk-inst.go \
	go.mod
hawkgo_DEPENDENCIES = hawkgo.bin

## let the linker to move hawkgo.bin to the actual target
hawkgo_LINK = cp -pf hawkgo.bin hawkgo$(EXEEXT) || echo "FAILED TO LINK"

if ENABLE_STATIC
CGO_CFLAGS_ADD=-static
CGO_LDFLAGS_ADD=-static
else
CGO_CFLAGS_ADD=
CGO_LDFLAGS_ADD=
endif

hawkgo.bin: lib/libhawk.la $(hawkgo_OBJECTS)
	cp -pf $(srcdir)/go.mod $(builddir)/go.mod >/dev/null 2>&1 || true
	chmod u+w $(builddir)/go.mod ## with `make distcheck`, the echo's redirection to the file fails without this permission change
	[ -f $(srcdir)/go.sum ] && cp -pf $(srcdir)/go.sum $(builddir)/go.sum >/dev/null 2>&1 || true
	## ---------------------------------------------------------------
	CC=$(CC) \
	CGO_CFLAGS="-I$(abs_srcdir)/lib -I$(abs_builddir)/lib $(CFLAGS) $(CGO_CFLAGS_EXTRA) $(CGO_CFLAGS_ADD)" \
	CGO_LDFLAGS="-L$(abs_builddir)/lib -L$(abs_builddir)/lib/.libs -lhawk -ldl $(LIBM) $(CGO_LDFLAGS_EXTRA) $(CGO_LDFLAGS_ADD)"  \
	go build -C $(srcdir) -ldflags "-X 'main.BINDIR=$(bindir)' -X 'main.SBINDIR=$(sbindir)' -X 'main.LIBDIR=$(libdir)' -X 'main.SYSCONFDIR=$(sysconfdir)'" -x -o $(abs_builddir)/hawkgo.bin -modfile $(abs_builddir)/go.mod
	## ---------------------------------------------------------------
	go clean -C $(srcdir) -x -modfile $(abs_builddir)/go.mod

hawkgo.check:
	CC=$(CC) \
	CGO_CFLAGS="-I$(abs_srcdir)/lib -I$(abs_builddir)/lib $(CFLAGS) $(CGO_CFLAGS_EXTRA) $(CGO_CFLAGS_ADD)" \
	CGO_LDFLAGS="-L$(abs_builddir)/lib -L$(abs_builddir)/lib/.libs -lhawk -ldl $(LIBM) $(CGO_LDFLAGS_EXTRA) $(CGO_LDFLAGS_ADD)"  \
	go test -C $(srcdir) -ldflags "-X 'main.BINDIR=$(bindir)' -X 'main.SBINDIR=$(sbindir)' -X 'main.LIBDIR=$(libdir)' -X 'main.SYSCONFDIR=$(sysconfdir)'" -x -modfile $(abs_builddir)/go.mod
	go clean -C $(srcdir) -x -modfile $(abs_builddir)/go.mod

# fake recipes to deceive make
.go.o:
	echo $< > $@

.mod.o:
	echo $< > $@

endif
######################################################################

rpm: dist-gzip
	mkdir -p "@abs_builddir@/pkgs/RPM/BUILD"
	mkdir -p "@abs_builddir@/pkgs/RPM/SOURCES"
	mkdir -p "@abs_builddir@/pkgs/RPM/SRPMS"
	mkdir -p "@abs_builddir@/pkgs/RPM/RPMS"
	cp @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.gz "@abs_builddir@/pkgs/RPM/SOURCES"
	rpmbuild --define "_topdir @abs_builddir@/pkgs/RPM" -ba @abs_builddir@/pkgs/hawk.spec --target=@build_cpu@

