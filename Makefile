SHELL := /bin/bash
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
LDLIBS := -lm
TOOLS_ROOT ?= /usr
XCC ?= $(TOOLS_ROOT)/bin/x86_64-w64-mingw32-gcc-win32
XWINDRES ?= $(TOOLS_ROOT)/bin/x86_64-w64-mingw32-windres
XOBJDUMP ?= $(TOOLS_ROOT)/bin/x86_64-w64-mingw32-objdump
XSTRIP ?= $(TOOLS_ROOT)/bin/x86_64-w64-mingw32-strip
MAKENSIS ?= $(TOOLS_ROOT)/bin/makensis
SEVENZIP ?= $(TOOLS_ROOT)/lib/7zip/7z
export PATH := $(TOOLS_ROOT)/bin:$(PATH)
export NSISDIR := $(TOOLS_ROOT)/share/nsis
export ASAN_OPTIONS := detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS := halt_on_error=1
XCFLAGS := -std=c11 -Wall -Wextra -Werror -Os -ffunction-sections -fdata-sections -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -DCOBJMACROS
XLDFLAGS := -municode -mwindows -static-libgcc -Wl,--gc-sections -Wl,--nxcompat -Wl,--dynamicbase
XLDLIBS := -lole32 -luuid -lksguid -lcomctl32 -lshell32 -lavrt -lpropsys -ladvapi32 -lgdi32 -luser32 -lm
.PHONY: test test-build-contract fetch-vendor test-assets assets windows windows-audio package verify-package test-package clean source
-include tests/audio_test.mk
-include tests/app_test.mk
-include tests/windows_test.mk
assets:
	python3 scripts/build_assets.py
test-assets:
	python3 -m unittest discover -s tests -p test_assets.py
test: test-assets test-build-contract test-delivery-report test-format test-clip test-mixer test-app test-wasapi-contract test-ui-contract
windows-audio:
	mkdir -p build/objects
	$(XCC) $(XCFLAGS) -c src/windows/devices.c -o build/objects/devices.o
	$(XCC) $(XCFLAGS) -c src/windows/wasapi.c -o build/objects/wasapi.o
windows:
	mkdir -p dist build/objects
	$(XWINDRES) --preprocessor=$(XCC) --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I src/windows src/windows/app.rc build/objects/app.o
	$(XCC) $(XCFLAGS) src/audio/*.c src/app/*.c src/windows/*.c build/objects/app.o $(XLDFLAGS) $(XLDLIBS) -o dist/GameVoiceBox.exe
	$(XSTRIP) --strip-all dist/GameVoiceBox.exe
	$(XOBJDUMP) -p dist/GameVoiceBox.exe > dist/pe-imports.txt
package: windows
	python3 scripts/generate_uninstall.py
	$(MAKENSIS) -DPROJECT_DIR=$(CURDIR) installer/gamevoicebox.nsi > build/nsis.log
	tail -20 build/nsis.log
verify-package:
	python3 scripts/verify_package.py --sevenzip $(SEVENZIP) --objdump $(XOBJDUMP)
test-package:
	python3 -m unittest discover -s tests -p test_package.py
source:
	python3 scripts/package_source.py
clean:
	rm -rf build/objects build/tests build/package-extract dist/GameVoiceBox.exe dist/GameVoiceBox-Setup-1.2.1-x64.exe

fetch-vendor:
	python3 scripts/fetch_vendor.py
test-build-contract:
	python3 -m unittest discover -s tests -p test_gamevoicebox_packaging.py

test-delivery-report:
	python3 -m unittest discover -s tests -p test_delivery_report.py

.PHONY: test-tray
test: test-tray
test-tray:
	python3 -m unittest discover -s tests -p test_tray.py

.PHONY: test-guide-update
test: test-guide-update
test-guide-update:
	python3 -m unittest discover -s tests -p test_guide_update.py

.PHONY: test-installer-upgrade
test: test-installer-upgrade
test-installer-upgrade:
	python3 -m unittest discover -s tests -p test_installer_upgrade.py

.PHONY: test-hotkey-diagnostics
test: test-hotkey-diagnostics
test-hotkey-diagnostics:
	python3 -m unittest discover -s tests -p test_hotkey_diagnostics.py
