# Elaiksir Firmware Monorepo
# ESP-IDF must be sourced before running build targets:
#   export IDF_PATH=~/Work/esp-idf && source $IDF_PATH/export.sh

PRODUCTS := matcha-bowl liquid-machine
SRCDIRS  := shared matcha-bowl liquid-machine
SOURCES  := $(shell find $(SRCDIRS) -name '*.c' -o -name '*.h' | grep -v build)

.PHONY: all build build-matcha build-liquid clean \
        format format-check lint test help

all: build

# ── Building ─────────────────────────────────────────────

build: build-matcha build-liquid

build-matcha:
	cd matcha-bowl && idf.py build

build-liquid:
	cd liquid-machine && idf.py build

# First-time setup (run once per product)
setup-matcha:
	cd matcha-bowl && idf.py set-target esp32

setup-liquid:
	cd liquid-machine && idf.py set-target esp32p4

# ── Formatting ───────────────────────────────────────────

format:
	clang-format -i $(SOURCES)
	@echo "Formatted $(words $(SOURCES)) files"

format-check:
	@clang-format --dry-run --Werror $(SOURCES) \
		&& echo "Formatting OK" \
		|| (echo "Formatting errors found. Run 'make format' to fix." && exit 1)

# ── Linting ──────────────────────────────────────────────

lint:
	cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		--error-exitcode=1 \
		--quiet \
		$(SOURCES)
	@echo "Lint OK"

# ── Testing ──────────────────────────────────────────────

test:
	@echo "No tests yet. Add unit tests under <product>/test/ using Unity framework."

# ── Cleaning ─────────────────────────────────────────────

clean:
	rm -rf matcha-bowl/build
	rm -rf liquid-machine/build

# ── Help ─────────────────────────────────────────────────

help:
	@echo "Targets:"
	@echo "  build          Build both products"
	@echo "  build-matcha   Build matcha-bowl (ESP32)"
	@echo "  build-liquid   Build liquid-machine (ESP32-P4)"
	@echo "  setup-matcha   First-time: set ESP32 target"
	@echo "  setup-liquid   First-time: set ESP32-P4 target"
	@echo "  format         Auto-format all source files"
	@echo "  format-check   Check formatting (CI)"
	@echo "  lint           Run cppcheck static analysis"
	@echo "  test           Run tests (placeholder)"
	@echo "  clean          Remove build directories"
	@echo ""
	@echo "Prerequisites:"
	@echo "  export IDF_PATH=~/Work/esp-idf && source \$$IDF_PATH/export.sh"
