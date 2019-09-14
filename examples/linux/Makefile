##Usage
##    make
##or specify PROJECT=, e.g.:
##    make PROJECT=Trill_print
##

LIB_DIR := ../../lib/linux
CPPFLAGS := -I$(LIB_DIR)

CC := $(CXX) # ensure CXX is used for linking

LIB_SRCS = $(wildcard $(LIB_DIR)/*.cpp)
LIB_OBJS := $(LIB_SRCS:.cpp=.o)
ALL_PROJECTS := Trill_print Trill_settings_test
ALL_PROJECT_BINS := $(foreach dir,$(ALL_PROJECTS),$(dir)/$(dir))

PROJECT_BIN := $(PROJECT)/$(PROJECT)
PROJECT_SRCS := $(wildcard $(PROJECT)/*.cpp)
PROJECT_OBJS := $(PROJECT_SRCS:%.cpp=%.o)

ifneq ($(PROJECT),)
$(PROJECT_BIN): $(PROJECT_OBJS) $(LIB_OBJS)
endif

all: ## Build all examples or the one specified as PROJECT=
all: $(ALL_PROJECTS_BINS)
	for project in $(ALL_PROJECTS); do $(MAKE) --no-print-directory PROJECT=$$project; done

clean: ## Clean al build files
	rm -rf */*.o $(LIB_DIR)/*.o

help: ## Show this help
	@fgrep -h "##" $(MAKEFILE_LIST) | fgrep -v fgrep | sed -e 's/^\(.*\): .*##\(.*\)/\1:#\2/' | sed -e 's/^\(.*\)= .* -- \(.*\)/\1=#\2/' | sed 's/^##//' | awk -F"#" '{ printf "%-18s %-1s\n", $$1, $$2}'
	@echo '(default: $(.DEFAULT_GOAL))'

