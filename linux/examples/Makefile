##Usage
##    make
##or specify PROJECT=, e.g.:
##    make PROJECT=Trill_print
##

LIB_DIR := ../lib/
CPPFLAGS := -I$(LIB_DIR)
CXXFLAGS := -g
CFLAGS := $(CXXFLAGS)

CC := $(CXX) # ensure CXX is used for linking

LIB_SRCS = $(wildcard $(LIB_DIR)/*.cpp)
LIB_OBJS := $(LIB_SRCS:.cpp=.o)
ALL_PROJECTS := $(shell find * -maxdepth 1 -type d)

PROJECT_BIN := $(PROJECT)/$(PROJECT)
PROJECT_SRCS := $(wildcard $(PROJECT)/*.cpp)
PROJECT_OBJS := $(PROJECT_SRCS:%.cpp=%.o)

ifneq ($(PROJECT),)
$(PROJECT_BIN): $(PROJECT_OBJS) $(LIB_OBJS)
endif

all: ## Build all examples or the one specified as PROJECT=
	for project in $(ALL_PROJECTS); do $(MAKE) --no-print-directory PROJECT=$$project || exit 1; done

clean: ## Clean al build files
	rm -rf */*.o $(LIB_DIR)/*.o

help: ## Show this help
	@fgrep -h "##" $(MAKEFILE_LIST) | fgrep -v fgrep | sed -e 's/^\(.*\): .*##\(.*\)/\1:#\2/' | sed -e 's/^\(.*\)= .* -- \(.*\)/\1=#\2/' | sed 's/^##//' | awk -F"#" '{ printf "%-18s %-1s\n", $$1, $$2}'
	@echo '(default: $(.DEFAULT_GOAL))'

ifneq (,$(PROJECT))
run: $(PROJECT_BIN)
	./$(PROJECT_BIN) $(CL)
endif
