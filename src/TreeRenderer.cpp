#include "TreeRenderer.h"
#include "glad/glad.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <stack>
#include <functional>

TreeRenderer::TreeRenderer() : shaderProgram(0), VAO(0), VBO(0), lineWidth(2.0f), 
                               useMonochrome(false), gradientMode(false), 
                               thicknessMode(false), descendantsColorMode(false) {} // NOVO

TreeRenderer::~TreeRenderer() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

bool TreeRenderer::initialize() {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec3 aColor;
        uniform mat4 transform;
        out vec3 fragColor;
        
        void main() {
            gl_Position = transform * vec4(aPos, 0.0, 1.0);
            fragColor = aColor;
        }
    )";
    
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec3 fragColor;
        out vec4 FragColor;
        
        void main() {
            FragColor = vec4(fragColor, 1.0);
        }
    )";
    
    shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    if (!shaderProgram) return false;
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    std::cout << "TreeRenderer inicializado com sucesso" << std::endl;
    return true;
}

int TreeRenderer::findRootSegment(const std::vector<Segment>& segments) {
    if (segments.empty()) return -1;
    
    std::vector<bool> hasParent(segments.size(), false);
    
    for (size_t i = 0; i < segments.size(); i++) {
        for (size_t j = 0; j < segments.size(); j++) {
            if (i == j) continue;
            
            // Verifica se o segmento j termina onde o segmento i começa
            float dist = std::abs(segments[j].end.x - segments[i].start.x) + 
                       std::abs(segments[j].end.y - segments[i].start.y);
            if (dist < 0.001f) {
                hasParent[i] = true;
                break;
            }
        }
    }
    
    for (size_t i = 0; i < segments.size(); i++) {
        if (!hasParent[i]) return static_cast<int>(i);
    }
    
    return 0;
}

void TreeRenderer::buildAdjacencyList(const std::vector<Segment>& segments,
                                    std::vector<std::vector<int>>& children) {
    children.clear();
    children.resize(segments.size());
    
    for (size_t i = 0; i < segments.size(); i++) {
        for (size_t j = 0; j < segments.size(); j++) {
            if (i == j) continue;
            
            float dist = std::abs(segments[j].start.x - segments[i].end.x) + 
                       std::abs(segments[j].start.y - segments[i].end.y);
            if (dist < 0.001f) {
                children[i].push_back(static_cast<int>(j));
            }
        }
    }
}

void TreeRenderer::calculateNodeInfo(const std::vector<Segment>& segments,
                                   std::vector<int>& depth,
                                   std::vector<int>& descendantCount) {
    depth.resize(segments.size(), -1);
    descendantCount.resize(segments.size(), 0);
    
    if (segments.empty()) return;
    
    std::vector<std::vector<int>> children;
    buildAdjacencyList(segments, children);
    
    int root = findRootSegment(segments);
    if (root == -1) return;
    
    // Calcula profundidade usando BFS
    std::queue<int> q;
    depth[root] = 0;
    q.push(root);
    
    while (!q.empty()) {
        int current = q.front();
        q.pop();
        
        for (int child : children[current]) {
            if (depth[child] == -1) {
                depth[child] = depth[current] + 1;
                q.push(child);
            }
        }
    }
    
    // Calcula número de descendentes recursivamente
    std::function<int(int)> calculateDescendants = [&](int node) -> int {
        int count = 0;
        for (int child : children[node]) {
            count += 1 + calculateDescendants(child);
        }
        descendantCount[node] = count;
        return count;
    };
    
    calculateDescendants(root);
}

TreeRenderer::RenderData TreeRenderer::prepareRenderData(const std::vector<Segment>& segments) {
    RenderData data;
    
    if (segments.empty()) return data;
    
    // Calcula informações dos nós
    std::vector<int> depth;
    std::vector<int> descendantCount;
    calculateNodeInfo(segments, depth, descendantCount);
    
    // Encontra valores máximos para normalização
    int maxDepth = *std::max_element(depth.begin(), depth.end());
    int maxDescendants = *std::max_element(descendantCount.begin(), descendantCount.end());
    
    if (maxDepth == 0) maxDepth = 1;
    if (maxDescendants == 0) maxDescendants = 1;
    
    // Prepara dados de renderização
    data.vertices.reserve(segments.size() * 4);
    data.colors.reserve(segments.size() * 6);
    data.thicknesses.reserve(segments.size());
    
    for (size_t i = 0; i < segments.size(); i++) {
        const auto& segment = segments[i];
        float normalizedDepth = static_cast<float>(depth[i]) / maxDepth;
        float normalizedDescendants = static_cast<float>(descendantCount[i]) / maxDescendants;
        
        // Calcula cor
        float r, g, b;
        
        if (useMonochrome) {
            r = 0.0f;
            g = 1.0f;
            b = 0.0f;
        } else if (gradientMode) {
            // Gradiente bottom-up: Violeta (folhas) -> Vermelho (raiz)
            r = 1.0f - normalizedDepth * 0.5f;
            g = 0.0f;
            b = normalizedDepth * 0.5f;
        } else if (descendantsColorMode) {
            // Gradiente por número de descendentes 
            r = sqrt(normalizedDescendants);           
            g = 0.0f;
            b = 1.0f - normalizedDescendants * normalizedDescendants;    
        } else {
            r = g = b = 1.0f;
        }
        
        // Calcula espessura
        float thickness = lineWidth;
        if (thicknessMode) {
            // ESPESSURA BASEADA NO NÚMERO DE DESCENDENTES
            thickness = 2.0f + normalizedDescendants * 13.0f;
        }
        
        // Adiciona vértices e cores
        data.vertices.push_back(segment.start.x);
        data.vertices.push_back(segment.start.y);
        data.colors.push_back(r);
        data.colors.push_back(g);
        data.colors.push_back(b);
        
        data.vertices.push_back(segment.end.x);
        data.vertices.push_back(segment.end.y);
        data.colors.push_back(r);
        data.colors.push_back(g);
        data.colors.push_back(b);
        
        data.thicknesses.push_back(thickness);
    }
    
    return data;
}

