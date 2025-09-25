#include "kv3-parser.hpp"
#include <algorithm>
#include <fstream>
#include <stdlib.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
namespace fs = std::filesystem;

// credits tni & learn_more (pasted from www.unknowncheats.me/forum/3868338-post34.html)
#define INRANGE(x,a,b)      (x >= a && x <= b) 
#define getBits( x )        (INRANGE(x,'0','9') ? (x - '0') : ((x&(~0x20)) - 'A' + 0xa))
#define get_byte( x )       (getBits(x[0]) << 4 | getBits(x[1]))

template <typename Ty>
vector<Ty> bytes_to_vec(const string& bytes)
{
    if (bytes.empty()) {
        return vector<Ty>();
    }
    
    const auto num_bytes = bytes.size() / 3;
    const auto num_elements = num_bytes / sizeof(Ty);

    vector<Ty> vec;
    vec.resize(num_elements + 1);

    const char* p1 = bytes.c_str();
    uint8_t* p2 = reinterpret_cast<uint8_t*>(vec.data());
    while (*p1 != '\0')
    {
        if (*p1 == ' ')
        {
            ++p1;
        }
        else
        {
            *p2++ = get_byte(p1);
            p1 += 2;
        }
    }

    return vec;
}

struct Vector3 {
    float x, y, z;
};
struct Triangle {
    Vector3 p1, p2, p3;
};
struct Edge {
    uint8_t next, twin, origin, face;
};

vector<string> get_vphys_files() {
    vector<string> vphys_files;
    
    // Check if input directory exists, create it if it doesn't
    if (!fs::exists("input")) {
        fs::create_directory("input");
        cout << "Created input directory. Please place your .vphys files in the input/ directory." << endl;
    }
    
    for (const auto& entry : fs::directory_iterator("input")) {
        if (entry.path().extension() == ".vphys") {
            vphys_files.push_back(entry.path().string());
        }
    }
    return vphys_files;
}

// Helper function to clean and normalize collision group strings
string clean_collision_string(const string& str) {
    string cleaned = str;
    // Remove quotes if present
    if (cleaned.length() >= 2 && cleaned.front() == '"') {
        // Find the last quote, ignoring trailing whitespace/newlines
        size_t last_quote = cleaned.find_last_of('"');
        if (last_quote != string::npos && last_quote > 0) {
            cleaned = cleaned.substr(1, last_quote - 1);
        }
    }
    // Convert to lowercase for case-insensitive comparison
    transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);
    return cleaned;
}

vector<int> get_collision_attribute_indices(c_kv3_parser parser) {
    vector<int> indices;
    int index = 0;
    while (true) {
        string index_str = to_string(index);
        string collision_group_string = parser.get_value("m_collisionAttributes[" + index_str + "].m_CollisionGroupString");
        if (collision_group_string != "") {
            string cleaned = clean_collision_string(collision_group_string);
            if (cleaned == "default") {
                indices.push_back(index);
            }
        }
        else {
            break;
        }
        index++;
    }
    return indices;
}

