#version 150
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_bindless_texture : require

#define TASK 21  // 21 22 31 32 33 4 5
#define ENABLE_OPACITY_CORRECTION 0
#define ENABLE_LIGHTNING 0
#define ENABLE_SHADOWING 0

in vec3 ray_entry_position;

layout(location = 0) out vec4 FragColor;

uniform mat4 Modelview;

uniform sampler3D volume_texture;
uniform sampler2D transfer_texture;

uniform uvec2   volume_texture_bindless_handle;

uniform vec3    camera_location;
uniform float   sampling_distance;
uniform float   sampling_distance_ref;
uniform float   iso_value;
uniform vec3    max_bounds;
uniform ivec3   volume_dimensions;

uniform vec3    light_position;
uniform vec3    light_ambient_color;
uniform vec3    light_diffuse_color;
uniform vec3    light_specular_color;
uniform float   light_ref_coef;

vec4 shade_sample(in vec3 spos, in vec3 grad, in vec4 mat_color, float shadow_mult = 1.0f);

bool
inside_volume_bounds(const in vec3 sampling_position)
{
    return (   all(greaterThanEqual(sampling_position, vec3(0.0)))
            && all(lessThanEqual(sampling_position, max_bounds)));
}

float
get_sample_data(vec3 in_sampling_pos){

    vec3 obj_to_tex = vec3(1.0) / max_bounds;
    //return texture(volume_texture, in_sampling_pos * obj_to_tex).r;

    return texture(sampler3D(volume_texture_bindless_handle), in_sampling_pos * obj_to_tex).r;

}

vec3
get_gradient(vec3 in_sampling_pos, vec3 step_size){

    float nx = get_sample_data(in_sampling_pos - vec3(step_size.x, 0.0, 0.0));
    float px = get_sample_data(in_sampling_pos + vec3(step_size.x, 0.0, 0.0));

    float ny = get_sample_data(in_sampling_pos - vec3(0.0, step_size.z, 0.0));
    float py = get_sample_data(in_sampling_pos + vec3(0.0, step_size.z, 0.0));

    float nz = get_sample_data(in_sampling_pos - vec3(0.0, 0.0, step_size.z));
    float pz = get_sample_data(in_sampling_pos + vec3(0.0, 0.0, step_size.z));

    float grad_x = px - nx;
    float grad_y = py - ny;
    float grad_z = pz - nz;

    return(vec3(grad_x, grad_y, grad_z) * 0.5);

}

vec4
shade_sample(in vec3 spos, in vec3 grad, in vec4 mat_color, float shadow_mult)
{



    vec3 color = vec3(0.0, 0.0, 0.0);

    if (length(grad) >= 0.01){
        vec3 lt = light_position;

        vec3 n = normalize(grad);
        vec3 l = normalize(lt - spos);
        vec3 r = reflect(-l, n);

        vec3 v = normalize(spos-camera_location);

        float lambertian = max(dot(l, n), 0.0);

        float specular = 0.0;

        if (lambertian > 0.0) {
            float specAngle = max(dot(r, v), 0.0);
            specular = pow(specAngle, light_ref_coef);
        }
        color.rgb = mat_color.rgb * light_ambient_color +
                mat_color.rgb *lambertian*light_diffuse_color +
                specular*light_specular_color;

        
//            color.r = lambertian;
    }
    else{
        color.rgb = mat_color.rgb * light_ambient_color;
    }

    color *= shadow_mult;

    return vec4(color, mat_color.a);
}

