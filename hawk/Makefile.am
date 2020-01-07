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
