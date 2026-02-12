#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <filesystem>  
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "VTKLoader.h"
#include "TreeRenderer.h"

using namespace std;
namespace fs = std::filesystem;  

// =============================================
// Estruturas
// =============================================

struct CameraState {
    float translation[2] = {0.0f, 0.0f};
    float scale = 1.0f;
    float rotation = 0.0f;
    
    float targetTranslation[2] = {0.0f, 0.0f};
    float targetScale = 1.0f;
    float targetRotation = 0.0f;
};

struct AppConfig {
    float backgroundColor[3] = {0.05f, 0.05f, 0.08f};
    float moveSpeed = 0.0005f;
    float rotationSpeed = 0.0005f;
    float zoomSpeed = 0.4f;
    float smoothFactor = 3.0f;
    float dragSensitivity = 0.0013f;
    float minScale = 0.1f;
    float maxScale = 5.0f;
    float translationLimit = 2.0f;
};

struct MouseState {
    bool isDragging = false;
    double lastX = 0.0;
    double lastY = 0.0;
};

// =============================================
// Variáveis Globais
// =============================================

TreeRenderer treeRenderer;
VTKLoader vtkLoader;
vector<string> treeFiles;
vector<string> treeFileNames;
size_t currentTreeIndex = 0;

CameraState camera;
AppConfig config;
MouseState mouse;

bool showWireframe = false;
bool monochromeMode = false;
bool gradientMode = false;
bool thicknessMode = false;
bool descendantsColorMode = false;

float transformMatrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f, 
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

chrono::steady_clock::time_point lastTime;

// =============================================
// Declarações
// =============================================

void loadTreeFiles();
void updateTransformMatrix();
void updateSmoothTransform(float deltaTime);
void resetCamera();
void printControls();
void printCurrentTreeInfo();

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

void processInput(GLFWwindow* window);
void handleKeyPress(int key);
void handleTreeNavigation(int direction);

// =============================================
// Implementação
// =============================================

void loadTreeFiles() {
    treeFiles.clear();
    treeFileNames.clear();
    
    cout << "Procurando arquivos VTK..." << endl;
    
    vector<string> folders = {"data/Nterm_064", "data/Nterm_128", "data/Nterm_256"};
    int totalFiles = 0;
    
    for (const auto& folder : folders) {
        if (!fs::exists(folder) || !fs::is_directory(folder)) {
            cout << "  [!] Pasta não encontrada: " << folder << endl;
            continue;
        }
        
        cout << "Lendo pasta: " << folder << endl;
        
        for (const auto& entry : fs::directory_iterator(folder)) {
            if (!entry.is_regular_file()) continue;
            
            string path = entry.path().string();
            if (entry.path().extension() != ".vtk") continue;
            
            treeFiles.push_back(path);
            
            // Cria nome amigável
            string filename = entry.path().filename().string();
            string friendlyName = folder + "/" + filename;
            
            // Remove prefixos para nome mais legível
            size_t pos = friendlyName.find("tree2D_");
            if (pos != string::npos) {
                friendlyName = friendlyName.substr(pos + 7);
            }
            
            pos = friendlyName.find("_step");
            if (pos != string::npos) {
                friendlyName.insert(pos, " ");
            }
            
            treeFileNames.push_back(friendlyName);
            totalFiles++;
            
            cout << "  [+] " << filename << endl;
        }
    }
    
    if (treeFiles.empty()) {
        cout << "Nenhum arquivo VTK encontrado!" << endl;
        cout << "Criando lista de arquivos padrão..." << endl;
        
        treeFiles = {
            "data/Nterm_064/tree2D_Nterm0064_step0064.vtk",
            "data/Nterm_064/tree2D_Nterm0064_step0008.vtk", 
            "data/Nterm_128/tree2D_Nterm0128_step0128.vtk",
            "data/Nterm_256/tree2D_Nterm0256_step0256.vtk"
        };
        
        for (const auto& file : treeFiles) {
            treeFileNames.push_back(file);
        }
    }
    
    cout << "Total de arquivos VTK carregados: " << treeFiles.size() << endl;
}

void printCurrentTreeInfo() {
    if (currentTreeIndex >= treeFileNames.size()) return;
    
    cout << "\n=== Árvore Atual ===" << endl;
    cout << "Arquivo: " << treeFileNames[currentTreeIndex] << endl;
    cout << "Índice: " << (currentTreeIndex + 1) << " de " << treeFiles.size() << endl;
}

