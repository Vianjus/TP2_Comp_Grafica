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

TreeRenderer::TreeRenderer() : shaderProgram(0), VAO(0), VBO(0), EBO(0), normalVBO(0), lineWidth(2.0f), 
                               useMonochrome(false), gradientMode(false), 
                               thicknessMode(false), descendantsColorMode(false), renderCylinders(true) {
    modelMatrix = identity();
    viewMatrix = identity();
    projMatrix = identity();
}

// Helper functions para operações vetoriais
namespace {
    inline vec3 normalize_vec3(const vec3& v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len < 0.0001f) return v;
        return vec3(v.x / len, v.y / len, v.z / len);
    }
    
    inline vec3 cross_vec3(const vec3& a, const vec3& b) {
        return vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }
}

TreeRenderer::~TreeRenderer() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
    if (normalVBO) glDeleteBuffers(1, &normalVBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
}

bool TreeRenderer::initialize() {
    // Shader para cilindros (com normais)
    const char* cylinderVertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;
        layout (location = 2) in vec3 aNormal;
        
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;
        
        out vec3 fragColor;
        out vec3 fragNormal;
        out vec3 fragPos;
        
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            fragColor = aColor;
            fragPos = vec3(model * vec4(aPos, 1.0));
            fragNormal = mat3(transpose(inverse(model))) * aNormal;
        }
    )";
    
    const char* cylinderFragmentShaderSource = R"(
        #version 330 core
        in vec3 fragColor;
        in vec3 fragNormal;
        in vec3 fragPos;
        out vec4 FragColor;
        
        void main() {
            // Simples iluminação Phong
            vec3 norm = normalize(fragNormal);
            vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
            
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * fragColor;
            
            vec3 ambient = vec3(0.3) * fragColor;
            
            FragColor = vec4(ambient + diffuse, 1.0);
        }
    )";
    
    // Shader para linhas (simples)
    const char* lineVertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;
        
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;
        
        out vec3 fragColor;
        
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            fragColor = aColor;
        }
    )";
    
    const char* lineFragmentShaderSource = R"(
        #version 330 core
        in vec3 fragColor;
        out vec4 FragColor;
        
        void main() {
            FragColor = vec4(fragColor, 1.0);
        }
    )";
    
    shaderProgram = createShaderProgram(cylinderVertexShaderSource, cylinderFragmentShaderSource);
    if (!shaderProgram) return false;
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenBuffers(1, &normalVBO);
    
    glBindVertexArray(VAO);
    
    // Position attribute (3D)
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute (usando outro buffer)
    unsigned int colorVBO = 0;
    glGenBuffers(1, &colorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    
    // Normal attribute
    glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    
    glBindVertexArray(0);
    glDeleteBuffers(1, &colorVBO);
    
    std::cout << "TreeRenderer inicializado com renderizacao de cilindros 3D" << std::endl;
    return true;
}

