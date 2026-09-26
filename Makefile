# Variables
SRC_DIR := src
OBJ_DIR := obj
EXE := sim
EXE_XLSX := sim_xlsx

CXX := g++
PKG_CFLAGS := $(shell pkg-config --cflags glib-2.0 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs glib-2.0 2>/dev/null)
CPPFLAGS := -Iinclude/ $(PKG_CFLAGS)
CXXFLAGS ?= -O2 -Wall -Wextra -std=c++17 -MMD -MP
LDFLAGS :=
LDLIBS := $(PKG_LIBS)

ifeq ($(SANITIZE),1)
CXXFLAGS += -fsanitize=address -g
LDFLAGS += -fsanitize=address
endif

OPENXLSX_DIR := OpenXLSX
OPENXLSX_BUILD_DIR := $(OPENXLSX_DIR)/build
OPENXLSX_LIB := $(OPENXLSX_BUILD_DIR)/libOpenXLSX.a
OPENXLSX_EXPORT_HDR := $(OPENXLSX_BUILD_DIR)/OpenXLSX-Exports.hpp
XLSX_CPPFLAGS := $(CPPFLAGS) -DENABLE_XLSX -I$(OPENXLSX_DIR) -I$(OPENXLSX_DIR)/headers -I$(OPENXLSX_BUILD_DIR)

SRC := $(filter-out $(SRC_DIR)/XLSXEventLogger.cpp,$(wildcard $(SRC_DIR)/*.cpp))
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
XLSX_SRC := $(wildcard $(SRC_DIR)/*.cpp)
XLSX_OBJ := $(XLSX_SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/xlsx/%.o)

CONFS := $(wildcard configs/*.cfg)
PROGRAMS := $(wildcard programs/*.dat)
OUTPUTS := $(foreach conf,$(CONFS),$(foreach program,$(PROGRAMS),outputs/$(program:programs/%.dat=%).$(conf:configs/%.cfg=%).out))
OUTPUTS_XLSX := $(foreach conf,$(CONFS),$(foreach program,$(PROGRAMS),outputs_xlsx/$(program:programs/%.dat=%).$(conf:configs/%.cfg=%).xlsx))
DIFFS := $(foreach conf,$(CONFS),$(foreach program,$(PROGRAMS),diffs/$(program:programs/%.dat=%).$(conf:configs/%.cfg=%).diff))

.PHONY: all run outputs xlsx clean output-dir diff-dir xlsx-dir

all: $(EXE)

run: $(OUTPUTS) $(DIFFS)

outputs: $(OUTPUTS)

xlsx: $(OUTPUTS_XLSX)

$(EXE): $(OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(EXE_XLSX): $(XLSX_OBJ) $(OPENXLSX_LIB)
	$(CXX) $(LDFLAGS) $(XLSX_OBJ) $(OPENXLSX_LIB) $(LDLIBS) -o $@

$(OPENXLSX_LIB):
	cmake -S $(OPENXLSX_DIR) -B $(OPENXLSX_BUILD_DIR) -DOPENXLSX_LIBRARY_TYPE=STATIC
	cmake --build $(OPENXLSX_BUILD_DIR) --target OpenXLSX

$(OPENXLSX_EXPORT_HDR): $(OPENXLSX_LIB)
	@true

$(OBJ_DIR):
	@mkdir -p $@

$(OBJ_DIR)/xlsx:
	@mkdir -p $@

output-dir:
	@mkdir -p outputs

diff-dir:
	@mkdir -p diffs

xlsx-dir:
	@mkdir -p outputs_xlsx

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/xlsx/%.o: $(SRC_DIR)/%.cpp $(OPENXLSX_EXPORT_HDR) | $(OBJ_DIR)/xlsx
	$(CXX) $(XLSX_CPPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(OBJ:.o=.d) $(XLSX_OBJ:.o=.d)

define output_rules
outputs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).out: $(EXE) $(1) $(2) | output-dir
	@echo "Running ./$(EXE) -p $(1) -c $(2) -j outputs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).json -d > $$@"
	-@./$(EXE) -p $(1) -c $(2) -j outputs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).json -d > $$@ 2>&1
endef
$(foreach program,$(PROGRAMS),$(foreach conf,$(CONFS),$(eval $(call output_rules,$(program),$(conf)))))

define diff_rules
diffs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).diff: outputs/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).out outputs_solution/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).out | diff-dir
	-diff -dw $$< outputs_solution/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).out > $$@
endef
$(foreach program,$(PROGRAMS),$(foreach conf,$(CONFS),$(eval $(call diff_rules,$(program),$(conf)))))

define xlsx_rules
outputs_xlsx/$(1:programs/%.dat=%).$(2:configs/%.cfg=%).xlsx: $(EXE_XLSX) $(1) $(2) | xlsx-dir
	@echo "Running ./$(EXE_XLSX) -p $(1) -c $(2) -x $$@"
	-@./$(EXE_XLSX) -p $(1) -c $(2) -x $$@ 2>&1
endef
$(foreach program,$(PROGRAMS),$(foreach conf,$(CONFS),$(eval $(call xlsx_rules,$(program),$(conf)))))

clean:
	@$(RM) -rf $(OBJ_DIR)/* $(EXE) $(EXE_XLSX) outputs/* outputs_xlsx/* diffs/*
