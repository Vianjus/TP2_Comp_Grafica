#include "VTKLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <queue>
#include <functional>

VTKLoader::VTKLoader() {}

bool VTKLoader::loadFile(const std::string& filename) {
    std::cout << "Carregando: " << filename << std::endl;
    
    segments.clear();
    points.clear();

    if (loadRealVTKFile(filename)) {
        std::cout << "[+] Arquivo VTK carregado: " << segments.size() << " segmentos" << std::endl;
        return true;
    }
    
    std::cout << "[!] Arquivo não encontrado, gerando árvore procedural" << std::endl;
    generateProceduralTree();
    std::cout << "[+] Árvore procedural: " << segments.size() << " segmentos" << std::endl;
    
    return true;
}

bool VTKLoader::loadRealVTKFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::vector<Point2D> tempPoints;
    std::vector<std::pair<int, int>> connections;
    std::vector<float> radii;

    int pointsCount = 0, linesCount = 0;
    bool inPointsSection = false, inLinesSection = false, inRadiiSection = false;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string token;
        
        if (line.find("POINTS") == 0) {
            iss >> token >> pointsCount >> token;
            tempPoints.reserve(pointsCount);
            inPointsSection = true;
            inLinesSection = inRadiiSection = false;
            continue;
        }
        else if (line.find("LINES") == 0) {
            iss >> token >> linesCount;
            int totalValues;
            iss >> totalValues;
            connections.reserve(linesCount);
            inLinesSection = true;
            inPointsSection = inRadiiSection = false;
            continue;
        }
        else if (line.find("RADIUS") == 0 || line.find("SCALARS") == 0) {
            inRadiiSection = true;
            inPointsSection = inLinesSection = false;
            continue;
        }
        else if (line.find("LOOKUP_TABLE") == 0) {
            continue;
        }

        if (inPointsSection && pointsCount > 0) {
            float x, y, z;
            if (iss >> x >> y >> z) {
                tempPoints.emplace_back(x, y);
                if (--pointsCount == 0) inPointsSection = false;
            }
        }
        else if (inLinesSection && linesCount > 0) {
            int numPoints;
            if (iss >> numPoints) {
                if (numPoints == 2) {
                    int p1, p2;
                    if (iss >> p1 >> p2) {
                        connections.emplace_back(p1, p2);
                        linesCount--;
                    }
                } else {
                    // Pula polylines
                    for (int i = 0; i < numPoints; i++) {
                        int dummy;
                        iss >> dummy;
                    }
                    linesCount--;
                }
            }
        }
        else if (inRadiiSection) {
            float radius;
            while (iss >> radius) {
                radii.push_back(radius);
            }
        }
    }

    file.close();

    if (tempPoints.empty() || connections.empty()) {
        return false;
    }

    points = std::move(tempPoints);
    
    // Normalização das coordenadas
    float minX = points[0].x, maxX = points[0].x;
    float minY = points[0].y, maxY = points[0].y;
    
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    
    float scaleX = 2.0f / (maxX - minX);
    float scaleY = 2.0f / (maxY - minY);
    float scale = std::min(scaleX, scaleY) * 0.8f;
    
    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;
    
    segments.reserve(connections.size());
    
    for (const auto& conn : connections) {
        if (conn.first >= points.size() || conn.second >= points.size()) continue;
        
        Segment seg;
        
        seg.start.x = (points[conn.first].x - centerX) * scale;
        seg.start.y = (points[conn.first].y - centerY) * scale;
        seg.end.x = (points[conn.second].x - centerX) * scale;
        seg.end.y = (points[conn.second].y - centerY) * scale;
        
        if (conn.first < radii.size() && conn.second < radii.size()) {
            seg.startRadius = radii[conn.first] * scale * 0.5f;
            seg.endRadius = radii[conn.second] * scale * 0.5f;
        } else {
            seg.startRadius = 0.03f;
            seg.endRadius = 0.01f;
        }
        
        seg.parentIndex = -1;
        segments.push_back(seg);
    }
    
    return !segments.empty();
}

void VTKLoader::generateProceduralTree() {
    segments.clear();
    points.clear();
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.05f, 0.05f);
    
    points.emplace_back(0.0f, -0.8f);
    
    std::function<int(Point2D, Point2D, float, float, int, int)> generateBranch;
    
    generateBranch = [&](Point2D start, Point2D direction, float length, 
                         float startRadius, int depth, int parentPointIdx) -> int {
        if (depth <= 0 || length < 0.01f) return -1;
        
        Point2D end;
        end.x = start.x + direction.x * length + dist(rng);
        end.y = start.y + direction.y * length + dist(rng);
        
        int endPointIdx = static_cast<int>(points.size());
        points.push_back(end);
        
        Segment seg;
        seg.start = start;
        seg.end = end;
        seg.startRadius = startRadius;
        seg.endRadius = startRadius * 0.7f;
        seg.parentIndex = parentPointIdx;
        segments.push_back(seg);
        
        if (depth > 1) {
            int numBranches = (depth > 3) ? 2 : 1;
            
            for (int i = 0; i < numBranches; i++) {
                float angle = (i == 0) ? 0.5f : -0.5f;
                
                Point2D newDir;
                newDir.x = direction.x * cos(angle) - direction.y * sin(angle);
                newDir.y = direction.x * sin(angle) + direction.y * cos(angle);
                
                float mag = sqrt(newDir.x * newDir.x + newDir.y * newDir.y);
                if (mag > 0) {
                    newDir.x /= mag;
                    newDir.y /= mag;
                }
                
                generateBranch(end, newDir, length * 0.6f, startRadius * 0.7f, 
                              depth - 1, endPointIdx);
            }
        }
        
        return endPointIdx;
    };
    
    // Gera árvore
    generateBranch(points[0], Point2D(0.0f, 1.0f), 0.6f, 0.08f, 6, 0);
    generateBranch(Point2D(0.0f, -0.6f), Point2D(0.8f, 0.4f), 0.3f, 0.04f, 4, 0);
    generateBranch(Point2D(0.0f, -0.6f), Point2D(-0.8f, 0.4f), 0.3f, 0.04f, 4, 0);
    generateBranch(Point2D(0.0f, -0.3f), Point2D(0.9f, 0.2f), 0.25f, 0.03f, 3, 0);
    generateBranch(Point2D(0.0f, -0.3f), Point2D(-0.9f, 0.2f), 0.25f, 0.03f, 3, 0);
}

void VTKLoader::clear() {
    segments.clear();
    points.clear();
}