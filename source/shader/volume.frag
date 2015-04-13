#version 430
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_bindless_texture : require

#define TASK 21  // 21 22 31 32 33 4 5
#define ENABLE_OPACITY_CORRECTION 0
#define ENABLE_LIGHTNING 0
#define ENABLE_SHADOWING 0

const float epsilon = 0.00001;

in per_vertex{
    smooth vec4 ray_exit_os;
    smooth vec4 ray_exit_vs;
    smooth vec4 ray_exit_ts;
} v_in;
in float camera_distance;

layout(location = 0) out vec4 FragColor;


layout(std430, binding = 2) buffer volume_data
{
    uvec2 volume_handles[];
};


layout(std430, binding = 3) buffer transfer_data
{    
    uvec2 texture_handles[];    
    
};

layout(std430, binding = 4) buffer transfer_used_data
{
    uint channel_used[];
};

layout(std430, binding = 5) buffer channel_range_data
{
    vec2 channel_range[];
};

layout(std430, binding = 6) buffer color_write_buffer
{
    vec4 color_interpolation_compositing[];
};

uniform uint volume_handles_count;
uniform uint channel_count;
uniform uint time_step_count;

uniform mat4 Modelview_inv;

uniform sampler3D volume_texture;
uniform sampler2D transfer_texture;

uniform uvec2   volume_texture_bindless_handle;
uniform uvec2   next_volume_texture_bindless_handle;

uniform float   time_position; //0.0..1.0

uniform vec3    camera_location;
uniform float   sampling_distance;
uniform float   sampling_distance_ref;
uniform float   iso_value;
uniform vec3    obj_to_tex;
uniform ivec3   volume_dimensions;
uniform vec4    bbmin_view;
uniform vec4    bbmax_view;
uniform vec4    camera_os;


uniform vec3    light_position;
uniform vec3    light_ambient_color;
uniform vec3    light_diffuse_color;
uniform vec3    light_specular_color;
uniform float   light_ref_coef;

struct ray
{
    vec3    origin;
    vec3    direction;
    vec3    direction_rec;
}; // struct ray

ray
make_safe_ray(in vec3  entry,
            in vec3  dir)
{
    ray gen_ray;
    gen_ray.origin = entry;
    gen_ray.direction = dir;

    vec3 ray_sign = sign(gen_ray.direction);

    if (abs(dir.x) < epsilon) gen_ray.direction.x = ray_sign.x < 0.0 ? -epsilon : epsilon; // sign can return 0.0
    if (abs(dir.y) < epsilon) gen_ray.direction.y = ray_sign.y < 0.0 ? -epsilon : epsilon; // sign can return 0.0
    if (abs(dir.z) < epsilon) gen_ray.direction.z = ray_sign.z < 0.0 ? -epsilon : epsilon; // sign can return 0.0

    gen_ray.direction_rec = vec3(1.0) / gen_ray.direction;

    return gen_ray;
}

/* http://ompf.org/ray/ray_box.html */
//bool_t ray_box_intersects_slabs_geimer_mueller(const aabb_t &box, const ray_t &ray) {
bool
ray_box_intersection(in ray    r,
in vec3   bbmin,
in vec3   bbmax,
out float tmin,
out float tmax)
{
    float l1 = (bbmin.x - r.origin.x) * r.direction_rec.x;
    float l2 = (bbmax.x - r.origin.x) * r.direction_rec.x;
    tmin = min(l1, l2);
    tmax = max(l1, l2);

    l1 = (bbmin.y - r.origin.y) * r.direction_rec.y;
    l2 = (bbmax.y - r.origin.y) * r.direction_rec.y;
    tmin = max(min(l1, l2), tmin);
    tmax = min(max(l1, l2), tmax);

    l1 = (bbmin.z - r.origin.z) * r.direction_rec.z;
    l2 = (bbmax.z - r.origin.z) * r.direction_rec.z;
    tmin = max(min(l1, l2), tmin);
    tmax = min(max(l1, l2), tmax);

    //return ((lmax > 0.f) & (lmax >= lmin));
    //return ((lmax > 0.f) & (lmax > lmin));
    return ((tmin > 0.0) && (tmax > tmin));
}

vec4 shade_sample(in vec3 spos, in vec3 grad, in vec4 mat_color, float shadow_mult = 1.0f);

bool
inside_volume_bounds(const in vec3 sampling_position)
{
    return (   all(greaterThanEqual(sampling_position, vec3(0.0)))
        && all(lessThanEqual(sampling_position, vec3(bbmax_view))));
}

float
get_sample_data(vec3 in_sampling_pos, uint in_channel, uint time_pos){

    uint handle_position_in_ssbo = time_pos + in_channel * time_step_count;
        
    float v = texture(sampler3D(volume_handles[handle_position_in_ssbo]), in_sampling_pos).r;
    
    float range = channel_range[in_channel].y - channel_range[in_channel].x;
    v = (v - channel_range[in_channel].x) / range;

    return v;
}


