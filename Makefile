BUILD:=build
SRC:=src
EXMPL:=examples
C_FLAGS:=-fPIC -ggdb -I $(SRC) 

$(BUILD)/async.o: $(SRC)/async.c
	mkdir -p $(BUILD)
	gcc -c $(C_FLAGS) -o $@ $(SRC)/async.c 

$(BUILD)/libasync.a: $(BUILD)/async.o
	mkdir -p $(BUILD)
	ar r $@ $^

$(BUILD)/test: $(SRC)/test.c $(BUILD)/libasync.a
	mkdir -p $(BUILD)
	gcc $(C_FLAGS) $(SRC)/test.c -o $@ -L $(BUILD) -lasync 

build: $(BUILD)/test $(BUILD)/libasync.a

$(BUILD)/$(EXMPL)/sync_http: $(EXMPL)/sync_http.c $(BUILD)/libasync.a
	mkdir -p $(BUILD)/$(EXMPL)
	gcc $(C_FLAGS) $^ -o $@ -L $(BUILD) -lasync

examples: $(BUILD)/$(EXMPL)/sync_http

.PHONY: clean
clean:
	rm -rf $(BUILD)