int main()
{
    vector<string> vphys_files = get_vphys_files();

    // Create output directory if it doesn't exist
    if (!fs::exists("output")) {
        fs::create_directory("output");
    }

    for (const auto& file_name : vphys_files) {
        string export_file_name = "output/" + fs::path(file_name).stem().string() + ".tri";

        c_kv3_parser parser;
        vector<Triangle> triangles;

        ifstream in(file_name, ios::in);
        istreambuf_iterator<char> beg(in), end;
        string strdata(beg, end);
        in.close();

        parser.parse(strdata);

        string().swap(strdata);

        int index = 0;
        int count_hulls = 0;
        int count_meshes = 0;

        vector<int> collision_attribute_indices = get_collision_attribute_indices(parser);

        //check hulls 
        while (true) {
            string index_str = to_string(index);
            string collision_index_str = parser.get_value("m_parts[0].m_rnShape.m_hulls[" + index_str + "].m_nCollisionAttributeIndex");
            if (collision_index_str != "") {
                int collision_index = atoi(collision_index_str.c_str());
                if (std::find(collision_attribute_indices.begin(), collision_attribute_indices.end(), collision_index) != collision_attribute_indices.end()) {
                    vector<float> vertex_processed{};
                    
                    string vertex_positions_str = parser.get_value("m_parts[0].m_rnShape.m_hulls[" + index_str + "].m_Hull.m_VertexPositions");
                    if (!vertex_positions_str.empty())
                       vertex_processed = bytes_to_vec<float>(vertex_positions_str);
                    else
                       vertex_processed = bytes_to_vec<float>(parser.get_value("m_parts[0].m_rnShape.m_hulls[" + index_str + "].m_Hull.m_Vertices"));

                    if (vertex_processed.empty()) {
                        index++;
                        continue;
                    }

                    vector<Vector3> vertices;
                    for (int i = 0; i < vertex_processed.size(); i += 3) {
                        vertices.push_back({ vertex_processed[i], vertex_processed[i + 1], vertex_processed[i + 2] });
                    }
                    vector<float>().swap(vertex_processed);

                    string faces_str = parser.get_value("m_parts[0].m_rnShape.m_hulls[" + index_str + "].m_Hull.m_Faces");
                    string edges_str = parser.get_value("m_parts[0].m_rnShape.m_hulls[" + index_str + "].m_Hull.m_Edges");
                    
                    if (faces_str.empty() || edges_str.empty()) {
                        vector<Vector3>().swap(vertices);
                        index++;
                        continue;
                    }

                    vector<uint8_t> faces_processed = bytes_to_vec<uint8_t>(faces_str);
                    vector<uint8_t> edges_tmp = bytes_to_vec<uint8_t>(edges_str);
                    
                    if (faces_processed.empty() || edges_tmp.empty()) {
                        vector<Vector3>().swap(vertices);
                        index++;
                        continue;
                    }
                    
                    vector<Edge> edges_processed;
                    for (int i = 0; i < edges_tmp.size(); i += 4) {
                        edges_processed.push_back({ edges_tmp[i], edges_tmp[i + 1], edges_tmp[i + 2], edges_tmp[i + 3] });
                    }
                    vector<uint8_t>().swap(edges_tmp);

                    for (auto start_edge : faces_processed) {
                        if (start_edge >= edges_processed.size()) {
                            continue;
                        }
                        
                        int edge = edges_processed[start_edge].next;
                        int face_vertex_count = 0;
                        while (edge != start_edge && face_vertex_count < 100) { // Prevent infinite loops
                            if (edge >= edges_processed.size()) {
                                break;
                            }
                            
                            int nextEdge = edges_processed[edge].next;
                            if (nextEdge >= edges_processed.size()) {
                                break;
                            }
                            
                            if (edges_processed[start_edge].origin < vertices.size() &&
                                edges_processed[edge].origin < vertices.size() &&
                                edges_processed[nextEdge].origin < vertices.size()) {
                                triangles.push_back(
                                    {
                                        vertices[edges_processed[start_edge].origin],
                                        vertices[edges_processed[edge].origin],
                                        vertices[edges_processed[nextEdge].origin]
                                    }
                                );
                            }
                            edge = nextEdge;
                            face_vertex_count++;
                        }
                    }
                    vector<uint8_t>().swap(faces_processed);
                    vector<Edge>().swap(edges_processed);
                    vector<Vector3>().swap(vertices);

                    count_hulls++;
                }
            }
            else {
                cout << endl << "Hulls: " << index << " (Total)" << endl;
                cout << endl << "Found " << count_hulls << " hulls with valid collision attributes" << endl;
                break;
            }
            index++;
        }

        //reset index and check meshes
        index = 0;
        while (true) {
            string index_str = to_string(index);
            string collision_index_str = parser.get_value("m_parts[0].m_rnShape.m_meshes[" + index_str + "].m_nCollisionAttributeIndex");
            if (collision_index_str != "") {
                int collision_index = atoi(collision_index_str.c_str());
                if (std::find(collision_attribute_indices.begin(), collision_attribute_indices.end(), collision_index) != collision_attribute_indices.end()) {
                    string triangles_str = parser.get_value("m_parts[0].m_rnShape.m_meshes[" + index_str + "].m_Mesh.m_Triangles");
                    string vertices_str = parser.get_value("m_parts[0].m_rnShape.m_meshes[" + index_str + "].m_Mesh.m_Vertices");
                    
                    if (triangles_str.empty() || vertices_str.empty()) {
                        index++;
                        continue;
                    }
                    
                    vector<int> triangle_processed = bytes_to_vec<int>(triangles_str);
                    vector<float> vertex_processed = bytes_to_vec<float>(vertices_str);

                    if (triangle_processed.empty() || vertex_processed.empty()) {
                        index++;
                        continue;
                    }

                    vector<Vector3> vertices;
                    for (int i = 0; i < vertex_processed.size(); i += 3) {
                        vertices.push_back({ vertex_processed[i], vertex_processed[i + 1], vertex_processed[i + 2] });
                    }
                    vector<float>().swap(vertex_processed);

                    for (int i = 0; i < triangle_processed.size(); i += 3) {
                        if (triangle_processed[i] < vertices.size() &&
                            triangle_processed[i + 1] < vertices.size() &&
                            triangle_processed[i + 2] < vertices.size()) {
                            triangles.push_back({ 
                                vertices[triangle_processed[i]], 
                                vertices[triangle_processed[i + 1]], 
                                vertices[triangle_processed[i + 2]] 
                            });
                        }
                    }

                    vector<int>().swap(triangle_processed);
                    vector<Vector3>().swap(vertices);

                    count_meshes++;
                }
            }
            else {
                cout << endl << "Meshes: " << index << " (Total)" << endl;
                cout << endl << "Found " << count_meshes << " meshes with valid collision attributes" << endl;
                break;
            }
            index++;
        }

        cout << "Total triangles found: " << triangles.size() << endl;
        
        if (triangles.size() > 0) {
            ofstream out(export_file_name, ios::out | ios::binary);
            if (out.is_open()) {
                out.write(reinterpret_cast<const char*>(triangles.data()), triangles.size() * sizeof(Triangle));
                out.close();
                cout << "Processed file: " << file_name << " -> " << export_file_name << endl;
            } else {
                cout << "Error: Could not open output file " << export_file_name << endl;
            }
        } else {
            cout << "No triangles found, skipping file write" << endl;
        }

        // Clear variables for next iteration
        triangles.clear();
    }

    return 0;
}
