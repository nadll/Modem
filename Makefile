INCLUDE_DIR=-Iinclude
EXEC=main
EXTRA_CFLAGS=-Wall -g $(INCLUDE_DIR)
EXTRA_LDFLAGS=
OBJECTS=	main.o

.PHONY: all
all: $(EXEC)

$(EXEC): $(EXEC).o $(OBJECTS)
	@ echo "[LD] $@"
	@ $(CC) -o $@ $^ $(EXTRA_CFLAGS) $(EXTRA_LDFLAGS)

%.o: %.c
	@ echo "[CC] $@"
	@ $(CC) -o $@ -c $< $(EXTRA_CFLAGS)

.PHONY: clean
clean:
	@rm -f *.o $(EXEC)
	@rm -f */*.o

.PHONY: help
help:
	@echo ''
	@echo 'Makefile targets:'
	@echo ''
	@echo '  make       - generated binary'
	@echo '  make clean - clean '
	@echo '  make help  - Show help'
	@echo ''