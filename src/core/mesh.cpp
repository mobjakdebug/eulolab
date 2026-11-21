#include "mesh.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

namespace eulo {
namespace {
constexpr double pi=3.14159265358979323846;
constexpr size_t maxTriangles=5000,maxVertices=10000,maxBytes=2097152;
bool sane(Vec3 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
        std::fabs(p.x)<=1000000 && std::fabs(p.y)<=1000000 && std::fabs(p.z)<=1000000;
}
bool scalar(const std::string& text,double& value) {
    char* end=nullptr;errno=0;
    value=std::strtod(text.c_str(),&end);
    return text.c_str()!=end && *end=='\0' && errno!=ERANGE && std::isfinite(value);
}
bool vertex(std::istream& stream,Vec3& result) {
    std::string x,y,z;
    return (stream>>x>>y>>z) && scalar(x,result.x) &&
        scalar(y,result.y) && scalar(z,result.z) && sane(result);
}
bool append(std::vector<MeshTriangle>& tris,Vec3 a,Vec3 b,Vec3 c) {
    if(tris.size()>=maxTriangles || !sane(a) || !sane(b) || !sane(c))return false;
    tris.push_back({a,b,c,Surface::Reference,0});
    return true;
}
bool parseObj(const std::string& data,ReferenceMesh& mesh,std::string& error) {
    std::vector<Vec3> positions;
    std::istringstream input(data);
    std::string line;
    while(std::getline(input,line)) {
        if(line.size()>4096) {error="OBJ line exceeds 4096 bytes.";return false;}
        std::istringstream row(line);
        std::string command;
        if(!(row>>command) || command[0]=='#')continue;
        if(command=="v") {
            Vec3 p;
            if(positions.size()>=maxVertices || !vertex(row,p)) {
                error="OBJ vertex is invalid or vertex limit exceeded.";return false;
            }
            positions.push_back(p);
        } else if(command=="f") {
            std::vector<Vec3> polygon;
            std::string field;
            while(row>>field) {
                if(field[0]=='#')break;
                const std::string number=field.substr(0,field.find('/'));
                char* end=nullptr;errno=0;
                const long index=std::strtol(number.c_str(),&end,10);
                if(number.empty() || number.c_str()==end || *end || errno==ERANGE ||
                   index==0 || index<LONG_MIN+static_cast<long>(positions.size()) ||
                   index>static_cast<long>(positions.size()) || polygon.size()>=64) {
                    error="OBJ face uses an invalid vertex index or too many corners.";
                    return false;
                }
                const long resolved=index>0?index-1:
                    static_cast<long>(positions.size())+index;
                if(resolved<0 || static_cast<size_t>(resolved)>=positions.size()) {
                    error="OBJ face refers to a missing vertex.";return false;
                }
                polygon.push_back(positions[static_cast<size_t>(resolved)]);
            }
            if(polygon.size()<3) {error="OBJ face needs at least 3 corners.";return false;}
            for(size_t i=1;i+1<polygon.size();++i)
                if(!append(mesh.triangles,polygon[0],polygon[i],polygon[i+1])) {
                    error="OBJ triangle limit or coordinate range exceeded.";return false;
                }
        }
        // vt/vn, materials, groups and smoothing are ignored; no external
        // texture or material file is read from an imported mesh.
    }
    if(mesh.triangles.empty()) {error="OBJ has no polygon faces.";return false;}
    return true;
}
uint32_t little(const char* p) {
    return static_cast<uint32_t>(static_cast<unsigned char>(p[0])) |
        (static_cast<uint32_t>(static_cast<unsigned char>(p[1]))<<8) |
        (static_cast<uint32_t>(static_cast<unsigned char>(p[2]))<<16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(p[3]))<<24);
}
double binaryFloat(const char* p) {
    const uint32_t bits=little(p);
    float value=0;
    std::memcpy(&value,&bits,sizeof(value));
    return value;
}
bool parseBinaryStl(const std::string& data,ReferenceMesh& mesh,std::string& error) {
    if(!std::numeric_limits<float>::is_iec559) {
        error="This target cannot decode IEEE-754 STL coordinates.";return false;
    }
    const size_t count=little(data.data()+80);
    if(count==0 || count>maxTriangles) {
        error="Binary STL is empty or exceeds 5000 triangles.";return false;
    }
    for(size_t i=0;i<count;++i) {
        const char* p=data.data()+84+i*50+12;
        const Vec3 a{binaryFloat(p),binaryFloat(p+4),binaryFloat(p+8)};
        const Vec3 b{binaryFloat(p+12),binaryFloat(p+16),binaryFloat(p+20)};
        const Vec3 c{binaryFloat(p+24),binaryFloat(p+28),binaryFloat(p+32)};
        if(!append(mesh.triangles,a,b,c)) {
            error="Binary STL has a non-finite or out-of-range vertex.";return false;
        }
    }
    return true;
}
bool parseAsciiStl(const std::string& data,ReferenceMesh& mesh,std::string& error) {
    std::istringstream input(data);
    std::string token;
    if(!(input>>token) || token!="solid") {
        error="STL is neither supported ASCII nor binary mesh data.";return false;
    }
    std::string title;
    std::getline(input,title);
    bool closed=false;
    while(input>>token) {
        if(token=="endsolid") {closed=true;break;}
        if(token!="facet" || !(input>>token) || token!="normal")break;
        Vec3 normal,a,b,c;
        if(!vertex(input,normal) || !(input>>token) || token!="outer" ||
           !(input>>token) || token!="loop" ||
           !(input>>token) || token!="vertex" || !vertex(input,a) ||
           !(input>>token) || token!="vertex" || !vertex(input,b) ||
           !(input>>token) || token!="vertex" || !vertex(input,c) ||
           !(input>>token) || token!="endloop" ||
           !(input>>token) || token!="endfacet" ||
           !append(mesh.triangles,a,b,c))break;
    }
    if(!closed || mesh.triangles.empty()) {
        error="ASCII STL facet is malformed, empty, or exceeds mesh limits.";
        return false;
    }
    return true;
}
void bounds(ReferenceMesh& mesh) {
    mesh.lower=mesh.upper=mesh.triangles.front().a;
    for(const MeshTriangle& tri:mesh.triangles)for(Vec3 p:{tri.a,tri.b,tri.c}) {
        mesh.lower.x=std::min(mesh.lower.x,p.x);mesh.upper.x=std::max(mesh.upper.x,p.x);
        mesh.lower.y=std::min(mesh.lower.y,p.y);mesh.upper.y=std::max(mesh.upper.y,p.y);
        mesh.lower.z=std::min(mesh.lower.z,p.z);mesh.upper.z=std::max(mesh.upper.z,p.z);
    }
}
} // namespace

