################################################################################
# Host unit tests for IS19's pure logic modules
#
# These modules include no kernel or SDK headers, so they build and run on
# a laptop with any C99 compiler.  No hardware is needed.
#
#   make            build and run every test (exit code 1 if any fails)
#   make SAN=1      the same with AddressSanitizer and UBSan (gcc/clang);
#                   rebuilds whenever SAN, CC or the flags change, and the
#                   output's [INFO] lines show the compiler and flags used
#   make clean
#
# Save the output to testing/<subsystem>/logs/ and record the result in
# the matching TEST_LOG.md, as for the on-target tests.
################################################################################

CC      ?= cc
APP     := ../../app_program
CFLAGS  := -std=c99 -Wall -Wextra -Wshadow -Wpedantic -Werror -g -O1 \
           -I$(APP) -I.

LDLIBS  := -lm

ifeq ($(SAN),1)
CFLAGS  += -fsanitize=address,undefined -fno-sanitize-recover=all \
           -fno-omit-frame-pointer
endif

# The pure modules under test, built from their real source files.  A
# module or suite that is not in the tree yet (its pull request is not
# merged) is simply left out.
APP_SRCS := $(wildcard $(APP)/common/fixmath.c \
                       $(APP)/common/bitpack.c \
                       $(APP)/subsystem1/cmd_parse.c \
                       $(APP)/subsystem1/tlm_json.c \
                       $(APP)/subsystem1/tlm_format.c \
                       $(APP)/subsystem1/tlm_msgs.c \
                       $(APP)/subsystem2/motion_ctl.c \
                       $(APP)/subsystem2/motion_msgs.c \
                       $(APP)/subsystem3/bc_decode.c \
                       $(APP)/subsystem3/bc_capture.c \
                       $(APP)/subsystem3/line_track.c \
                       $(APP)/subsystem3/steering.c \
                       $(APP)/subsystem3/letter_map.c \
                       $(APP)/subsystem3/line_keep.c \
                       $(APP)/subsystem3/line_msgs.c \
                       $(APP)/subsystem4/imu_calc.c \
                       $(APP)/subsystem4/imu_msgs.c \
                       $(APP)/subsystem4/safety_latch.c \
                       $(APP)/subsystem5/scan_logic.c \
                       $(APP)/subsystem5/scan_msgs.c \
                       $(APP)/navigation/nav_sm.c)

SUITES    := $(patsubst test_%.c,%,$(filter-out test_main.c, \
                                     $(wildcard test_*.c)))
TEST_SRCS := unit.c test_main.c $(addprefix test_,$(addsuffix .c,$(SUITES))) \
             $(wildcard ref/*.c)

# test_main.c runs the suites that are present (TEST_SUITE_COMMON ...).
CFLAGS  += $(foreach s,$(SUITES), \
             -DTEST_SUITE_$(shell echo $(s) | tr '[:lower:]' '[:upper:]'))

# The compiler and flags run_tests was built with.  run_tests depends on
# this file, which changes whenever CC, SAN or the flags change, so
# "make" then "make SAN=1" (or another CC) always rebuilds instead of
# running the old binary again.
BUILD_INFO := $(CC) $(CFLAGS) $(LDLIBS)

.PHONY: all clean FORCE

all: run_tests
	@echo "[INFO] host compiler: $$($(CC) --version | head -n 1)"
	@echo "[INFO] host flags: $(CFLAGS)"
	./run_tests

run_tests.flags: FORCE
	@echo '$(BUILD_INFO)' | cmp -s - $@ || echo '$(BUILD_INFO)' > $@

run_tests: $(TEST_SRCS) $(APP_SRCS) $(wildcard *.h ref/*.h) \
           $(wildcard $(APP)/*.h $(APP)/*/*.h) run_tests.flags
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(APP_SRCS) $(LDLIBS)

clean:
	rm -f run_tests run_tests.exe run_tests.flags
