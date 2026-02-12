#ifndef TREERENDERER_H
#define TREERENDERER_H

#include "VTKLoader.h"
#include <vector>
#include <string>
#include "glm.hpp"

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
    
private:
    struct RenderData {
        std::vector<float> vertices;
        std::vector<float> colors;
        std::vector<float> thicknesses;
    };
    
    unsigned int shaderProgram;
    unsigned int VAO, VBO;
    float lineWidth;
    bool useMonochrome;
    bool gradientMode;
    bool thicknessMode;
    bool descendantsColorMode;
    
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
    
    void calculateNodeInfo(const std::vector<Segment>& segments,
                          std::vector<int>& depth,
                          std::vector<int>& descendantCount);
    int findRootSegment(const std::vector<Segment>& segments);
    void buildAdjacencyList(const std::vector<Segment>& segments,
                          std::vector<std::vector<int>>& children);
    
    std::vector<Segment> createTestTree();
    void renderSegments(const std::vector<Segment>& segments);
    RenderData prepareRenderData(const std::vector<Segment>& segments);
    
    unsigned int compileShader(const std::string& source, unsigned int type);
    unsigned int createShaderProgram(const std::string& vertexSource, 
                                    const std::string& fragmentSource);
};

#endif