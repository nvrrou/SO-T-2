CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread
TARGET = grid_tarea_2_cristobal_maria
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "Compilando $(SRC)..."
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)
	@echo "Compilación completa: ./$(TARGET)"

run: $(TARGET)
	@echo "Ejecutando programa..."
	./$(TARGET)

clean:
	@echo "Limpiando archivos..."
	rm -f $(TARGET)
	@echo "Limpieza completa."

.PHONY: all run clean