void updateTransformMatrix() {
    // Matriz identidade
    fill_n(transformMatrix, 16, 0.0f);
    transformMatrix[0] = transformMatrix[5] = transformMatrix[10] = transformMatrix[15] = 1.0f;
    
    // Aplica rotação e escala
    float cosR = cos(camera.rotation);
    float sinR = sin(camera.rotation);
    transformMatrix[0] = cosR * camera.scale;
    transformMatrix[1] = -sinR * camera.scale;
    transformMatrix[4] = sinR * camera.scale;
    transformMatrix[5] = cosR * camera.scale;
    
    // Aplica translação
    transformMatrix[12] = camera.translation[0];
    transformMatrix[13] = camera.translation[1];
}

void updateSmoothTransform(float deltaTime) {
    // Interpolação suave usando LERP
    auto lerp = [](float current, float target, float factor, float deltaTime) {
        return current + (target - current) * factor * deltaTime;
    };
    
    camera.translation[0] = lerp(camera.translation[0], camera.targetTranslation[0], 
                                config.smoothFactor, deltaTime);
    camera.translation[1] = lerp(camera.translation[1], camera.targetTranslation[1], 
                                config.smoothFactor, deltaTime);
    camera.rotation = lerp(camera.rotation, camera.targetRotation, 
                          config.smoothFactor, deltaTime);
    camera.scale = lerp(camera.scale, camera.targetScale, 
                       config.smoothFactor, deltaTime);
    
    updateTransformMatrix();
}

void resetCamera() {
    camera.targetTranslation[0] = camera.targetTranslation[1] = 0.0f;
    camera.targetScale = 1.0f;
    camera.targetRotation = 0.0f;
    cout << "Transformações resetadas" << endl;
}

inline void limitCameraValues() {
    camera.targetTranslation[0] = clamp(camera.targetTranslation[0], 
                                       -config.translationLimit, config.translationLimit);
    camera.targetTranslation[1] = clamp(camera.targetTranslation[1], 
                                       -config.translationLimit, config.translationLimit);
    camera.targetScale = clamp(camera.targetScale, config.minScale, config.maxScale);
}

// =============================================
// Callbacks GLFW
// =============================================

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    
    mouse.isDragging = (action == GLFW_PRESS);
    
    if (mouse.isDragging) {
        glfwGetCursorPos(window, &mouse.lastX, &mouse.lastY);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!mouse.isDragging) return;
    
    float deltaX = static_cast<float>(xpos - mouse.lastX) * config.dragSensitivity;
    float deltaY = static_cast<float>(mouse.lastY - ypos) * config.dragSensitivity;
    
    // Compensa o zoom para movimento consistente
    float zoomCompensation = 1.0f / camera.scale;
    camera.targetTranslation[0] += deltaX * zoomCompensation;
    camera.targetTranslation[1] += deltaY * zoomCompensation;
    
    mouse.lastX = xpos;
    mouse.lastY = ypos;
    limitCameraValues();
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    float zoomAmount = static_cast<float>(yoffset) * 0.1f * config.zoomSpeed;
    camera.targetScale += zoomAmount;
    limitCameraValues();
    cout << "Zoom: " << camera.targetScale << endl;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        handleKeyPress(key);
    }
}

// =============================================
// Processamento de Entrada
// =============================================

void handleKeyPress(int key) {
    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
            break;
        case GLFW_KEY_R:
            resetCamera();
            break;
        // case GLFW_KEY_T:
        //     showWireframe = !showWireframe;
        //     glPolygonMode(GL_FRONT_AND_BACK, showWireframe ? GL_LINE : GL_FILL);
        //     cout << "Wireframe: " << (showWireframe ? "ON" : "OFF") << endl;
        //     break;
        case GLFW_KEY_L:
            thicknessMode = !thicknessMode;
            treeRenderer.setThicknessMode(thicknessMode);
            cout << "Espessura adaptativa: " << (thicknessMode ? "ON" : "OFF") << endl;
            break;
        case GLFW_KEY_RIGHT:
            handleTreeNavigation(1);
            break;
        case GLFW_KEY_LEFT:
            handleTreeNavigation(-1);
            break;
        case GLFW_KEY_C:
            // Ciclo: Branco -> Monocromático Verde -> Gradiente Violeta-Vermelho -> Gradiente Azul-Vermelho (descendentes)
            if (!monochromeMode && !gradientMode && !descendantsColorMode) {
                // Primeiro: Modo monocromático verde
                monochromeMode = true;
                gradientMode = false;
                descendantsColorMode = false;
                treeRenderer.setColorMode(true);
                treeRenderer.setGradientMode(false);
                treeRenderer.setDescendantsColorMode(false);
                cout << "Modo monocromático: ON (Verde)" << endl;
            } else if (monochromeMode && !gradientMode && !descendantsColorMode) {
                // Segundo: Gradiente violeta-vermelho (profundidade)
                monochromeMode = false;
                gradientMode = true;
                descendantsColorMode = false;
                treeRenderer.setColorMode(false);
                treeRenderer.setGradientMode(true);
                treeRenderer.setDescendantsColorMode(false);
                cout << "Modo gradiente por profundidade: ON (Violeta->Vermelho)" << endl;
            } else if (!monochromeMode && gradientMode && !descendantsColorMode) {
                // Terceiro: Gradiente azul-vermelho (descendentes)
                monochromeMode = false;
                gradientMode = false;
                descendantsColorMode = true;
                treeRenderer.setColorMode(false);
                treeRenderer.setGradientMode(false);
                treeRenderer.setDescendantsColorMode(true);
                cout << "Modo gradiente por descendentes: ON (Azul->Vermelho)" << endl;
            } else {
                // Quarto: Volta para branco
                monochromeMode = false;
                gradientMode = false;
                descendantsColorMode = false;
                treeRenderer.setColorMode(false);
                treeRenderer.setGradientMode(false);
                treeRenderer.setDescendantsColorMode(false);
                cout << "Modo de cor: OFF (Branco)" << endl;
            }
            break;
        case GLFW_KEY_I:
            printCurrentTreeInfo();
            break;
    }
}

