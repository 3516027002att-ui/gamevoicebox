AUDIO_TEST_CC ?= gcc
AUDIO_TEST_FLAGS ?= -std=c11 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer -fsanitize=address,undefined
AUDIO_TEST_DIR ?= build/tests
AUDIO_TEST_RUN ?= ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1

.PHONY: test-format test-clip test-mixer test-audio

test-audio: test-format test-clip test-mixer

test-format: $(AUDIO_TEST_DIR)/test_format
	$(AUDIO_TEST_RUN) $<

test-clip: $(AUDIO_TEST_DIR)/test_clip
	$(AUDIO_TEST_RUN) $<

test-mixer: $(AUDIO_TEST_DIR)/test_mixer
	$(AUDIO_TEST_RUN) $<

$(AUDIO_TEST_DIR)/test_format: tests/test_format.c src/audio/format.c src/audio/format.h
	mkdir -p $(AUDIO_TEST_DIR)
	$(AUDIO_TEST_CC) $(AUDIO_TEST_FLAGS) -Isrc/audio tests/test_format.c src/audio/format.c -lm -o $@

$(AUDIO_TEST_DIR)/test_clip: tests/test_clip.c src/audio/clip.c src/audio/clip.h
	mkdir -p $(AUDIO_TEST_DIR)
	$(AUDIO_TEST_CC) $(AUDIO_TEST_FLAGS) -Isrc/audio tests/test_clip.c src/audio/clip.c -lm -o $@

$(AUDIO_TEST_DIR)/test_mixer: tests/test_mixer.c src/audio/mixer.c src/audio/format.c src/audio/clip.c src/audio/mixer.h src/audio/clip.h src/audio/format.h
	mkdir -p $(AUDIO_TEST_DIR)
	$(AUDIO_TEST_CC) $(AUDIO_TEST_FLAGS) -Isrc/audio tests/test_mixer.c src/audio/mixer.c src/audio/format.c src/audio/clip.c -lm -o $@
