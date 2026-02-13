#ifndef TREERENDERER_H
#define TREERENDERER_H

#include "VTKLoader.h"
#include <vector>
#include <string>
#include <algorithm>
#include "glm.hpp"

// Enumeração para modelos de iluminação
enum class LightingModel {
    PHONG,      // Per-fragment (mais realista)
    GOURAUD,    // Per-vertex (interpolado)
    FLAT        // Flat shading (facetas planas)
};

class TreeRenderer {
public:
    TreeRenderer();
    ~TreeRenderer();
    
    bool initialize();
    void render(const std::vector<Segment>& segments);
    void setLineWidth(float width) { lineWidth = width; }
    void applyTransform(const mat4& modelMatrix);
    void setViewMatrix(const mat4& viewMatrix) { this->viewMatrix = viewMatrix; }
    void setProjectionMatrix(const mat4& projMatrix) { this->projMatrix = projMatrix; }
    void setColorMode(bool monochrome) { useMonochrome = monochrome; }
    void setGradientMode(bool enabled) { gradientMode = enabled; }
    void setThicknessMode(bool enabled) { thicknessMode = enabled; }
    void setDescendantsColorMode(bool enabled) { descendantsColorMode = enabled; }
    void setCylinderMode(bool enabled) { renderCylinders = enabled; }
    void setOpacity(float alpha) { opacity = std::clamp(alpha, 0.0f, 1.0f); }
    float getOpacity() const { return opacity; }
    void setLightingModel(LightingModel model) { currentLighting = model; }
    void cycleLightingModel();
    
private:
    struct RenderData {
        std::vector<float> vertices;
        std::vector<float> colors;
        std::vector<float> thicknesses;
        std::vector<float> normals;
        std::vector<unsigned int> indices;
    };
    
    struct CylinderGeometry {
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<unsigned int> indices;
        int vertexCount;
    };
    
    unsigned int shaderProgram;
    unsigned int shaderPhong;
    unsigned int shaderGouraud;
    unsigned int shaderFlat;
    unsigned int VAO, VBO, EBO, normalVBO;
    float lineWidth;
    float opacity;
    bool useMonochrome;
    bool gradientMode;
    bool thicknessMode;
    bool descendantsColorMode;
    bool renderCylinders;
    LightingModel currentLighting;
    
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
    
    void calculateNodeInfo(const std::vector<Segment>& segments,
                          std::vector<int>& depth,
                          std::vector<int>& descendantCount);
    int findRootSegment(const std::vector<Segment>& segments);
    void buildAdjacencyList(const std::vector<Segment>& segments,
                          std::vector<std::vector<int>>& children);
    
    // Métodos para cilindros
    CylinderGeometry generateCylinder(const Point3D& startPos, const Point3D& endPos,
                                      float startRadius, float endRadius,
                                      int segments = 16);
    void buildCylinderMesh(const std::vector<Segment>& segments,
                          std::vector<float>& vertices,
                          std::vector<float>& normals,
                          std::vector<float>& colors,
                          std::vector<unsigned int>& indices);
    void renderCylinderSegments(const std::vector<Segment>& segments);
    
    std::vector<Segment> createTestTree();
    void renderSegments(const std::vector<Segment>& segments);
    RenderData prepareRenderData(const std::vector<Segment>& segments);
    
    unsigned int compileShader(const std::string& source, unsigned int type);
    unsigned int createShaderProgram(const std::string& vertexSource, 
                                    const std::string& fragmentSource);
};

#endif