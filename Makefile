BUILD:=build
SRC:=src
C_FLAGS:=-fPIC -ggdb -I $(SRC)

$(BUILD)/async.o: $(SRC)/async.c
	mkdir -p $(BUILD)
	gcc -c $(C_FLAGS) -o $@ $(SRC)/async.c 

$(BUILD)/libasync.a: $(BUILD)/async.o
	mkdir -p $(BUILD)
	ar r $@ $^

$(BUILD)/main: $(SRC)/main.c $(BUILD)/libasync.a
	mkdir -p $(BUILD)
	gcc $(C_FLAGS) $(SRC)/main.c -o $@ -L $(BUILD) -lasync 

build: $(BUILD)/main $(BUILD)/libasync.a

.PHONY: clean
clean:
	rm -rf $(BUILD)