void handleTreeNavigation(int direction) {
    if (treeFiles.empty()) return;
    
    if (direction > 0) {
        currentTreeIndex = (currentTreeIndex + 1) % treeFiles.size();
    } else {
        currentTreeIndex = (currentTreeIndex == 0) ? treeFiles.size() - 1 : currentTreeIndex - 1;
    }
    
    if (vtkLoader.loadFile(treeFiles[currentTreeIndex])) {
        cout << "\n--- Nova Árvore Carregada ---" << endl;
        printCurrentTreeInfo();
    }
}

void processInput(GLFWwindow* window) {
    // Movimento com WASD
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) 
        camera.targetTranslation[0] -= config.moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) 
        camera.targetTranslation[0] += config.moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) 
        camera.targetTranslation[1] += config.moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) 
        camera.targetTranslation[1] -= config.moveSpeed;
    
    // Rotação com Q/E
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) 
        camera.targetRotation += config.rotationSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) 
        camera.targetRotation -= config.rotationSpeed;
    
    limitCameraValues();
}

void printControls() {
    cout << "=== TP1 - Visualizador de Árvores Arteriais 2D ===" << endl;
    cout << "Controles:" << endl;
    cout << "ESC - Sair" << endl;
    cout << "R - Resetar visualização" << endl;
    cout << "WASD - Mover suavemente" << endl;
    cout << "Clique e Arraste - Mover com mouse" << endl;
    cout << "Q/E - Rotacionar suavemente" << endl;
    cout << "Scroll Mouse - Zoom suave" << endl;
    //cout << "T - Alternar Wireframe" << endl;
    cout << "L - Alternar Linhas Adaptativas" << endl;
    cout << "C - Alternar Modo de Cor (Branco -> Verde -> Profundidade -> Descendentes)" << endl;
    cout << "SETAS - Navegar entre árvores" << endl;
    cout << "I - Mostrar informação da árvore atual" << endl;
    cout << endl;
}

// =============================================
// Função Principal
// =============================================

int main() {
    // Inicialização GLFW
    if (!glfwInit()) {
        cerr << "Falha ao inicializar GLFW" << endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1200, 800, 
                                         "TP1 - Visualização de Árvores Arteriais 2D", 
                                         NULL, NULL);
    if (!window) {
        cerr << "Falha ao criar janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    
    // Configura callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Inicialização GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Falha ao iniciar GLAD" << endl;
        return -1;
    }

    // Inicialização do renderizador
    if (!treeRenderer.initialize()) {
        cerr << "Falha ao iniciar renderização da árvore" << endl;
        return -1;
    }

    // Carrega dados
    loadTreeFiles();
    if (!treeFiles.empty()) {
        vtkLoader.loadFile(treeFiles[0]);
        printCurrentTreeInfo();
    }

    // Configuração OpenGL
    glClearColor(config.backgroundColor[0], config.backgroundColor[1], 
                 config.backgroundColor[2], 1.0f);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    // Inicialização
    updateTransformMatrix();
    lastTime = chrono::steady_clock::now();
    printControls();

    // Loop principal
    while (!glfwWindowShouldClose(window)) {
        // Calcula delta time
        auto currentTime = chrono::steady_clock::now();
        float deltaTime = chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Limita delta time para evitar problemas
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        
        // Processamento
        processInput(window);
        updateSmoothTransform(deltaTime);
        
        // Renderização
        glClear(GL_COLOR_BUFFER_BIT);
        treeRenderer.applyTransform(transformMatrix);
        treeRenderer.render(vtkLoader.getSegments());
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    cout << "Programa finalizado!" << endl;
    return 0;
}