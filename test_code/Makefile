# ===================== User Configurable =====================
TARGET_NAME = hello_world
SRC         = hello_world.c

# ===================== Tools =====================
CC       = gcc
OBJDUMP  = objdump
READELF  = readelf
NM       = nm
SIZE     = size
STRINGS  = strings
OBJCOPY  = objcopy

# ===================== Directories and File Names =====================
OBJDIR = obj
TARGET = $(OBJDIR)/$(TARGET_NAME)
OBJ    = $(OBJDIR)/$(SRC:.c=.o)

# ===================== Flags =====================
CFLAGS  = -Wall -O2
LDFLAGS = -Wl,-Map=$(OBJDIR)/$(TARGET_NAME).map

# ===================== Output Files =====================
PREPROCESSED = $(OBJDIR)/$(TARGET_NAME).i
ASSEMBLY     = $(OBJDIR)/$(TARGET_NAME).s
SYMBOLS      = $(OBJDIR)/$(TARGET_NAME).sym
SECTIONS     = $(OBJDIR)/$(TARGET_NAME).sections
DDR_INFO     = $(OBJDIR)/ddr_load.info
DISASM       = $(OBJDIR)/$(TARGET_NAME).disasm
SORTED_SYMS  = $(OBJDIR)/$(TARGET_NAME)_sorted_symbols.txt
SIZE_INFO    = $(OBJDIR)/$(TARGET_NAME).size
STRINGS_OUT  = $(OBJDIR)/$(TARGET_NAME).strings
BIN_OUT      = $(OBJDIR)/$(TARGET_NAME).bin
HEX_OUT      = $(OBJDIR)/$(TARGET_NAME).hex
ELF_HEADER   = $(OBJDIR)/$(TARGET_NAME).elfheader
FUNC_LIST    = $(OBJDIR)/$(TARGET_NAME).functions

# ===================== Build Targets =====================
all: $(OBJDIR) $(TARGET) extras

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

extras: $(TARGET)
	@echo "Generating intermediate and debug outputs..."

	# C Preprocessing
	$(CC) -E $(SRC) $(CFLAGS) > $(PREPROCESSED)

	# C to Assembly
	$(CC) -S $(SRC) $(CFLAGS) -o $(ASSEMBLY)

	# Symbols Table
	$(OBJDUMP) -t $(TARGET) > $(SYMBOLS)

	# ELF Sections
	$(READELF) -S $(TARGET) > $(SECTIONS)

	# ELF Program Headers (LOAD)
	$(READELF) -l $(TARGET) | grep -A 10 "LOAD" > $(DDR_INFO)

	# Full Disassembly
	$(OBJDUMP) -d $(TARGET) > $(DISASM)

	# Sorted Symbols
	$(NM) -n $(TARGET) > $(SORTED_SYMS)

	# Size Info
	$(SIZE) $(TARGET) > $(SIZE_INFO)

	# Extract Strings
	$(STRINGS) $(TARGET) > $(STRINGS_OUT)

	# Binary output
	$(OBJCOPY) -O binary $(TARGET) $(BIN_OUT)

	# Intel HEX output
	$(OBJCOPY) -O ihex $(TARGET) $(HEX_OUT)

	# ELF Header
	$(READELF) -h $(TARGET) > $(ELF_HEADER)

	# Function list (with demangling)
	$(NM) -n --defined-only $(TARGET) | c++filt > $(FUNC_LIST)

# ===================== Clean Up =====================
clean:
	rm -rf $(OBJDIR)

.PHONY: all clean extras

