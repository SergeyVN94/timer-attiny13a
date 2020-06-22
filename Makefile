MCU = attiny13a
MCU_CORE = t13
DEVICE_PATH = /dev/ttyUSB0

COMPILER = avr-g++
COMPILER_FLAGS = -mmcu=$(MCU) -Os -Wall
COMPILER_OUT_FILE = main.elf
COMPILER_OBJ_FILE = main.o
HEX_FILE = main.hex

all: to_hex check_size clean

# главный файл
main: main.cpp
	$(COMPILER) $(COMPILER_FLAGS) -c main.cpp -o $(COMPILER_OBJ_FILE)
	$(COMPILER) $(COMPILER_FLAGS) $(COMPILER_OBJ_FILE) -o $(COMPILER_OUT_FILE)

# преобразование в формат для записи в мк
to_hex: main
	avr-objcopy -O ihex $(COMPILER_OUT_FILE) $(HEX_FILE)

# вывод размера программы
check_size: main
	avr-size --mcu=$(MCU) --format=avr $(COMPILER_OUT_FILE)

# удаление ненужных файлов
clean: check_size
	rm $(COMPILER_OUT_FILE) $(COMPILER_OBJ_FILE)

firmware: $(HEX_FILE)
	avrdude -c arduino -p $(MCU_CORE) -P $(DEVICE_PATH) -b 19200 -U flash:w:$(HEX_FILE)
