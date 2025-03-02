BUILD:=build
SRC:=src
C_FLAGS:=-fPIC -ggdb -I $(SRC) -O3

$(BUILD)/async.o: $(SRC)/async.c
	mkdir -p $(BUILD)
	gcc -c $(C_FLAGS) -DDEBUG -o $@ $(SRC)/async.c 

$(BUILD)/libasync.a: $(BUILD)/async.o
	mkdir -p $(BUILD)
	ar r $@ $^

$(BUILD)/test: $(SRC)/test.c $(BUILD)/libasync.a
	mkdir -p $(BUILD)
	gcc $(C_FLAGS) $(SRC)/test.c -o $@ -L $(BUILD) -lasync 

build: $(BUILD)/test $(BUILD)/libasync.a

.PHONY: clean
clean:
	rm -rf $(BUILD)

