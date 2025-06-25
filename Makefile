CXX       	?= g++
TAR		?= bsdtar
PREFIX	  	?= /usr
MANPREFIX	?= $(PREFIX)/share/man
LOCALEDIR	?= $(PREFIX)/share/locale
VARS  	  	?= -DENABLE_NLS=1

DEBUG 		?= 1
# https://stackoverflow.com/a/1079861
# WAY easier way to build debug and release builds
ifeq ($(DEBUG), 1)
        BUILDDIR  = build/debug
        CXXFLAGS := -ggdb3 -Wall -Wextra -Wpedantic -Wno-unused-parameter -DDEBUG=1 -fsanitize=address $(DEBUG_CXXFLAGS) $(CXXFLAGS)
        LDFLAGS	 += -fsanitize=address
else
	# Check if an optimization flag is not already set
	ifneq ($(filter -O%,$(CXXFLAGS)),)
    		$(info Keeping the existing optimization flag in CXXFLAGS)
	else
    		CXXFLAGS := -O3 $(CXXFLAGS)
	endif
        BUILDDIR  = build/release
endif

NAME		 = ulpm
TARGET		?= $(NAME)
OLDVERSION	 = 0.0.0
VERSION    	 = 0.0.1
SRC	 	 = $(wildcard src/*.cpp)
OBJ	 	 = $(SRC:.cpp=.o)
LDFLAGS   	+= -L./$(BUILDDIR) -lfmt
CXXFLAGS  	?= -mtune=generic -march=native
CXXFLAGS        += -fvisibility=hidden -Iinclude -std=c++20 $(VARS) -DVERSION=\"$(VERSION)\" -DLOCALEDIR=\"$(LOCALEDIR)\"

all: genver fmt $(TARGET)

fmt:
ifeq ($(wildcard $(BUILDDIR)/libfmt.a),)
	mkdir -p $(BUILDDIR)
	make -C src/libs/fmt BUILDDIR=$(BUILDDIR)
endif

genver: ./scripts/generateVersion.sh
ifeq ($(wildcard include/version.h),)
	./scripts/generateVersion.sh
endif

$(TARGET): genver fmt $(OBJ)
	mkdir -p $(BUILDDIR)
	sh ./scripts/generateVersion.sh
	$(CXX) $(OBJ) -o $(BUILDDIR)/$(TARGET) $(LDFLAGS)
	cd $(BUILDDIR)/ && ln -sf $(TARGET) cufetch

locale:
	scripts/make_mo.sh locale/

clean:
	rm -rf $(BUILDDIR)/$(TARGET) $(OBJ)

distclean:
	rm -rf $(BUILDDIR) $(OBJ)
	find . -type f -name "*.tar.gz" -exec rm -rf "{}" \;
	find . -type f -name "*.o" -exec rm -rf "{}" \;
	find . -type f -name "*.a" -exec rm -rf "{}" \;

updatever:
	sed -i "s#$(OLDVERSION)#$(VERSION)#g" $(wildcard .github/workflows/*.yml) compile_flags.txt
	sed -i "s#Project-Id-Version: $(NAME) $(OLDVERSION)#Project-Id-Version: $(NAME) $(VERSION)#g" po/*

.PHONY: $(TARGET) updatever distclean fmt all locale
