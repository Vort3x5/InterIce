TARGET = koprocesor
HW_SRC = hw/top.sv hw/spi_slave.sv hw/quadra.sv hw/square.sv hw/lut.sv
PCF = hw/pins.pcf

all: prog

$(TARGET).json: $(HW_SRC) hw/quadra.svh
	yosys -p "read_verilog -sv $(HW_SRC); synth_ice40 -top top -json $(TARGET).json"

$(TARGET).asc: $(TARGET).json $(PCF)
	nextpnr-ice40 --hx8k --package ct256 --pcf $(PCF) --json $(TARGET).json --asc $(TARGET).asc

$(TARGET).bin: $(TARGET).asc
	icepack $(TARGET).asc $(TARGET).bin

prog: $(TARGET).bin
	iceprog $(TARGET).bin

clean:
	rm -f *.json *.asc *.bin
