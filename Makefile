APP_NAME=lora-linux
SRC_FILES := $(shell find . -name '*.c')
OBJ_FILES = $(SRC_FILES:.c=.o)
HEADER_FILES := $(shell find . -name '*.h')
SPI_DEV ?= /dev/spidev1.0

CFLAGS += -Wall -D__DEBUG=0 -DSPI_DEV=\"$(SPI_DEV)\"
LDFLAGS += -lpthread -lrt

all: $(APP_NAME)

$(APP_NAME): $(OBJ_FILES)
	@echo "LD $(APP_NAME)"
	@$(CC) -o $(APP_NAME) $^ $(LDFLAGS)
	@echo "SPI_DEV: $(SPI_DEV)"

%.o: %.c $(HEADER_FILES)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@echo "Delete binary files"
	@rm -f $(APP_NAME) *.o