TreeRenderer::CylinderGeometry TreeRenderer::generateCylinder(const Point3D& startPos, const Point3D& endPos,
                                                              float startRadius, float endRadius,
                                                              int numSegments) {
    CylinderGeometry geometry;
    
    // Calcula direção e comprimento
    vec3 direction = vec3(endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z);
    float len = sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    
    if (len < 0.0001f) {
        geometry.vertexCount = 0;
        return geometry;
    }
    
    direction = normalize_vec3(direction);
    
    // Calcula vetores perpendiculares
    vec3 up = vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(direction.y) > 0.99f) {
        up = vec3(1.0f, 0.0f, 0.0f);
    }
    
    vec3 right = normalize_vec3(cross_vec3(direction, up));
    vec3 actualUp = cross_vec3(right, direction);
    
    // Gera vértices do cilindro
    float angleStep = 2.0f * 3.14159265f / numSegments;
    
    // Vértices da base (start)
    for (int i = 0; i < numSegments; i++) {
        float angle = i * angleStep;
        float x = std::cos(angle);
        float z = std::sin(angle);
        
        vec3 offset = right * x * startRadius + actualUp * z * startRadius;
        vec3 vertex = vec3(startPos.x, startPos.y, startPos.z) + offset;
        
        geometry.vertices.push_back(vertex.x);
        geometry.vertices.push_back(vertex.y);
        geometry.vertices.push_back(vertex.z);
        
        // Normal radial
        vec3 normal = normalize_vec3(right * x + actualUp * z);
        geometry.normals.push_back(normal.x);
        geometry.normals.push_back(normal.y);
        geometry.normals.push_back(normal.z);
    }
    
    // Vértices do topo (end)
    for (int i = 0; i < numSegments; i++) {
        float angle = i * angleStep;
        float x = std::cos(angle);
        float z = std::sin(angle);
        
        vec3 offset = right * x * endRadius + actualUp * z * endRadius;
        vec3 vertex = vec3(endPos.x, endPos.y, endPos.z) + offset;
        
        geometry.vertices.push_back(vertex.x);
        geometry.vertices.push_back(vertex.y);
        geometry.vertices.push_back(vertex.z);
        
        // Normal radial
        vec3 normal = normalize_vec3(right * x + actualUp * z);
        geometry.normals.push_back(normal.x);
        geometry.normals.push_back(normal.y);
        geometry.normals.push_back(normal.z);
    }
    
    // Gera índices para os triângulos (lado do cilindro)
    for (int i = 0; i < numSegments; i++) {
        int next = (i + 1) % numSegments;
        
        // Quadrado dividido em 2 triângulos
        unsigned int base = i;
        unsigned int baseNext = next;
        unsigned int top = numSegments + i;
        unsigned int topNext = numSegments + next;
        
        // Primeiro triângulo
        geometry.indices.push_back(base);
        geometry.indices.push_back(top);
        geometry.indices.push_back(baseNext);
        
        // Segundo triângulo
        geometry.indices.push_back(baseNext);
        geometry.indices.push_back(top);
        geometry.indices.push_back(topNext);
    }
    
    // Gera bases (opcional - pode desabilitar para melhor performance)
    // Base inferior
    int baseStartIndex = numSegments * 2;
    geometry.vertices.push_back(startPos.x);
    geometry.vertices.push_back(startPos.y);
    geometry.vertices.push_back(startPos.z);
    geometry.normals.push_back(-direction.x);
    geometry.normals.push_back(-direction.y);
    geometry.normals.push_back(-direction.z);
    int baseCenterIdx = baseStartIndex;
    
    for (int i = 0; i < numSegments; i++) {
        int current = i;
        int next = (i + 1) % numSegments;
        geometry.indices.push_back(baseCenterIdx);
        geometry.indices.push_back(next);
        geometry.indices.push_back(current);
    }
    
    // Base superior
    geometry.vertices.push_back(endPos.x);
    geometry.vertices.push_back(endPos.y);
    geometry.vertices.push_back(endPos.z);
    geometry.normals.push_back(direction.x);
    geometry.normals.push_back(direction.y);
    geometry.normals.push_back(direction.z);
    int topCenterIdx = baseStartIndex + 1;
    
    for (int i = 0; i < numSegments; i++) {
        int current = numSegments + i;
        int next = numSegments + (i + 1) % numSegments;
        geometry.indices.push_back(topCenterIdx);
        geometry.indices.push_back(current);
        geometry.indices.push_back(next);
    }
    
    geometry.vertexCount = geometry.indices.size();
    return geometry;
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

void TreeRenderer::buildCylinderMesh(const std::vector<Segment>& segments,
                                    std::vector<float>& vertices,
                                    std::vector<float>& normals,
                                    std::vector<float>& colors,
                                    std::vector<unsigned int>& indices) {
    vertices.clear();
    normals.clear();
    colors.clear();
    indices.clear();
    
    // Calcula informações dos nós
    std::vector<int> depth;
    std::vector<int> descendantCount;
    calculateNodeInfo(segments, depth, descendantCount);
    
    // Encontra valores máximos para normalização
    int maxDepth = *std::max_element(depth.begin(), depth.end());
    int maxDescendants = *std::max_element(descendantCount.begin(), descendantCount.end());
    
    if (maxDepth == 0) maxDepth = 1;
    if (maxDescendants == 0) maxDescendants = 1;
    
    unsigned int currentVertexOffset = 0;
    
    for (size_t i = 0; i < segments.size(); i++) {
        const auto& segment = segments[i];
        
        // Calcula cor
        float normalizedDepth = static_cast<float>(depth[i]) / maxDepth;
        float normalizedDescendants = static_cast<float>(descendantCount[i]) / maxDescendants;
        
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
        
        // Gera cilindro para este segmento
        float radiusStart = segment.startRadius * 0.3f;  // Reduz tamanho em 70%
        float radiusEnd = segment.endRadius * 0.3f;
        
        // Suaviza transição: interpola mais linear ao invés de cônico agressivo
        float avgRadius = (radiusStart + radiusEnd) * 0.5f;
        radiusStart = avgRadius;
        radiusEnd = avgRadius * 0.85f;  // Apenas 15% de redução
        
        // Se thicknessMode estiver ativo, modula os raios
        if (thicknessMode) {
            float scale = 1.0f + normalizedDescendants * 1.0f;  // Reduz escala de 2.0 para 1.0
            radiusStart *= scale;
            radiusEnd *= scale;
        }
        
        // Usa número menor de segmentos para melhor performance
        CylinderGeometry cylinder = generateCylinder(segment.start, segment.end,
                                                    radiusStart, radiusEnd, 12);
        
        // Adiciona vértices e normals
        for (size_t j = 0; j < cylinder.vertices.size(); j++) {
            vertices.push_back(cylinder.vertices[j]);
            normals.push_back(cylinder.normals[j]);
        }
        
        // Adiciona cores
        for (size_t j = 0; j < cylinder.vertices.size() / 3; j++) {
            colors.push_back(r);
            colors.push_back(g);
            colors.push_back(b);
        }
        
        // Adiciona índices com offset
        for (unsigned int idx : cylinder.indices) {
            indices.push_back(idx + currentVertexOffset);
        }
        
        currentVertexOffset += cylinder.vertices.size() / 3;
    }
}

