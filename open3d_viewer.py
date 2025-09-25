#!/usr/bin/env python3
"""
Open3D High-Performance Viewer - GPU-accelerated 3D visualization for large meshes.
Designed for smooth navigation with 960k+ triangles and LOS point visualization.
"""

import numpy as np
import open3d as o3d
import pandas as pd
import struct
import os
import sys
import time

def load_tri_file(file_path):
    """Load .tri file and return vertices and faces."""
    print(f"Loading .tri file: {file_path}")
    
    with open(file_path, 'rb') as f:
        data = f.read()
    
    file_size = len(data)
    triangle_size = 36  # 3 Vector3 * 3 floats * 4 bytes
    num_triangles = file_size // triangle_size
    
    print(f"Loading {num_triangles:,} triangles...")
    
    vertices = []
    faces = []
    vertex_index = 0
    
    for i in range(num_triangles):
        offset = i * triangle_size
        try:
            coords = struct.unpack('9f', data[offset:offset + triangle_size])
            
            p1 = np.array([coords[0], coords[1], coords[2]])
            p2 = np.array([coords[3], coords[4], coords[5]])
            p3 = np.array([coords[6], coords[7], coords[8]])
            
            vertices.extend([p1, p2, p3])
            faces.append([vertex_index, vertex_index + 1, vertex_index + 2])
            vertex_index += 3
            
        except struct.error:
            break
    
    return np.array(vertices), np.array(faces)

def load_los_tests(csv_file):
    """Load LOS test data from CSV file."""
    print(f"Loading LOS tests from: {csv_file}")
    
    df = pd.read_csv(csv_file)
    print(f"Loaded {len(df)} LOS tests")
    
    return df

def create_open3d_mesh(vertices, faces):
    """Create Open3D mesh from vertices and faces."""
    print("Creating Open3D mesh...")
    
    # Create Open3D mesh
    mesh = o3d.geometry.TriangleMesh()
    mesh.vertices = o3d.utility.Vector3dVector(vertices)
    mesh.triangles = o3d.utility.Vector3iVector(faces)
    
    # Compute normals for better lighting
    mesh.compute_vertex_normals()
    
    # Set mesh color (light gray)
    mesh.paint_uniform_color([0.8, 0.8, 0.8])
    
    print(f"Mesh created with {len(mesh.vertices)} vertices and {len(mesh.triangles)} triangles")
    
    return mesh

def create_los_visualization(vertices, faces, los_df):
    """Create LOS visualization with points and lines."""
    print("Creating LOS visualization...")
    
    geometries = []
    
    # Create the main mesh
    mesh = create_open3d_mesh(vertices, faces)
    geometries.append(mesh)
    
    # Add LOS test points and lines
    for idx, row in los_df.iterrows():
        # Extract positions
        p1_pos = np.array([row['player 1 x'], row['player 1 y'], row['player 1 z']])
        p2_pos = np.array([row['player 2 x'], row['player 2 y'], row['player 2 z']])
        
        # Create spheres for player positions
        sphere1 = o3d.geometry.TriangleMesh.create_sphere(radius=15)
        sphere1.translate(p1_pos)
        
        sphere2 = o3d.geometry.TriangleMesh.create_sphere(radius=15)
        sphere2.translate(p2_pos)
        
        # Color spheres based on LOS result
        if row['los'] == 'TRUE':
            color = [0, 1, 0]  # Green for visible
            line_color = [0, 1, 0]
        else:
            color = [1, 0, 0]  # Red for blocked
            line_color = [1, 0, 0]
        
        sphere1.paint_uniform_color(color)
        sphere2.paint_uniform_color(color)
        
        # Create line between positions
        line_points = np.array([p1_pos, p2_pos])
        line = o3d.geometry.LineSet()
        line.points = o3d.utility.Vector3dVector(line_points)
        line.lines = o3d.utility.Vector2iVector([[0, 1]])
        line.colors = o3d.utility.Vector3dVector([line_color])
        
        # Add to geometries
        geometries.extend([sphere1, sphere2, line])
        
        print(f"Added LOS test {idx+1}: {row['Description']} ({'Visible' if row['los'] == 'TRUE' else 'Blocked'})")
    
    return geometries

def visualize_with_open3d(geometries, file_name):
    """Visualize using Open3D's high-performance viewer."""
    print(f"Opening Open3D viewer for {file_name}...")
    print("Controls:")
    print("  - Mouse: Rotate view")
    print("  - Mouse wheel: Zoom in/out")
    print("  - Right-click drag: Pan view")
    print("  - WASD: Move around (if enabled)")
    print("  - R: Reset view")
    print("  - F: Fit view to all geometry")
    print("  - ESC: Exit viewer")
    print("\nLegend:")
    print("  - Gray mesh: Map geometry")
    print("  - Green spheres/lines: Visible LOS")
    print("  - Red spheres/lines: Blocked LOS")
    
    # Create visualizer
    vis = o3d.visualization.Visualizer()
    vis.create_window(window_name=f"CS2 Map Viewer - {file_name}", width=1200, height=800)
    
    # Add all geometries
    for geometry in geometries:
        vis.add_geometry(geometry)
    
    # Set up the view
    vis.get_render_option().background_color = np.array([0.1, 0.1, 0.1])  # Dark background
    vis.get_render_option().mesh_show_back_face = True
    vis.get_render_option().point_size = 3.0
    vis.get_render_option().line_width = 2.0
    
    # Enable lighting
    vis.get_render_option().light_on = True
    
    # Fit view to show all geometry
    vis.get_view_control().set_front([0, 0, -1])
    vis.get_view_control().set_lookat([0, 0, 0])
    vis.get_view_control().set_up([0, 1, 0])
    vis.get_view_control().set_zoom(0.8)
    
    print("Viewer opened successfully!")
    print("Use mouse to navigate, ESC to exit")
    
    # Run the visualizer
    vis.run()
    vis.destroy_window()

def main():
    """Main function to handle command line arguments and visualization."""
    
    if len(sys.argv) < 2:
        print("Usage: python3 open3d_viewer.py <tri_file> [csv_file]")
        print("Examples:")
        print("  python3 open3d_viewer.py output/de_ancient.tri")
        print("  python3 open3d_viewer.py output/de_ancient.tri los-tests.csv")
        return
    
    tri_file = sys.argv[1]
    csv_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    if not os.path.exists(tri_file):
        print(f"Error: .tri file not found: {tri_file}")
        return
    
    if csv_file and not os.path.exists(csv_file):
        print(f"Error: CSV file not found: {csv_file}")
        return
    
    try:
        print(f"\n{'='*60}")
        print(f"Open3D High-Performance Viewer - {os.path.basename(tri_file)}")
        print(f"{'='*60}")
        
        # Load map mesh
        vertices, faces = load_tri_file(tri_file)
        
        if len(vertices) == 0:
            print("No vertices found in file!")
            return
        
        if csv_file:
            # Load LOS tests and create visualization
            los_df = load_los_tests(csv_file)
            geometries = create_los_visualization(vertices, faces, los_df)
        else:
            # Just show the map
            mesh = create_open3d_mesh(vertices, faces)
            geometries = [mesh]
        
        # Visualize with Open3D
        visualize_with_open3d(geometries, os.path.basename(tri_file))
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
