# Compilador
CXX := g++
# Flags de compilação
CXXFLAGS := -g -std=c++17 -Ilib
# Flags de linkedição
LDFLAGS := -Llib -lglfw3dll -lgdi32 -lopengl32
# Nome do executável
TARGET := programa.exe
# Arquivos fonte
SOURCES := src/main.cpp src/VTKLoader.cpp src/TreeRenderer.cpp lib/glad/glad.c
# DLL necessária
DLL := lib/GLFW/glfw3.dll

# Regra padrão
all: $(TARGET)

# Regra para compilar o programa
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)
	@echo "=== TP1 Compilado com Sucesso ==="

# Regra para executar
run: $(TARGET)
	@echo "=== Executando Visualizador de Arvores Arteriais ==="
	@.\$(TARGET)

# Regra para limpar
clean:
	@if exist "$(TARGET)" del "$(TARGET)"
	@if exist "glfw3.dll" del "glfw3.dll"
	@echo "=== Arquivos limpos ==="

# Ajuda
help:
	@echo "Comandos disponiveis:"
	@echo "  make      - Compila o programa"
	@echo "  make run  - Executa o programa"
	@echo "  make clean - Limpa arquivos gerados"

.PHONY: all run clean help