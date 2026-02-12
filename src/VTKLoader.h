#ifndef VTKLOADER_H
#define VTKLOADER_H

#include <vector>
#include <string>

struct Point2D {
    float x, y;
    Point2D(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}
};

struct Segment {
    Point2D start, end;
    float startRadius, endRadius;
    int parentIndex;
    
    Segment(Point2D s = Point2D(), Point2D e = Point2D(), 
            float sr = 0.1f, float er = 0.05f, int parent = -1) 
        : start(s), end(e), startRadius(sr), endRadius(er), parentIndex(parent) {}
};

class VTKLoader {
public:
    VTKLoader();
    bool loadFile(const std::string& filename);
    void clear();
    
    const std::vector<Segment>& getSegments() const { return segments; }
    const std::vector<Point2D>& getPoints() const { return points; }
    bool hasData() const { return !segments.empty(); }
    
private:
    std::vector<Segment> segments;
    std::vector<Point2D> points;
    
    void generateProceduralTree();
    bool loadRealVTKFile(const std::string& filename);
};

#endif