#ifndef VTKLOADER_H
#define VTKLOADER_H

#include <vector>
#include <string>

struct Point3D {
    float x, y, z;
    Point3D(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}
};

struct Segment {
    Point3D start, end;
    float startRadius, endRadius;
    int parentIndex;
    
    Segment(Point3D s = Point3D(), Point3D e = Point3D(), 
            float sr = 0.1f, float er = 0.05f, int parent = -1) 
        : start(s), end(e), startRadius(sr), endRadius(er), parentIndex(parent) {}
};

class VTKLoader {
public:
    VTKLoader();
    bool loadFile(const std::string& filename);
    void clear();
    
    const std::vector<Segment>& getSegments() const { return segments; }
    const std::vector<Point3D>& getPoints() const { return points; }
    bool hasData() const { return !segments.empty(); }
    
private:
    std::vector<Segment> segments;
    std::vector<Point3D> points;
    
    void generateProceduralTree();
    bool loadRealVTKFile(const std::string& filename);
};

#endif