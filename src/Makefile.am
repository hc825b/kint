AM_CXXFLAGS = `llvm-config --cxxflags` -I/class/cs526/sp17-projects/llvm/llvm-3.9.1.src/include -Wall -fno-rtti

noinst_LTLIBRARIES = libsat.la
lib_LTLIBRARIES    = libintck.la libcmpck.la libintglobal.la
bin_PROGRAMS       = intglobal
EXTRA_DIST         = intck cmpck llvm/DataLayout.h llvm/DebugInfo.h llvm/IRBuilder.h

all-local: libintck.la libcmpck.la libintglobal.la
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libintck.so
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libcmpck.so
	@cd $(top_builddir)/lib && $(LN_S) -f ../src/.libs/libintglobal.so
	@cd $(top_builddir)/bin && $(LN_S) -f ../src/intglobal

libsat_la_CPPFLAGS = -I$(top_builddir)/lib
libsat_la_SOURCES  = ValueGen.cc PathGen.cc Diagnostic.cc SMTSolver.cc
libsat_la_SOURCES += ValueGen.h PathGen.h Diagnostic.h SMTSolver.h
libsat_la_SOURCES += SMTBoolector.cc
libsat_la_LIBADD   = -lboolector -llgl
#libsat_la_SOURCES += SMTSonolar.cc
#libsat_la_LIBADD   = -lsonolar
#libsat_la_SOURCES += SMTZ3.cc
#libsat_la_LIBADD   = -lz3 -lgomp
libsat_la_LDFLAGS  = -L$(top_builddir)/lib

libintck_la_SOURCES = IntRewrite.cc IntLibcalls.cc IntSat.cc \
	OverflowIdiom.cc OverflowSimplify.cc \
	LoadRewrite.cc
libintck_la_LIBADD  = libsat.la
libintck_la_LDFLAGS = -module

libcmpck_la_SOURCES = CmpTautology.cc CmpOverflow.cc CmpSat.cc
libcmpck_la_LIBADD  = libsat.la
libcmpck_la_LDFLAGS = -module

libintglobal_la_SOURCES = Annotation.cc KINTCallGraph.cc KINTTaint.cc KINTRange.cc
libintglobal_la_LDFLAGS = `llvm-config --ldflags` `llvm-config --libs analysis irreader` `llvm-config --system-libs analysis irreader`

intglobal_LDFLAGS = -all-static `llvm-config --ldflags` `llvm-config --libs analysis irreader` `llvm-config --system-libs analysis irreader`
intglobal_LDADD  = libintglobal.la
intglobal_SOURCES = IntGlobal.cc CallGraph.cc Taint.cc Range.cc \
	IntGlobal.h CRange.h