void TreeRenderer::applyTransform(const float* transformMatrix) {
    glUseProgram(shaderProgram);
    GLuint transformLoc = glGetUniformLocation(shaderProgram, "transform");
    if (transformLoc != -1) {
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, transformMatrix);
    }
}

void TreeRenderer::render(const std::vector<Segment>& segments) {
    static bool firstRender = true;
    
    if (segments.empty()) {
        if (firstRender) {
            std::cout << "Nenhuma árvore carregada, renderizando árvore de teste..." << std::endl;
            firstRender = false;
        }
        std::vector<Segment> testSegments = createTestTree();
        renderSegments(testSegments);
        return;
    }
    
    if (firstRender) {
        std::cout << "Renderizando árvore com " << segments.size() << " segmentos" << std::endl;
        firstRender = false;
    }
    
    renderSegments(segments);
}

std::vector<Segment> TreeRenderer::createTestTree() {
    std::vector<Segment> testSegments;
    
    // Tronco principal
    testSegments.emplace_back(Point2D(0.0f, -1.0f), Point2D(0.0f, -0.5f), 0.1f, 0.08f);
    
    // Ramos primários
    testSegments.emplace_back(Point2D(0.0f, -0.5f), Point2D(0.3f, -0.2f), 0.08f, 0.06f);
    testSegments.emplace_back(Point2D(0.0f, -0.5f), Point2D(-0.3f, -0.2f), 0.08f, 0.06f);
    
    // Ramos secundários
    testSegments.emplace_back(Point2D(0.3f, -0.2f), Point2D(0.5f, 0.1f), 0.06f, 0.04f);
    testSegments.emplace_back(Point2D(-0.3f, -0.2f), Point2D(-0.5f, 0.1f), 0.06f, 0.04f);
    
    // Ramos terciários
    testSegments.emplace_back(Point2D(0.5f, 0.1f), Point2D(0.6f, 0.4f), 0.04f, 0.02f);
    testSegments.emplace_back(Point2D(0.5f, 0.1f), Point2D(0.4f, 0.4f), 0.04f, 0.02f);
    testSegments.emplace_back(Point2D(-0.5f, 0.1f), Point2D(-0.6f, 0.4f), 0.04f, 0.02f);
    testSegments.emplace_back(Point2D(-0.5f, 0.1f), Point2D(-0.4f, 0.4f), 0.04f, 0.02f);
    
    return testSegments;
}

void TreeRenderer::renderSegments(const std::vector<Segment>& segments) {
    RenderData data = prepareRenderData(segments);
    
    if (data.vertices.empty()) return;
    
    if (thicknessMode && !data.thicknesses.empty()) {
        // Renderiza segmento por segmento com espessuras diferentes
        for (size_t i = 0; i < segments.size(); i++) {
            float thickness = std::clamp(data.thicknesses[i], 1.0f, 10.0f);
            glLineWidth(thickness);
            
            std::vector<float> segmentData;
            size_t baseIdx = i * 2;
            
            // Vértice inicial
            segmentData.insert(segmentData.end(), {
                data.vertices[baseIdx * 2],
                data.vertices[baseIdx * 2 + 1],
                data.colors[baseIdx * 3],
                data.colors[baseIdx * 3 + 1],
                data.colors[baseIdx * 3 + 2]
            });
            
            // Vértice final
            segmentData.insert(segmentData.end(), {
                data.vertices[(baseIdx + 1) * 2],
                data.vertices[(baseIdx + 1) * 2 + 1],
                data.colors[(baseIdx + 1) * 3],
                data.colors[(baseIdx + 1) * 3 + 1],
                data.colors[(baseIdx + 1) * 3 + 2]
            });
            
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, segmentData.size() * sizeof(float), 
                        segmentData.data(), GL_STATIC_DRAW);
            glDrawArrays(GL_LINES, 0, 2);
        }
    } else {
        // Renderiza todos os segmentos de uma vez
        glLineWidth(lineWidth);
        
        std::vector<float> interleavedData;
        interleavedData.reserve(data.vertices.size() / 2 * 5);
        
        for (size_t i = 0; i < data.vertices.size() / 2; i++) {
            interleavedData.push_back(data.vertices[i * 2]);
            interleavedData.push_back(data.vertices[i * 2 + 1]);
            interleavedData.push_back(data.colors[i * 3]);
            interleavedData.push_back(data.colors[i * 3 + 1]);
            interleavedData.push_back(data.colors[i * 3 + 2]);
        }
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, interleavedData.size() * sizeof(float), 
                    interleavedData.data(), GL_STATIC_DRAW);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(data.vertices.size() / 2));
    }
    
    glBindVertexArray(0);
}

unsigned int TreeRenderer::compileShader(const std::string& source, unsigned int type) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Erro de compilação do shader: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

unsigned int TreeRenderer::createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
    unsigned int vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    
    if (!vertexShader || !fragmentShader) {
        std::cerr << "Erro: Shaders não compilados corretamente" << std::endl;
        return 0;
    }
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Erro de linking do programa: " << infoLog << std::endl;
        glDeleteProgram(program);
        program = 0;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}