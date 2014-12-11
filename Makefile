
BIN ?= video-transcode
SRC = video-transcode.c
TEST_VIDEO ?= test-video.mov
TEST_VIDEOTMP ?= tmp-$(TEST_VIDEO)

.PHONY: $(BIN)
$(BIN):
	$(CC) -lavcodec -lavutil $(SRC) -o $(@)

test: $(BIN)
	cp -f $(TEST_VIDEO) $(TEST_VIDEOTMP)
	$(BIN) $(TEST_VIDEOTMP)

clean:
	rm -f $(TEST_VIDEOTMP)
	rm -f $(BIN)
