#

AUTOMAKE_OPTIONS = foreign
CLEANFILES = core.*

INCLUDES = -I$(top_srcdir)

bin_PROGRAMS = sim_mgr list_trace trace_builder trace_build_from_file sim_ctlr update_trace

sim_mgr_SOURCES = sim_mgr.c
list_trace_SOURCES = list_trace.c
trace_builder_SOURCES = trace_builder.c
trace_build_from_file_SOURCES = trace_build_from_file.c
sim_ctlr_SOURCES = sim_ctlr.c
update_trace_SOURCES = update_trace.c

convenience_libs = $(top_builddir)/src/api/libslurm.o 

sim_mgr_LDADD = $(convenience_libs)
list_trace_LDADD = $(convenience_libs)
trace_builder_LDADD = $(convenience_libs)
trace_build_from_file_LDADD = $(convenience_libs)
update_trace_LDADD = $(convenience_libs)
sim_ctlr_LDADD = $(convenience_libs)

sim_mgr_LDFLAGS = -export-dynamic $(CMD_LDFLAGS) -ldl -lrt -lpthread -lhwloc
list_trace_LDFLAGS = -export-dynamic $(CMD_LDFLAGS) -ldl -lrt -lpthread -lhwloc
trace_builder_LDFLAGS = -export-dynamic $(CMD_LDFLAGS) -ldl -lrt -lpthread -lhwloc
trace_build_from_file_LDFLAGS = -export-dynamic $(CMD_LDFLAGS) -ldl -lrt -lpthread -lhwloc
update_trace_LDFLAGS = -export-dynamic $(CMD_LDFLAGS) -ldl -lrt -lpthread -lhwloc
sim_ctlr_LDFLAGS = -export-dynamic $(CMD_LDFLAGS) -ldl -lrt -lpthread -lhwloc

pkglib_LTLIBRARIES = libslurm_sim.la

libslurm_sim_la_SOURCES = sim_lib.c
libslurm_sim_la_LDFLAGS = $(SO_LDFLAGS) -ldl -lrt -lpthread -lhwloc

force:
$(convenience_libs) : force
	      @cd `dirname $@` && $(MAKE) `basename $@`

