APP_NAME=lora-linux
SRC_FILES := $(shell find . -name '*.c')
OBJ_FILES = $(SRC_FILES:.c=.o)
HEADER_FILES := $(shell find . -name '*.h')

CFLAGS += -Wall -D__DEBUG=1
LDFLAGS += -lpthread

all: $(APP_NAME)

$(APP_NAME): $(OBJ_FILES)
	@echo "LD $(APP_NAME)"
	@$(CC) -o $(APP_NAME) $^ $(LDFLAGS)

%.o: %.c $(HEADER_FILES)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@echo "Delete binary files"
	@rm -f $(APP_NAME) *.o