vec3
get_gradient_alpha_transfer(vec3 in_sampling_pos, vec3 step_size){

    float nx = get_sample_data(in_sampling_pos - vec3(step_size.x, 0.0, 0.0));
    float px = get_sample_data(in_sampling_pos + vec3(step_size.x, 0.0, 0.0));

    float ny = get_sample_data(in_sampling_pos - vec3(0.0, step_size.z, 0.0));
    float py = get_sample_data(in_sampling_pos + vec3(0.0, step_size.z, 0.0));

    float nz = get_sample_data(in_sampling_pos - vec3(0.0, 0.0, step_size.z));
    float pz = get_sample_data(in_sampling_pos + vec3(0.0, 0.0, step_size.z));

    float dnx = texture(transfer_texture, vec2(nx, 0.5)).a;
    float dpx = texture(transfer_texture, vec2(px, 0.5)).a;

    float dny = texture(transfer_texture, vec2(ny, 0.5)).a;
    float dpy = texture(transfer_texture, vec2(py, 0.5)).a;

    float dnz = texture(transfer_texture, vec2(nz, 0.5)).a;
    float dpz = texture(transfer_texture, vec2(pz, 0.5)).a;

    float grad_x = dpx - dnx;
    float grad_y = dpy - dny;
    float grad_z = dpz - dnz;

    return(vec3(grad_x, grad_y, grad_z) * 0.5);

}

bool
shadow(vec3 spos){
    
    
    vec3 lt = light_position;
    vec3 l = -normalize(lt - spos);
    
    vec3 ray_light_increment = l * sampling_distance * 2.0;
    vec3 sampling_pos = spos;
    sampling_pos += ray_light_increment * 3.0;

    float last_data_sample = get_sample_data(sampling_pos);
    float trav_sign = sign(last_data_sample - iso_value);
        
    bool inside_volume = true;

    while (inside_volume) {

        float cur_data = get_sample_data(sampling_pos);
        float cur_trav_sign = sign(cur_data - iso_value);

        if (cur_trav_sign != trav_sign) {
            return true;
        }

        // increment ray
        sampling_pos += ray_light_increment;
        inside_volume = inside_volume_bounds(sampling_pos);

        last_data_sample = cur_data;
    }

    return false;
}


