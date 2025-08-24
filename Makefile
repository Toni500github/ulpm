CXX       	?= g++
TAR		?= bsdtar
PREFIX	  	?= /usr
MANPREFIX	?= $(PREFIX)/share/man
LOCALEDIR	?= $(PREFIX)/share/locale
VARS  	  	?= -DENABLE_NLS=1
CXXSTD		?= c++20

DEBUG 		?= 1

COMPILER := $(shell $(CXX) --version | head -n1)

ifeq ($(findstring GCC,$(COMPILER)),GCC)
	export LTO_FLAGS = -flto=auto -ffat-lto-objects
else ifeq ($(findstring clang,$(COMPILER)),clang)
	export LTO_FLAGS = -flto=thin
else
    $(warning Unknown compiler: $(COMPILER). No LTO flags applied.)
endif

# https://stackoverflow.com/a/1079861
# WAY easier way to build debug and release builds
ifeq ($(DEBUG), 1)
        BUILDDIR  := build/debug
	LTO_FLAGS  = -fno-lto
        CXXFLAGS  := -ggdb3 -Wall -Wextra -pedantic -Wno-unused-parameter -fsanitize=address \
			-DDEBUG=1 -fno-omit-frame-pointer $(DEBUG_CXXFLAGS) $(CXXFLAGS)
        LDFLAGS	  += -fsanitize=address -fno-lto -Wl,-rpath,$(BUILDDIR)
else
	# Check if an optimization flag is not already set
	ifneq ($(filter -O%,$(CXXFLAGS)),)
    		$(info Keeping the existing optimization flag in CXXFLAGS)
	else
    		CXXFLAGS := -O3 $(CXXFLAGS)
	endif
	LDFLAGS   += $(LTO_FLAGS)
        BUILDDIR  := build/release
endif

NAME		 = ulpm
TARGET		?= $(NAME)
OLDVERSION	 = 0.0.0
VERSION    	 = 0.0.1
SRC	 	 = $(wildcard src/*.cpp)
OBJ	 	 = $(SRC:.cpp=.o)
LDFLAGS   	+= -L$(BUILDDIR)
LDLIBS		+= $(wildcard $(BUILDDIR)/*.a) -lncurses
CXXFLAGS        += $(LTO_FLAGS) -fvisibility-inlines-hidden -fvisibility=hidden -Iinclude -Iinclude/libs -std=$(CXXSTD) $(VARS) -DVERSION=\"$(VERSION)\" -DLOCALEDIR=\"$(LOCALEDIR)\" -DICONPREFIX=\"$(ICONPREFIX)\"

all: genver fmt tpl getopt-port $(TARGET)

fmt:
ifeq ($(wildcard $(BUILDDIR)/libfmt.a),)
	mkdir -p $(BUILDDIR)
	$(MAKE) -C src/libs/fmt BUILDDIR=$(BUILDDIR) CXXSTD=$(CXXSTD)
endif

tpl:
ifeq ($(wildcard $(BUILDDIR)/libtiny-process-library.a),)
	$(MAKE) -C src/libs/tiny-process-library BUILDDIR=$(BUILDDIR) CXXSTD=$(CXXSTD)
endif

getopt-port:
ifeq ($(wildcard $(BUILDDIR)/getopt.o),)
	$(MAKE) -C src/libs/getopt_port BUILDDIR=$(BUILDDIR)
endif

genver: ./scripts/generateVersion.sh
ifeq ($(wildcard include/version.h),)
	./scripts/generateVersion.sh
endif

$(TARGET): genver fmt tpl getopt-port $(OBJ)
	mkdir -p $(BUILDDIR)
	sh ./scripts/generateVersion.sh
	$(CXX) -o $(BUILDDIR)/$(TARGET) $(OBJ) $(BUILDDIR)/*.o $(LDFLAGS) $(LDLIBS)

locale:
	scripts/make_mo.sh locale/

clean:
	rm -rf $(BUILDDIR)/$(TARGET) $(OBJ)

distclean:
	rm -rf $(BUILDDIR) $(OBJ)
	find . -type f -name "*.tar.gz" -delete
	find . -type f -name "*.o" -delete
	find . -type f -name "*.a" -delete

updatever:
	sed -i "s#$(OLDVERSION)#$(VERSION)#g" $(wildcard .github/workflows/*.yml) compile_flags.txt
	sed -i "s#Project-Id-Version: $(NAME) $(OLDVERSION)#Project-Id-Version: $(NAME) $(VERSION)#g" po/*

.PHONY: $(TARGET) updatever distclean fmt tpl genver clean all locale