void TreeRenderer::renderCylinderSegments(const std::vector<Segment>& segments) {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<unsigned int> indices;
    
    buildCylinderMesh(segments, vertices, normals, colors, indices);
    
    if (vertices.empty() || indices.empty()) return;
    
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    
    // Upload vertices
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Upload colors
    unsigned int colorVBO;
    glGenBuffers(1, &colorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    
    // Upload normals
    glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    
    // Upload indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    // Draw
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    
    // Cleanup
    glDeleteBuffers(1, &colorVBO);
    glBindVertexArray(0);
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
    data.vertices.reserve(segments.size() * 6);  // 3D: x, y, z para cada ponto
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
        
        // Adiciona vértices 3D e cores
        data.vertices.push_back(segment.start.x);
        data.vertices.push_back(segment.start.y);
        data.vertices.push_back(segment.start.z);
        data.colors.push_back(r);
        data.colors.push_back(g);
        data.colors.push_back(b);
        
        data.vertices.push_back(segment.end.x);
        data.vertices.push_back(segment.end.y);
        data.vertices.push_back(segment.end.z);
        data.colors.push_back(r);
        data.colors.push_back(g);
        data.colors.push_back(b);
        
        data.thicknesses.push_back(thickness);
    }
    
    return data;
}

void TreeRenderer::applyTransform(const mat4& modelMatrix) {
    glUseProgram(shaderProgram);
    
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLuint projLoc = glGetUniformLocation(shaderProgram, "projection");
    
    if (modelLoc != -1) {
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(modelMatrix));
    }
    if (viewLoc != -1) {
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(viewMatrix));
    }
    if (projLoc != -1) {
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(projMatrix));
    }
}

void TreeRenderer::render(const std::vector<Segment>& segments) {
    static bool firstRender = true;
    
    if (segments.empty()) {
        if (firstRender) {
            std::cout << "Nenhuma arvore carregada, renderizando arvore de teste..." << std::endl;
            firstRender = false;
        }
        std::vector<Segment> testSegments = createTestTree();
        if (renderCylinders) {
            renderCylinderSegments(testSegments);
        } else {
            renderSegments(testSegments);
        }
        return;
    }
    
    if (firstRender) {
        std::cout << "Renderizando arvore com " << segments.size() << " segmentos";
        if (renderCylinders) {
            std::cout << " como cilindros 3D" << std::endl;
        } else {
            std::cout << std::endl;
        }
        firstRender = false;
    }
    
    if (renderCylinders) {
        renderCylinderSegments(segments);
    } else {
        renderSegments(segments);
    }
}

