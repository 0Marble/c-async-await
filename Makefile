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

$(BUILD)/str.o: $(EXMPL)/str.c
	mkdir -p $(BUILD)
	gcc -c $(C_FLAGS) -I$(SRC) -o $@ $<

$(BUILD)/libstr.a: $(BUILD)/str.o
	mkdir -p $(BUILD)
	ar r $@ $^

$(BUILD)/test: $(SRC)/test.c $(BUILD)/libasync.a
	mkdir -p $(BUILD)
	gcc $(C_FLAGS) -I$(SRC) $(SRC)/test.c -o $@ -L $(BUILD) -lasync 

$(BUILD)/runner: $(TESTS)/runner.c 
	mkdir -p $(BUILD)
	gcc $(C_FLAGS) $< -o $@ 

test: $(BUILD)/runner
	mkdir -p $(BUILD)/tests
	ls $(TESTS) | grep -v runner.c | $(BUILD)/runner -

$(BUILD)/$(EXMPL)/echo_fork: $(EXMPL)/echo_fork.c $(BUILD)/libstr.a
	mkdir -p $(BUILD)/$(EXMPL)
	gcc $(C_FLAGS) -I$(SRC) -I$(EXMPL) $< -o $@ -L $(BUILD) -lstr

$(BUILD)/$(EXMPL)/echo_async: $(EXMPL)/echo_async.c $(BUILD)/libstr.a $(BUILD)/libasync.a
	mkdir -p $(BUILD)/$(EXMPL)
	gcc $(C_FLAGS) -I$(SRC) -I$(EXMPL) $< -o $@ -L $(BUILD) -lstr -lasync

build: $(BUILD)/test $(BUILD)/libasync.a $(BUILD)/runner $(BUILD)/str.a $(BUILD)/$(EXMPL)/echo_fork $(BUILD)/$(EXMPL)/echo_async

.PHONY: clean
clean:
	rm -rf $(BUILD)

