CC      = gcc
CFLAGS  = -std=gnu11 -O2 -Wall -Wextra -pthread -I../include -D_GNU_SOURCE
LDFLAGS = -lrt -lpthread -lm
TARGET  = pipeline
SRCS    = main.c producer.c consumer.c display.c shm_setup.c baseline.c

.PHONY: all clean run dirs reset-shm

all: dirs $(TARGET)

dirs:
	@mkdir -p ../results

$(TARGET): $(SRCS) ../include/pipeline.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)
	@echo "[OK] ./$(TARGET)"

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

reset-shm:
	@rm -f /dev/shm/surgical_pipeline_shm 2>/dev/null || true
	@echo "[RESET] shm cleared"