APP_CC ?= gcc
APP_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined
APP_LDFLAGS ?= -fsanitize=address,undefined
APP_TEST_DIR ?= build/tests
APP_TEST_ENV ?= ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1
APP_SOURCES = src/app/settings.c src/app/hotkeys.c src/app/controller.c
.PHONY: test-app test-settings test-hotkeys test-controller
$(APP_TEST_DIR)/test_settings: tests/test_settings.c $(APP_SOURCES) src/app/settings.h src/app/hotkeys.h src/app/controller.h
	mkdir -p $(APP_TEST_DIR)
	$(APP_CC) $(APP_CFLAGS) -Isrc/app tests/test_settings.c $(APP_SOURCES) $(APP_LDFLAGS) -o $@
$(APP_TEST_DIR)/test_hotkeys: tests/test_hotkeys.c $(APP_SOURCES) src/app/settings.h src/app/hotkeys.h src/app/controller.h
	mkdir -p $(APP_TEST_DIR)
	$(APP_CC) $(APP_CFLAGS) -Isrc/app tests/test_hotkeys.c $(APP_SOURCES) $(APP_LDFLAGS) -o $@
$(APP_TEST_DIR)/test_controller: tests/test_controller.c $(APP_SOURCES) src/app/settings.h src/app/hotkeys.h src/app/controller.h
	mkdir -p $(APP_TEST_DIR)
	$(APP_CC) $(APP_CFLAGS) -Isrc/app tests/test_controller.c $(APP_SOURCES) $(APP_LDFLAGS) -o $@
test-settings: $(APP_TEST_DIR)/test_settings
	$(APP_TEST_ENV) $(APP_TEST_DIR)/test_settings
test-hotkeys: $(APP_TEST_DIR)/test_hotkeys
	$(APP_TEST_ENV) $(APP_TEST_DIR)/test_hotkeys
test-controller: $(APP_TEST_DIR)/test_controller
	$(APP_TEST_ENV) $(APP_TEST_DIR)/test_controller
test-app: test-settings test-hotkeys test-controller
