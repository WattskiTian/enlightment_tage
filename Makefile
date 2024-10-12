CC=g++
TARGET_TAGE=demo_tage
TARGET_LTAGE=demo_ltage
TARGET_TAGE_IO=tage_IO
GDB_TARGET_TAGE=demo_tage.gdb
GDB_TARGET_LTAGE=demo_ltage.gdb
GDB_TARGET_TAGE_IO=tage_IO.gdb

SRC_LTAGE = demo_loop.cpp utils.cpp demo_ltage.cpp

.PHONY: all tage tage_gdb clean build ltage ltage_gdb tageIO tageIO_gdb

build: $(TARGET_TAGE) $(TARGET_LTAGE) $(TARGET_TAGE_IO)

all: $(TARGET_TAGE) $(GDB_TARGET_TAGE) $(TARGET_LTAGE) $(GDB_TARGET_LTAGE) $(TARGET_TAGE_IO) $(GDB_TARGET_TAGE_IO)

$(TARGET_TAGE): demo_tage.cpp
	$(CC) -O3 -w -o $@ $<

$(GDB_TARGET_TAGE): demo_tage.cpp
	$(CC) -g -w -o $@ $<

$(TARGET_LTAGE): $(SRC_LTAGE)
	$(CC) -O3 -w -o $@ $(SRC_LTAGE)

$(GDB_TARGET_LTAGE): $(SRC_LTAGE)
	$(CC) -g -w -o $@ $(SRC_LTAGE)

$(TARGET_TAGE_IO): tage_IO.cpp
	$(CC) -O3 -w -o $@ $<

$(GDB_TARGET_TAGE_IO): tage_IO.cpp
	$(CC) -g -w -o $@ $<


tage: $(TARGET_TAGE)
	./$(TARGET_TAGE) > tage_log

tage_gdb: $(GDB_TARGET_TAGE)
	gdb --args ./$(GDB_TARGET_TAGE)
	
ltage: $(TARGET_LTAGE)
	./$(TARGET_LTAGE) > ltage_log

ltage_gdb: $(GDB_TARGET_LTAGE)
	gdb --args ./$(GDB_TARGET_LTAGE)

tageIO: $(TARGET_TAGE_IO)
	./$(TARGET_TAGE_IO) > tage_IO_log
	./$(TARGET_TAGE) >> tage_IO_log

tageIO_gdb: $(GDB_TARGET_TAGE_IO)
	gdb --args ./$(GDB_TARGET_TAGE_IO)


clean:
	rm -f $(TARGET_TAGE) $(GDB_TARGET_TAGE) $(TARGET_LTAGE) $(GDB_TARGET_LTAGE) $(TARGET_TAGE_IO) $(GDB_TARGET_TAGE_IO)
	rm -f ltage_log tage_log loop_log tage_IO_log

