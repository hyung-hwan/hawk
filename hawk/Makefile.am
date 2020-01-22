AUTOMAKE_OPTION = foreign
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST =
SUBDIRS =

if ENABLE_STATIC_MODULE
SUBDIRS += tools mod lib bin samples
else
SUBDIRS += tools lib mod bin samples
endif

DIST_SUBDIRS = $(SUBDIRS) t

distclean-local:
	@rm -rf $(top_srcdir)/autom4te.cache

######################################################################

TARGET ?= @build_cpu@

rpm: dist-gzip
	mkdir -p "@abs_builddir@/pkgs/RPM/BUILD"
	mkdir -p "@abs_builddir@/pkgs/RPM/SOURCES"
	mkdir -p "@abs_builddir@/pkgs/RPM/SRPMS"
	mkdir -p "@abs_builddir@/pkgs/RPM/RPMS"
	cp @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.gz "@abs_builddir@/pkgs/RPM/SOURCES"
	rpmbuild --define "_topdir @abs_builddir@/pkgs/RPM" -ba @abs_builddir@/pkgs/hawk.spec --target=$(TARGET)

