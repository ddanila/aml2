KVIKDOS_DIR = ../../vendor/kvikdos
CC = cc
CFLAGS = -O2 -Wall -Wextra -I$(KVIKDOS_DIR)

all: measure_key_lag

$(KVIKDOS_DIR)/test_harness.o: $(KVIKDOS_DIR)/test_harness.c $(KVIKDOS_DIR)/test_harness.h $(KVIKDOS_DIR)/kvikdos.c $(KVIKDOS_DIR)/cpu8086.c
	$(MAKE) -C $(KVIKDOS_DIR) test_harness.o

measure_key_lag: measure_key_lag.c $(KVIKDOS_DIR)/test_harness.o $(KVIKDOS_DIR)/cpu8086.o
	$(CC) $(CFLAGS) -o $@ $< $(KVIKDOS_DIR)/test_harness.o $(KVIKDOS_DIR)/cpu8086.o -lpthread

clean:
	rm -f measure_key_lag

.PHONY: all clean