std::vector<MeshTriangle> visualMesh(const Rocket& r) {
    std::vector<MeshTriangle> faces;
    if(!r.valid())return faces;
    faces.reserve(920);
    constexpr int segments=20;
    for(int j=0;j<12;++j) {
        const double y0=r.bodyLength*j/12.0,y1=r.bodyLength*(j+1)/12.0;
        for(int s=0;s<segments;++s) {
            const double a=2*pi*s/segments,b=2*pi*(s+1)/segments;
            const Vec3 A{r.radius*std::cos(a),y0,r.radius*std::sin(a)};
            const Vec3 B{r.radius*std::cos(b),y0,r.radius*std::sin(b)};
            const Vec3 C{r.radius*std::cos(a),y1,r.radius*std::sin(a)};
            const Vec3 D{r.radius*std::cos(b),y1,r.radius*std::sin(b)};
            faces.push_back({A,B,C,Surface::Body,(a+b)*.5});
            faces.push_back({B,D,C,Surface::Body,(a+b)*.5});
        }
    }
    for(int j=0;j<10;++j) {
        const double t0=j/10.0,t1=(j+1)/10.0;
        const double y0=r.bodyLength+t0*r.noseLength;
        const double y1=r.bodyLength+t1*r.noseLength;
        const double r0=noseRadius(r,t0),r1=noseRadius(r,t1);
        for(int s=0;s<segments;++s) {
            const double a=2*pi*s/segments,b=2*pi*(s+1)/segments;
            const Vec3 A{r0*std::cos(a),y0,r0*std::sin(a)};
            const Vec3 B{r0*std::cos(b),y0,r0*std::sin(b)};
            const Vec3 C{r1*std::cos(a),y1,r1*std::sin(a)};
            const Vec3 D{r1*std::cos(b),y1,r1*std::sin(b)};
            faces.push_back({A,B,C,Surface::Nose,(a+b)*.5});
            faces.push_back({B,D,C,Surface::Nose,(a+b)*.5});
        }
    }
    for(int k=0;k<r.finCount;++k) {
        const double a=2*pi*k/r.finCount;
        const auto point=[a](double radius,double y)->Vec3 {
            return {radius*std::cos(a),y,radius*std::sin(a)};
        };
        const double root0=r.finSetback,root1=root0+r.finRootChord;
        const double tip0=root0+r.finSweep,tip1=tip0+r.finTipChord;
        const Vec3 A=point(r.radius,root0),B=point(r.radius+r.finSpan,tip0);
        const Vec3 C=point(r.radius+r.finSpan,tip1),D=point(r.radius,root1);
        faces.push_back({A,B,C,Surface::Fin,a});
        faces.push_back({A,C,D,Surface::Fin,a});
    }
    if(r.insertEnabled) {
        const double low=r.insertFromTail-r.insertLength/2;
        const double high=r.insertFromTail+r.insertLength/2;
        for(int s=0;s<segments;++s) {
            const double a=2*pi*s/segments,b=2*pi*(s+1)/segments;
            const Vec3 A{r.insertOuterRadius*std::cos(a),low,r.insertOuterRadius*std::sin(a)};
            const Vec3 B{r.insertOuterRadius*std::cos(b),low,r.insertOuterRadius*std::sin(b)};
            const Vec3 C{r.insertOuterRadius*std::cos(a),high,r.insertOuterRadius*std::sin(a)};
            const Vec3 D{r.insertOuterRadius*std::cos(b),high,r.insertOuterRadius*std::sin(b)};
            faces.push_back({A,B,C,Surface::Insert,(a+b)*.5});
            faces.push_back({B,D,C,Surface::Insert,(a+b)*.5});
        }
    }
    return faces;
}

