#version 150
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_explicit_attrib_location : require

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normals;
layout(location = 2) in vec2 in_texCoord;
//layout(location = 0) in vec3 in_position;
//layout(location = 1) in vec3 in_normal;
//layout(location = 2) in vec2 in_texcoord;
uniform mat4 Projection;
uniform mat4 Model;
uniform mat4 Modelview;
uniform mat4 Modelview_inv;
uniform vec3 obj_to_tex;

out per_vertex{
    smooth vec4 ray_exit_os;
    smooth vec4 ray_exit_vs;
    smooth vec4 ray_exit_ts;
} vertex_out;

out float camera_distance;

void main()
{
    /*vertex_out.ray_entry_os = vec4(in_position, 1.0);
    vertex_out.ray_entry_ts = vec4(in_position, 1.0) * vec4(volume_data.scale_obj_to_tex.xyz, 1.0);
    gl_Position = volume_data.mvp_matrix * vec4(in_position, 1.0);*/    
    vertex_out.ray_exit_os = vec4(in_position, 1.0);
    vertex_out.ray_exit_vs = Modelview * vec4(in_position, 1.0);    
    vertex_out.ray_exit_ts = vec4(in_position, 1.0) * vec4(obj_to_tex, 1.0);

    camera_distance = length(vertex_out.ray_exit_vs.xyz);

    gl_Position = Projection * Modelview * vec4(in_position, 1.0);
    
}
