BTSTACK_ROOT =  ../..
PYTHON = python3

GENERIC_FLAGS    := -g -Wall -Wextra
CPPUTEST_CFLAGS  := ${shell pkg-config --cflags cpputest}
CPPUTEST_LDFLAGS := ${shell pkg-config --libs   cpputest}

# CppuTest from pkg-config
CFLAGS   += ${CPPUTEST_CFLAGS}
CXXFLAGS += ${CPPUTEST_CFLAGS}
LDFLAGS  += ${CPPUTEST_LDFLAGS}

# General
CFLAGS   += ${GENERIC_FLAGS} -std=gnu11
CXXFLAGS += ${GENERIC_FLAGS} -std=gnu++17

# Coverage
CFLAGS_COVERAGE   = ${CFLAGS} -Ibuild-coverage -fprofile-arcs -ftest-coverage
CXXFLAGS_COVERAGE = ${CXXFLAGS} -Ibuild-coverage -fprofile-arcs -ftest-coverage
LDFLAGS_COVERAGE  = ${LDFLAGS} -fprofile-arcs -ftest-coverage

# Address sanitiser
CFLAGS_ASAN     = ${CFLAGS} -Ibuild-asan -fsanitize=address -DHAVE_ASSERT
CXXFLAGS_ASAN   = ${CXXFLAGS} -Ibuild-asan -DCPPUTEST_MEM_LEAK_DETECTION_DISABLED -fsanitize=address -DHAVE_ASSERT
LDFLAGS_ASAN    = ${LDFLAGS} -fsanitize=address

LDFLAGS += -lCppUTest -lCppUTestExt

build-%:
	mkdir -p $@

build-asan/%.h: %.gatt | build-asan
	${PYTHON} ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-coverage/%.h: %.gatt| build-coverage
	${PYTHON} ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

build-coverage/%.o: %.c | build-coverage
	${CC} -c $(CFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.c | build-asan
	${CC} -c $(CFLAGS_ASAN) $< -o $@

build-coverage/%.o: %.cpp | build-coverage
	${CXX} -c $(CXXFLAGS_COVERAGE) $< -o $@

build-asan/%.o: %.cpp | build-asan
	${CXX} -c $(CXXFLAGS_ASAN) $< -o $@

build-coverage/%: build-coverage/%.o | build-coverage
	${CXX} $^ ${LDFLAGS_COVERAGE} -o $@

build-asan/%: build-asan/%.o | build-asan
	${CXX} $^ ${LDFLAGS_ASAN} -o $@