bool ReferenceMesh::load(const char* path,std::string& error) {
    if(!path) {error="No mesh path selected.";return false;}
    const std::string filename(path);
    const auto dot=filename.find_last_of('.');
    const std::string ext=dot==std::string::npos?"":filename.substr(dot);
    if(ext!=".obj" && ext!=".stl") {
        error="Only .obj and .stl visual meshes are supported.";return false;
    }
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) {error="Cannot open reference mesh beside the .eul project.";return false;}
    const auto length=input.tellg();
    if(length<0 || static_cast<size_t>(length)>maxBytes) {
        error="Reference mesh exceeds the 2 MiB limit.";return false;
    }
    input.seekg(0);
    std::string data(static_cast<size_t>(length),'\0');
    if(!input.read(&data[0],length)) {error="Could not read reference mesh.";return false;}
    ReferenceMesh candidate;
    bool ok=false;
    if(ext==".obj")ok=parseObj(data,candidate,error);
    else if(data.size()>=84 &&
            little(data.data()+80)<=maxTriangles &&
            data.size()==84+static_cast<size_t>(little(data.data()+80))*50)
        ok=parseBinaryStl(data,candidate,error);
    else ok=parseAsciiStl(data,candidate,error);
    if(!ok)return false;
    bounds(candidate);
    *this=std::move(candidate);
    error.clear();return true;
}

bool exportVisualObj(const Rocket& r,const char* path) {
    if(!r.valid() || !path)return false;
    const auto mesh=visualMesh(r);
    if(mesh.empty())return false;
    const std::string temp=std::string(path)+".tmp";
    FILE* file=std::fopen(temp.c_str(),"wb");
    if(!file)return false;
    bool ok=std::fputs("# EuloLab visual mesh, coordinates in millimetres.\n"
        "# Not watertight, not a manufacturing or simulation solid.\n",file)>0;
    Surface last=Surface::Reference;
    size_t index=1;
    for(const auto& tri:mesh) {
        if(!ok)break;
        if(tri.surface!=last) {
            const char* group=tri.surface==Surface::Body?"body":
                tri.surface==Surface::Nose?"nose":
                tri.surface==Surface::Fin?"fins":"insert";
            ok=std::fprintf(file,"g %s\n",group)>0;
            last=tri.surface;
        }
        for(Vec3 v:{tri.a,tri.b,tri.c})
            if(ok)ok=std::fprintf(file,"v %.12g %.12g %.12g\n",
                                   v.x*1000,v.y*1000,v.z*1000)>0;
        if(ok)ok=std::fprintf(file,"f %zu %zu %zu\n",index,index+1,index+2)>0;
        index+=3;
    }
    const bool closed=std::fclose(file)==0;
    if(!ok || !closed || std::rename(temp.c_str(),path)!=0) {
        std::remove(temp.c_str());return false;
    }
    return true;
}
} // namespace eulo
