
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET = lab8

.PHONY: all run generate experiment clean

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

generate:
	python3 generate_addresses.py

experiment: $(TARGET)
	@for size in 4 8 16 32; do echo "=== TLB $$size ==="; ./$(TARGET) --tlb-size $$size --quiet; done

clean:
	rm -f $(TARGET)