void main()
{
    /// One step trough the volume
    vec3 ray_increment      = normalize(ray_entry_position - camera_location) * sampling_distance;
    /// Position in Volume
    vec3 sampling_pos       = ray_entry_position + ray_increment; // test, increment just to be sure we are in the volume

    /// Init color of fragment
    vec4 dst = vec4(0.0, 0.0, 0.0, 0.0);

    /// check if we are inside volume
    bool inside_volume = inside_volume_bounds(sampling_pos);

#if TASK == 21
    vec4 max_val = vec4(0.0, 0.0, 0.0, 0.0);
  
    // the traversal loop,
    // termination when the sampling position is outside volume boundarys
    // another termination condition for early ray termination is added
    while (inside_volume) 
    {      
        // get sample
        float s = get_sample_data(sampling_pos);
                
        // apply the transfer functions to retrieve color and opacity
        vec4 color = texture(transfer_texture, vec2(s, s));
           
        // this is the example for maximum intensity projection
        max_val.r = max(color.r, max_val.r);
        max_val.g = max(color.g, max_val.g);
        max_val.b = max(color.b, max_val.b);
        max_val.a = max(color.a, max_val.a);
        
        // increment the ray sampling position
        sampling_pos  += ray_increment;

        // update the loop termination condition
        inside_volume  = inside_volume_bounds(sampling_pos);
    }

    dst = max_val;
#endif 
    
#if TASK == 22
    
    vec4 aver_val = vec4(0.0, 0.0, 0.0, 0.0);
    float aver_dens = 0.0;;
    int loops = 0;


    // the traversal loop,
    // termination when the sampling position is outside volume boundarys    
    while (inside_volume)
    {
        ++loops;

        // get sample
        float s = get_sample_data(sampling_pos);

        aver_dens += s;

        // apply the transfer functions to retrieve color and opacity
        vec4 color = texture(transfer_texture, vec2(s, 0.0));
        
        aver_val += color;

        // increment the ray sampling position
        sampling_pos += ray_increment;

        // update the loop termination condition
        inside_volume = inside_volume_bounds(sampling_pos);
    }

#if 1
    float dens = aver_dens / loops;

    dst = texture(transfer_texture, vec2(dens, 0.5));
#else
    dst = aver_val / loops;
#endif
#endif
   

#if TASK == 31 || TASK == 32 || TASK == 33  

    float last_data_sample = get_sample_data(sampling_pos);

    float trav_sign = sign(last_data_sample - iso_value);
    float cur_trav_sign = trav_sign;

    bool found_intersection = false;

    while (inside_volume && !found_intersection) {

        float cur_data = get_sample_data(sampling_pos);
        trav_sign = cur_trav_sign;
        float cur_trav_sign = sign(cur_data - iso_value);

        if (cur_trav_sign != trav_sign) {
            // ok a sign change has occured
            // we crossed the iso surface
            int step = 0;
            int steps = 256;
#if TASK == 32 // Binary Search


            while (length(ray_increment) > 0.0001){

                // first try:
                //  - interpolate the intrersection position
                //float u = (iso_value - cur_data) / (last_data_sample - cur_data);
                //vec3 iso_intersect = mix((sampling_pos - ray_increment), sampling_pos, u);

                float cur_data = get_sample_data(sampling_pos);
                cur_trav_sign = sign(cur_data - iso_value);


                if (trav_sign != cur_trav_sign){
                    ray_increment *= -1.0;
                }

                ray_increment *= 0.5;

                trav_sign = cur_trav_sign;

                sampling_pos += ray_increment;
            }

           
#endif
            //float u = (iso_value - cur_data) / (last_data_sample - cur_data);

            //vec3 iso_intersect = mix((sampling_pos - ray_increment), sampling_pos, u);
            vec3 iso_intersect = sampling_pos;// mix((sampling_pos - ray_increment), sampling_pos, u);

#if ENABLE_LIGHTNING == 1 // Add Shading
            float shadow_mult = 1.0;
#if ENABLE_SHADOWING == 1
            if (shadow(sampling_pos)){
                shadow_mult = 0.02;
            }
#endif

            vec3 gradient = get_gradient(iso_intersect, vec3(0.01, 0.01, 0.01));
            vec4 src = shade_sample(iso_intersect, gradient, vec4(1.0, 1.0, 1.0, 1.0), shadow_mult);

#else
            vec4 src = vec4(vec3(0.5), 1.0);
#endif            

            dst = src;
            found_intersection = true;
            if(step == steps)
                dst = vec4(1.0, 0.0, 0.0, 1.0);
        }
        // increment ray
        sampling_pos += ray_increment;
        inside_volume = inside_volume_bounds(sampling_pos);

        last_data_sample = cur_data;
    }

#endif 

#if TASK == 41


    // the traversal loop,
    // termination when the sampling position is outside volume boundarys
    // another termination condition for early ray termination is added
    while (inside_volume && dst.a < 0.95) {
        // get sample
        
        float s = get_sample_data(sampling_pos);
        vec4 src = texture(transfer_texture, vec2(s, 0.0));
#if ENABLE_OPACITY_CORRECTION == 1 // Opacity Correction
        const float alpha_corr = 1.0 - pow(1.0 - src.a, length(sampling_distance/sampling_distance_ref));
        src.a = alpha_corr;
#endif
        
#if ENABLE_LIGHTNING == 1 // Add Shading
        vec3 gradient = get_gradient_alpha_transfer(sampling_pos, vec3(0.01, 0.01, 0.01));
        //vec3 gradient = get_gradient(sampling_pos, vec3(0.01, 0.01, 0.01));
        src = shade_sample(sampling_pos, gradient, src, 1.0);
#endif

        // increment ray
        sampling_pos += ray_increment;
        inside_volume = inside_volume_bounds(sampling_pos);
        // compositing
        float omda_sa = (1.0 - dst.a) * src.a;
        dst.rgb += omda_sa*src.rgb;
        dst.a += omda_sa;
    }
#endif 

    // return the calculated color value
    FragColor = dst;
}