std::vector<Segment> TreeRenderer::createTestTree() {
    std::vector<Segment> testSegments;
    
    // Tronco principal
    testSegments.emplace_back(Point3D(0.0f, -1.0f, 0.0f), Point3D(0.0f, -0.5f, 0.0f), 0.1f, 0.08f);
    
    // Ramos primários
    testSegments.emplace_back(Point3D(0.0f, -0.5f, 0.0f), Point3D(0.3f, -0.2f, 0.0f), 0.08f, 0.06f);
    testSegments.emplace_back(Point3D(0.0f, -0.5f, 0.0f), Point3D(-0.3f, -0.2f, 0.0f), 0.08f, 0.06f);
    testSegments.emplace_back(Point3D(0.0f, -0.5f, 0.0f), Point3D(0.0f, -0.2f, 0.3f), 0.08f, 0.06f);
    testSegments.emplace_back(Point3D(0.0f, -0.5f, 0.0f), Point3D(0.0f, -0.2f, -0.3f), 0.08f, 0.06f);
    
    // Ramos secundários
    testSegments.emplace_back(Point3D(0.3f, -0.2f, 0.0f), Point3D(0.5f, 0.1f, 0.0f), 0.06f, 0.04f);
    testSegments.emplace_back(Point3D(-0.3f, -0.2f, 0.0f), Point3D(-0.5f, 0.1f, 0.0f), 0.06f, 0.04f);
    testSegments.emplace_back(Point3D(0.0f, -0.2f, 0.3f), Point3D(0.0f, 0.1f, 0.5f), 0.06f, 0.04f);
    testSegments.emplace_back(Point3D(0.0f, -0.2f, -0.3f), Point3D(0.0f, 0.1f, -0.5f), 0.06f, 0.04f);
    
    // Ramos terciários
    testSegments.emplace_back(Point3D(0.5f, 0.1f, 0.0f), Point3D(0.6f, 0.4f, 0.0f), 0.04f, 0.02f);
    testSegments.emplace_back(Point3D(0.5f, 0.1f, 0.0f), Point3D(0.4f, 0.4f, 0.2f), 0.04f, 0.02f);
    testSegments.emplace_back(Point3D(-0.5f, 0.1f, 0.0f), Point3D(-0.6f, 0.4f, 0.0f), 0.04f, 0.02f);
    testSegments.emplace_back(Point3D(-0.5f, 0.1f, 0.0f), Point3D(-0.4f, 0.4f, 0.2f), 0.04f, 0.02f);
    testSegments.emplace_back(Point3D(0.0f, 0.1f, 0.5f), Point3D(0.0f, 0.4f, 0.6f), 0.04f, 0.02f);
    testSegments.emplace_back(Point3D(0.0f, 0.1f, -0.5f), Point3D(0.0f, 0.4f, -0.6f), 0.04f, 0.02f);
    
    return testSegments;
}

void TreeRenderer::renderSegments(const std::vector<Segment>& segments) {
    RenderData data = prepareRenderData(segments);
    
    if (data.vertices.empty()) return;
    
    glUseProgram(shaderProgram);  // Reutiliza shader atual
    
    if (thicknessMode && !data.thicknesses.empty()) {
        // Renderiza segmento por segmento com espessuras diferentes
        for (size_t i = 0; i < segments.size(); i++) {
            float thickness = std::clamp(data.thicknesses[i], 1.0f, 10.0f);
            glLineWidth(thickness);
            
            std::vector<float> segmentData;
            size_t baseIdx = i * 2;
            
            // Vértice inicial (x, y, z, r, g, b)
            segmentData.insert(segmentData.end(), {
                data.vertices[baseIdx * 3],
                data.vertices[baseIdx * 3 + 1],
                data.vertices[baseIdx * 3 + 2],
                data.colors[baseIdx * 3],
                data.colors[baseIdx * 3 + 1],
                data.colors[baseIdx * 3 + 2]
            });
            
            // Vértice final
            segmentData.insert(segmentData.end(), {
                data.vertices[(baseIdx + 1) * 3],
                data.vertices[(baseIdx + 1) * 3 + 1],
                data.vertices[(baseIdx + 1) * 3 + 2],
                data.colors[(baseIdx + 1) * 3],
                data.colors[(baseIdx + 1) * 3 + 1],
                data.colors[(baseIdx + 1) * 3 + 2]
            });
            
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, segmentData.size() * sizeof(float), 
                        segmentData.data(), GL_STATIC_DRAW);
            
            // Config VAO para linhas (sem normais)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3*sizeof(float)));
            glEnableVertexAttribArray(1);
            glDisableVertexAttribArray(2);  // Desabilita normais
            
            glDrawArrays(GL_LINES, 0, 2);
        }
    } else {
        // Renderiza todos os segmentos de uma vez
        glLineWidth(lineWidth);
        
        std::vector<float> interleavedData;
        interleavedData.reserve(data.vertices.size() / 3 * 6);
        
        for (size_t i = 0; i < data.vertices.size() / 3; i++) {
            interleavedData.push_back(data.vertices[i * 3]);
            interleavedData.push_back(data.vertices[i * 3 + 1]);
            interleavedData.push_back(data.vertices[i * 3 + 2]);
            interleavedData.push_back(data.colors[i * 3]);
            interleavedData.push_back(data.colors[i * 3 + 1]);
            interleavedData.push_back(data.colors[i * 3 + 2]);
        }
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, interleavedData.size() * sizeof(float), 
                    interleavedData.data(), GL_STATIC_DRAW);
        
        // Config VAO para linhas (sem normais)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        glDisableVertexAttribArray(2);  // Desabilita normais
        
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(data.vertices.size() / 3));
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
        std::cerr << "Erro: Shaders nao compilados corretamente" << std::endl;
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