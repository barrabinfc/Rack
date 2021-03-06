ifdef VERSION
FLAGS += -DVERSION=$(VERSION)
endif

# Generate dependency files alongside the object files
FLAGS += -MMD
FLAGS += -g -w # avoid warning messages

# Optimization
FLAGS += -O3 -march=nocona -ffast-math -fno-finite-math-only
FLAGS += -Wall -Wextra -Wno-unused-parameter
ifneq ($(ARCH), mac)
CXXFLAGS += -Wsuggest-override
endif
CXXFLAGS += -std=c++11


ifeq ($(ARCH), lin)
	FLAGS += -DARCH_LIN
endif

ifeq ($(ARCH), mac)
	FLAGS += -DARCH_MAC
	CXXFLAGS += -stdlib=libc++
	LDFLAGS += -stdlib=libc++
	MAC_SDK_FLAGS = -mmacosx-version-min=10.7
	FLAGS += $(MAC_SDK_FLAGS)
	LDFLAGS += $(MAC_SDK_FLAGS)
endif

ifeq ($(ARCH), win)
	FLAGS += -DARCH_WIN
	FLAGS += -D_USE_MATH_DEFINES
endif


OBJECTS += $(patsubst %, build/%.o, $(SOURCES))
DEPS = $(patsubst %, build/%.d, $(SOURCES))

#
# Tests
#
doctest: $(TEST_APP) $(OBJECTS)
	$(CXX) $(FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

-include $(DEPS)


# Final targets
$(TARGET): $(MAIN_APP) $(OBJECTS)
	$(CXX) $(FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)


-include $(DEPS)

build/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<

build/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(FLAGS) $(CXXFLAGS) -c -o $@ $<

build/%.cc.o: %.cc
	@mkdir -p $(@D)
	$(CXX) $(FLAGS) $(CXXFLAGS) -c -o $@ $<

build/%.m.o: %.m
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<
