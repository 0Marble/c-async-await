BUILD:=build
SRC:=src
TESTS:=tests
EXMPL:=examples
C_FLAGS:=-fPIC -ggdb 

$(BUILD)/async.o: $(SRC)/async.c
	mkdir -p $(BUILD)
	gcc -c $(C_FLAGS) -I$(SRC) -o $@ $(SRC)/async.c 

$(BUILD)/libasync.a: $(BUILD)/async.o
	mkdir -p $(BUILD)
	ar r $@ $^

$(BUILD)/test: $(SRC)/test.c $(BUILD)/libasync.a
	mkdir -p $(BUILD)
	gcc $(C_FLAGS) -I$(SRC) $(SRC)/test.c -o $@ -L $(BUILD) -lasync 


$(BUILD)/runner: $(TESTS)/runner.c 
	mkdir -p $(BUILD)
	gcc $(C_FLAGS) $< -o $@ 

build: $(BUILD)/test $(BUILD)/libasync.a $(BUILD)/runner

.PHONY: clean
clean:
	rm -rf $(BUILD)

