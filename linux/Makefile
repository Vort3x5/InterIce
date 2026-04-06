obj-m += spi.o

all:
	@if [ -z "$$KDIR" ]; then echo "BLAD: Wywolaj nix-shell."; exit 1; fi
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@if [ -z "$$KDIR" ]; then echo "BLAD: Wywolaj nix-shell."; exit 1; fi
	$(MAKE) -C $(KDIR) M=$(PWD) clean
