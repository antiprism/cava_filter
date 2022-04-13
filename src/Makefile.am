SUBDIRS = cavacore

bin_PROGRAMS = cava_filter

cava_filter_SOURCES = \
	cava_filter.cpp programopts.cpp \
	status_msg.cpp ultragetopt.cpp utils.cpp \
	\
	programopts.hpp status_msg.hpp \
	ultragetopt.hpp utils.hpp

cava_filter_LDADD = cavacore/libcavacore.la

cava_filter_LDFLAGS = -lfftw3 -lm
