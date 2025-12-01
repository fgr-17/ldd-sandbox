SUBDIRS :=  01-hello-world \
			02-char-device-driver \
			03-char-dev-multibuf \
			04-char-dev-ioctl \
			05-char-dev-auto-node \
			06-char-dev-blocking

all:
	@for dir in $(SUBDIRS); do \
		echo "Building $$dir..."; \
		$(MAKE) -C $$dir || exit 1; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		echo "Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done

example:
	@if [ -z "$(ex)" ]; then \
		echo "Usage: make example ex=01-hello-world"; \
		exit 1; \
	fi
	$(MAKE) -C $(ex)

clean-example:
	@if [ -z "$(ex)" ]; then \
		echo "Usage: make clean-example ex=01-hello-world"; \
		exit 1; \
	fi
	$(MAKE) -C $(ex) clean

help:
	@echo "Available targets:"
	@echo "  make all              - Build all examples"
	@echo "  make clean            - Clean all examples"
	@echo "  make example ex=DIR   - Build specific example"
	@echo "  make clean-example ex=DIR - Clean specific example"
	@echo ""
	@echo "Available examples:"
	@for dir in $(SUBDIRS); do \
		echo "  - $$dir"; \
	done

.PHONY: all clean example clean-example help
