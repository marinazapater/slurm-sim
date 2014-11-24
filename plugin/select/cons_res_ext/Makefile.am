# Makefile for select/cons_res_ext plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = select_cons_res_ext.la

# Consumable resources node selection plugin.
select_cons_res_ext_la_SOURCES =  select_cons_res_ext.c select_cons_res_ext.h \
			      job_test_ext.c job_test_ext.h
select_cons_res_ext_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)
