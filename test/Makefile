ifeq ($(OS), Windows_NT)
    LIBPATH := /mingw64/lib
    OSPRETTY := Windows
else
    ifeq ($(shell uname -s), Darwin)
        OSPRETTY := macOS
    else
        OSPRETTY := Linux
    endif
    LIBPATH := /usr/local/lib
endif
CXXVER := 14
CXXFLAGS := -std=c++$(CXXVER) -O3 -Wall -Wextra -Wno-missing-braces -Wold-style-cast
ifeq ($(OSPRETTY), macOS)
    CXXFLAGS += -mmacosx-version-min=10.11 -Wunguarded-availability
else
    ifeq ($(OSPRETTY), Windows)
        CXXFLAGS += -Wno-deprecated-copy -static
    endif
    CXXFLAGS += -pthread
endif
CXXFLAGS += -I..
LDLIBS := ../libpazaudio.a
ifeq ($(OSPRETTY), macOS)
#    LDLIBS += -framework ??
else
    LDLIBS += $(LIBPATH)/libportaudio.a
    ifeq ($(OSPRETTY), Linux)
        LDLIBS += -lasound
    else
        LDLIBS += -lwinmm -lole32 -lsetupapi
    endif
endif

SRC := $(wildcard *.cpp)
EXE := $(SRC:.cpp=)

.PHONY: $(SRC)

all: $(EXE)

clean:
	$(RM) $(EXE)

print-% : ; @echo $* = $($*)
