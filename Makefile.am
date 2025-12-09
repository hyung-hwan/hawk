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
	go clean -C $(srcdir) -x -modfile $(abs_srcdir)/go.mod -cache

######################################################################
if ENABLE_HAWKGO

if ENABLE_STATIC
CGO_CFLAGS_ADD=-static
CGO_LDFLAGS_ADD=-static
else
CGO_CFLAGS_ADD=
CGO_LDFLAGS_ADD=
endif

hawkgo.check:
	CC=$(CC) \
	CGO_CFLAGS="-I$(abs_srcdir)/lib -I$(abs_builddir)/lib $(CFLAGS) $(CGO_CFLAGS_EXTRA) $(CGO_CFLAGS_ADD)" \
	CGO_LDFLAGS="-L$(abs_builddir)/lib -L$(abs_builddir)/lib/.libs -lhawk -ldl $(LIBM) $(CGO_LDFLAGS_EXTRA) $(CGO_LDFLAGS_ADD)"  \
	go test -C $(srcdir) -ldflags "-X 'main.BINDIR=$(bindir)' -X 'main.SBINDIR=$(sbindir)' -X 'main.LIBDIR=$(libdir)' -X 'main.SYSCONFDIR=$(sysconfdir)'" -x -modfile $(abs_srcdir)/go.mod
	go clean -C $(srcdir) -x -modfile $(abs_srcdir)/go.mod

endif
######################################################################

rpm: dist-gzip
	mkdir -p "@abs_builddir@/pkgs/RPM/BUILD"
	mkdir -p "@abs_builddir@/pkgs/RPM/SOURCES"
	mkdir -p "@abs_builddir@/pkgs/RPM/SRPMS"
	mkdir -p "@abs_builddir@/pkgs/RPM/RPMS"
	cp @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.gz "@abs_builddir@/pkgs/RPM/SOURCES"
	rpmbuild --define "_topdir @abs_builddir@/pkgs/RPM" -ba @abs_builddir@/pkgs/hawk.spec --target=@build_cpu@