float
get_raw_data(vec3 in_sampling_pos, uint in_channel){

    //vec3 obj_to_tex = vec3(1.0) / max_bounds;
    //return texture(volume_texture, in_sampling_pos * obj_to_tex).r;
    uint uint_time_pos = uint(time_position);
    float mix_time = time_position - float(uint_time_pos);

    if (time_step_count > 1){
        float v1 = get_sample_data(in_sampling_pos * obj_to_tex, in_channel, uint_time_pos).r;
        float v2 = get_sample_data(in_sampling_pos * obj_to_tex, in_channel, uint_time_pos + 1u).r;

        float vm = v1 * (1.0 - mix_time) + v2 * mix_time;

        return vm;
    }
    else{
        float v = get_sample_data(in_sampling_pos * obj_to_tex, in_channel, uint_time_pos).r;        
        return v;
    }
}

vec4
get_color_data(vec3 in_sampling_pos, uint in_channel){

    //vec3 obj_to_tex = vec3(1.0) / max_bounds;
    //return texture(volume_texture, in_sampling_pos * obj_to_tex).r;
    uint uint_time_pos = uint(time_position);
    float mix_time = time_position - float(uint_time_pos);

    if (time_step_count > 1){
        float v1 = get_sample_data(in_sampling_pos * obj_to_tex, in_channel, uint_time_pos).r;
        float v2 = get_sample_data(in_sampling_pos * obj_to_tex, in_channel, uint_time_pos + 1u).r;

        float vm = v1 * (1.0 - mix_time) + v2 * mix_time;

        vec4 c1 = texture(sampler2D(texture_handles[in_channel]), vec2(v1, v1));
        vec4 c2 = texture(sampler2D(texture_handles[in_channel]), vec2(v2, v2));

        vec4 cm;
        //cm = c1 * (1.0 - mix_time) + c2 * mix_time;
        cm = texture(sampler2D(texture_handles[in_channel]), vec2(vm, vm));
        
        //return vec4(mix_time, 0.0, 0.0, 1.0);
        return cm;
    }
    else{
        float v = get_sample_data(in_sampling_pos * obj_to_tex, in_channel, uint_time_pos).r;
        vec4 c = texture(sampler2D(texture_handles[in_channel]), vec2(v, 0.5));
        return c;
    }
}

vec4
get_color_data_single(vec3 in_sampling_pos, uint in_channel, uint uint_time_pos){

    float v = get_sample_data(in_sampling_pos * obj_to_tex, in_channel, uint_time_pos).r;
    vec4 c = texture(sampler2D(texture_handles[in_channel]), vec2(v, 0.5));
    return c;
}


vec3
get_gradient(vec3 in_sampling_pos, vec3 step_size, uint time_step){

    uint chan = 0;

    float nx = get_sample_data(in_sampling_pos - vec3(step_size.x, 0.0, 0.0), chan, time_step);
    float px = get_sample_data(in_sampling_pos + vec3(step_size.x, 0.0, 0.0), chan, time_step);
                                                                            
    float ny = get_sample_data(in_sampling_pos - vec3(0.0, step_size.z, 0.0), chan, time_step);
    float py = get_sample_data(in_sampling_pos + vec3(0.0, step_size.z, 0.0), chan, time_step);
                                                                            
    float nz = get_sample_data(in_sampling_pos - vec3(0.0, 0.0, step_size.z), chan, time_step);
    float pz = get_sample_data(in_sampling_pos + vec3(0.0, 0.0, step_size.z), chan, time_step);

    float grad_x = px - nx;
    float grad_y = py - ny;
    float grad_z = pz - nz;

    return(vec3(grad_x, grad_y, grad_z) * 0.5);

}



void main()
{       
    ray view_ray = make_safe_ray(camera_os.xyz, normalize(v_in.ray_exit_os.xyz - camera_os.xyz));
    view_ray.origin = camera_os.xyz;
    view_ray.direction = normalize(v_in.ray_exit_os.xyz - camera_os.xyz);
    view_ray.direction_rec = 1.0 / view_ray.direction;
    float t_entry = -1.0;
    float t_exit = -1.0;

    ray_box_intersection(view_ray, bbmin_view.xyz, bbmax_view.xyz, t_entry, t_exit);
    if (t_entry < 0.0)
        t_entry = t_exit - length(v_in.ray_exit_vs.xyz);

    /// One step trough the volume
    vec3 ray_increment = view_ray.direction * sampling_distance;
    /// Position in Volume
    vec3 sampling_pos = view_ray.origin + view_ray.direction * t_entry + ray_increment; // increment just to be sure we are in the volume

    /// Init color of fragment
    vec4 dst = vec4(0.0, 0.0, 0.0, 0.0);

    /// check if we are inside volume
    bool inside_volume = inside_volume_bounds(sampling_pos);

    float max_s = 0.0;
    vec4 max_val = vec4(0.0, 0.0, 0.0, 0.0);


    // the traversal loop,
    // termination when the sampling position is outside volume boundarys
    // another termination condition for early ray termination is added
    while (inside_volume)
    {
        float s = 0.0;
        vec4 color = vec4(0.0);
        
        // get sample
        s = get_raw_data(sampling_pos, 0);

        // apply the transfer functions to retrieve color and opacity
        color = get_color_data_single(sampling_pos, 0, uint(time_position));
                
        // this is the example for maximum intensity projection
        max_s = max(s, max_s);
        // this is the example for maximum intensity projection
        max_val = max(color, max_val);

        // increment the ray sampling position
        sampling_pos += ray_increment;

        // update the loop termination condition
        inside_volume = inside_volume_bounds(sampling_pos);
    }
    
    dst = max_val;
    

    // return the calculated color value
    FragColor = dst;
}
