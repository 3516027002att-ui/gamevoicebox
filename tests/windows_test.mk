# Included by root Makefile. Shared adapters below are actual production code.
WINDOWS_CONTRACT_FLAGS ?= -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g
.PHONY: test-wasapi-contract test-ui-contract
test-wasapi-contract: build/tests/test_wasapi_contract
	./build/tests/test_wasapi_contract
test-ui-contract: build/tests/test_ui_contract
	./build/tests/test_ui_contract
build/tests/test_wasapi_contract: tests/test_wasapi_contract.c src/windows/audio_contract.c src/windows/audio_contract.h src/windows/device_contract.c src/windows/device_contract.h src/windows/status_contract.c src/windows/status_contract.h
	@mkdir -p build/tests
	$(CC) $(WINDOWS_CONTRACT_FLAGS) tests/test_wasapi_contract.c src/windows/audio_contract.c src/windows/device_contract.c src/windows/status_contract.c -o $@
build/tests/test_ui_contract: tests/test_ui_contract.c src/windows/ui_contract.c src/windows/ui_contract.h src/windows/device_contract.h
	@mkdir -p build/tests
	$(CC) $(WINDOWS_CONTRACT_FLAGS) tests/test_ui_contract.c src/windows/ui_contract.c -o $@
.PHONY: test-command-queue
test-wasapi-contract: test-command-queue
test-command-queue: build/tests/test_command_queue
	./build/tests/test_command_queue
build/tests/test_command_queue: tests/test_command_queue.c src/windows/command_queue.c src/windows/command_queue.h
	@mkdir -p build/tests
	$(CC) $(WINDOWS_CONTRACT_FLAGS) tests/test_command_queue.c src/windows/command_queue.c -o $@
.PHONY: test-windows-source
test-ui-contract: test-windows-source
test-windows-source:
	python3 -m unittest discover -s tests -p test_windows_source.py
