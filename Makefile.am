ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src

EXTRA_DIST = README.md NEWS AUTHORS COPYING

docdir = @docdir@
doc_DATA = README.md NEWS AUTHORS COPYING


format_all:
	for f in src/*.cpp src/*.hpp ; do \
	  clang-format -style=file -i $$f; \
	done

