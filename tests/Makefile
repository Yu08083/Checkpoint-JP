ROOT		:=	..
BUILD		:=	build
CXX		?=	g++
CXXFLAGS	:=	-std=c++23 -Wall -Wextra -Werror -I$(ROOT)/3ds/include

TESTS			:=	ntrcard-probe
ntrcard-probe_SRC	:=	ntrcard-probe-test.cpp $(ROOT)/3ds/source/ntrcard.cpp

all: $(addsuffix .run,$(TESTS))

.SECONDARY:

.SECONDEXPANSION:
$(BUILD)/%: $$($$*_SRC) | $(BUILD)
	@$(CXX) $(CXXFLAGS) $^ -o $@

%.run: $(BUILD)/%
	@$<
	@echo "$* host test passed"

$(BUILD):
	@mkdir -p $(BUILD)

clean:
	@rm -rf $(BUILD)

.PHONY: all clean
