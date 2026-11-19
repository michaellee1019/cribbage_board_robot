# Build on the target architecture, or supply a Linux cross compiler in CXX.
CXX ?= c++
CC ?= cc
CXXFLAGS ?= -O2
PYTHON ?= python3
BUILD ?= build
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  LIBRARY := $(BUILD)/libscorebot.dylib
  SHARED_FLAGS := -dynamiclib -Wl,-install_name,@rpath/libscorebot.dylib
  RPATH := @loader_path
else
  LIBRARY := $(BUILD)/libscorebot.so
  SHARED_FLAGS := -shared -Wl,-soname,libscorebot.so
  RPATH := $$ORIGIN
endif
INCLUDES := -Iinclude -I../lib/scorebot/src
WARNINGS := -Wall -Wextra -Wpedantic -Werror
COMPILE_FLAGS := -std=c++17 -fno-exceptions -fPIC -fvisibility=hidden $(WARNINGS)
SOURCES := scorebot.cpp ../src/BoardRole.cpp
HEADERS := include/scorebot.h $(wildcard ../lib/scorebot/src/*.hpp)
ifneq ($(SCOREBOT_LEADER_NODE_ID),)
  IDENTITY_FLAGS += -DSCOREBOT_LEADER_NODE_ID=$(SCOREBOT_LEADER_NODE_ID)
endif
ifneq ($(SCOREBOT_LEADER_BLUETOOTH_MAC),)
  IDENTITY_FLAGS += -DSCOREBOT_LEADER_BLUETOOTH_MAC=$(SCOREBOT_LEADER_BLUETOOTH_MAC)
endif

.PHONY: all check clean FORCE
all: $(LIBRARY)

# Tiny build: always rebuild so an identity/flags change cannot leave a stale
# authority in the output directory. No generated or architecture-shared binary.
$(LIBRARY): $(SOURCES) $(HEADERS) Makefile FORCE
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(IDENTITY_FLAGS) $(CXXFLAGS) $(COMPILE_FLAGS) $(INCLUDES) $(SHARED_FLAGS) $(SOURCES) $(LDFLAGS) -o $@

$(BUILD)/test_c_api: ../test/native/test_c_api.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) -UNDEBUG -std=c11 $(WARNINGS) -Iinclude -c $< -o $(BUILD)/test_c_api.o
	$(CXX) $(BUILD)/test_c_api.o -L$(BUILD) -lscorebot -Wl,-rpath,'$(RPATH)' $(LDFLAGS) -o $@

check: $(BUILD)/test_c_api
	$(BUILD)/test_c_api
	PYTHONPATH=python SCOREBOT_LIBRARY=$(abspath $(LIBRARY)) $(PYTHON) -m unittest discover -s ../test/python -v

clean:
	rm -rf $(BUILD)
