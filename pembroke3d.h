#define PEMBROKE_IMPLEMENTATION
#include "pembroke.h"

typedef struct {
    float x, y, z;
} vec3;

typedef enum {
    UNION,
    INTERCEPT,
    S_UNION,
    S_INTERCEPT,
    SPHERE,
    BOX,
} sdf_type;

typedef struct {
    void* params;
    sdf_type type;
    sdf_type op;
    vec3 pos;
    color col;
} sdf_primitive;

typedef struct {
    sdf_primitive* objs;
    int count;
    vec3 cam_pos;
} render_data;

extern float S_UNION_K;

#if defined(PEMBROKE3D_IMPLEMENTATION)

float S_UNION_K = 0.5f;

static inline vec3 v3(float x, float y, float z){
    return (vec3){x, y, z};
}

static inline float length(vec3 v){
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

static inline vec3 sub(vec3 a, vec3 b){
    return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline vec3 add(vec3 a, vec3 b){
    return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline vec3 scale(vec3 a, float s){
    return (vec3){a.x * s, a.y * s, a.z * s};
}

static inline float dot(vec3 a, vec3 b){
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static inline vec3 normalize(vec3 v){
    float len = length(v);
    if(len < 1e-6f) return v3(0,0,0);
    return scale(v, 1.0f / len);
}

static inline vec3 reflect(vec3 I, vec3 N){
    return sub(I, scale(N, 2.0f * dot(I, N)));
}

static inline color color_lerp(color a, color b, float t){
    return (color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t)
    };
}

static float sdf(sdf_primitive a, vec3 p){
    vec3 local = sub(p, a.pos);
    switch(a.type){
        case SPHERE:{
            float radius = *(float*)a.params;
            return length(local) - radius;
        }
        case BOX:{
            float* s = (float*)a.params;

            vec3 b = v3(s[0], s[1], s[2]); // half extents

            vec3 q = v3(
                fabsf(local.x) - b.x,
                fabsf(local.y) - b.y,
                fabsf(local.z) - b.z
            );

            vec3 max_q = v3(
                fmaxf(q.x, 0.0f),
                fmaxf(q.y, 0.0f),
                fmaxf(q.z, 0.0f)
            );

            float outside = length(max_q);

            float inside = fminf(
                fmaxf(q.x, fmaxf(q.y, q.z)),
                0.0f
            );

            return outside + inside;
        }
    }
    return 0.0f;
}

static float smin(float a, float b, float k){
    float h = fmaxf(k - fabsf(a - b), 0.0f) / k;
    return fminf(a, b) - h*h*h * k * (1.0f/6.0f);
}

static float sdf_scene_color(vec3 pos, sdf_primitive objs[], int count, color* out_col){
    if(count == 0) return 1e15f;

    float d = sdf(objs[0], pos);
    *out_col = objs[0].col;

    for(int i = 1; i < count; i++){
        float d2 = sdf(objs[i], pos);

        switch(objs[i].op){
            case UNION:
                if(d2 < d){
                    d = d2;
                    *out_col = objs[i].col;
                }
                break;

            case S_UNION:{
                float k = S_UNION_K;

                float h = fmaxf(k - fabsf(d - d2), 0.0f) / k;
                float m = h*h*h * (1.0f / 6.0f);

                float d_new = fminf(d, d2) - m * k;

                // blend factor (this is the magic)
                float t = 0.5f + 0.5f * (d2 - d) / k;
                t = fmaxf(0.0f, fminf(1.0f, t));

                color blended = color_lerp(*out_col, objs[i].col, t);

                d = d_new;
                *out_col = blended;
                break;
            }

            case INTERCEPT:
                d = fmaxf(d, d2);
                break;
        }
    }

    return d;
}

static float sdf_scene(vec3 pos, sdf_primitive objs[], int count, int* hit_id){
    if(count == 0) return 1e15f;

    float ret = sdf(objs[0], pos);
    *hit_id = 0;

    for(int i = 1; i < count; i++){
        float d = sdf(objs[i], pos);

        switch(objs[i].op){
            case UNION:
                if(d < ret){
                    ret = d;
                    *hit_id = i;
                }
                break;

            case INTERCEPT:
                ret = fmaxf(ret, d);
                break;

            case S_UNION:
                ret = smin(ret, d, S_UNION_K);
                break;
        }
    }

    return ret;
}

static float sdf_scene_dist(vec3 pos, sdf_primitive objs[], int count){
    if(count == 0) return 1e15f;

    float ret = sdf(objs[0], pos);

    for(int i = 1; i < count; i++){
        float d = sdf(objs[i], pos);

        switch(objs[i].op){
            case UNION:
                ret = fminf(ret, d);
                break;

            case INTERCEPT:
                ret = fmaxf(ret, d);
                break;

            case S_UNION:
                ret = smin(ret, d, S_UNION_K);
                break;
        }
    }

    return ret;
}

static vec3 get_normal(vec3 p, sdf_primitive objs[], int count){
    float e = 0.001f;

    float dx = sdf_scene_dist(add(p, v3(e,0,0)), objs, count)
             - sdf_scene_dist(add(p, v3(-e,0,0)), objs, count);

    float dy = sdf_scene_dist(add(p, v3(0,e,0)), objs, count)
             - sdf_scene_dist(add(p, v3(0,-e,0)), objs, count);

    float dz = sdf_scene_dist(add(p, v3(0,0,e)), objs, count)
             - sdf_scene_dist(add(p, v3(0,0,-e)), objs, count);

    return normalize(v3(dx, dy, dz));
}

static int get_hit_id(vec3 p, sdf_primitive objs[], int count){
    float best = 1e15f;
    int id = -1;

    for(int i = 0; i < count; i++){
        float d = sdf(objs[i], p);
        if(d < best){
            best = d;
            id = i;
        }
    }

    return id;
}

float raymarch(vec3 ro, vec3 rd, sdf_primitive objs[], int count, color* out_col){
    float t = 0.0f;

    for(int i = 0; i < 100; i++){
        vec3 p = add(ro, scale(rd, t));

        float d = sdf_scene_color(p, objs, count, out_col);

        if(d < 0.001f){
            return t;
        }

        t += d;

        if(t > 100.0f){
            break;
        }
    }

    return -1.0f;
}

static float soft_shadow(vec3 ro, vec3 rd, sdf_primitive objs[], int count){
    float t = 0.02f;
    float res = 1.0f;

    for(int i = 0; i < 50; i++){
        vec3 p = add(ro, scale(rd, t));
        float d = sdf_scene_dist(p, objs, count);

        if(d < 0.001f){
            return 0.0f;
        }

        res = fminf(res, 10.0f * d / t);

        t += d;

        if(t > 20.0f){
            break;
        }
    }

    return res;
}

static float ambient_occlusion(vec3 p, vec3 n, sdf_primitive objs[], int count){
    float occlusion = 0.0f;
    float scalef = 1.0f;

    for(int i = 1; i <= 5; i++){
        float t = 0.02f * i;

        float d = sdf_scene_dist(add(p, scale(n, t)), objs, count);

        occlusion += (t - d) * scalef;
        scalef *= 0.5f;
    }

    float ao = 1.0f - occlusion;
    if(ao < 0.0f) ao = 0.0f;
    if(ao > 1.0f) ao = 1.0f;

    return ao;
}

static void ray(int x, int y, color* rgb, void* userdata){
    render_data* data = (render_data*)userdata;

    float u = (2.0f * x / VIDEO_WIDTH  - 1.0f);
    float v = (2.0f * y / VIDEO_HEIGHT - 1.0f);

    u *= (float)VIDEO_WIDTH / VIDEO_HEIGHT;

    vec3 ro = data->cam_pos;

    float fov = 1.5f;
    vec3 rd = normalize(v3(u, -v, fov));

    float refl_strength = 0.4f;
    color refl_color;
    float t = raymarch(ro, rd, data->objs, data->count, &refl_color);

    if(t > 0.0f){
        vec3 p = add(ro, scale(rd, t));
        vec3 n = get_normal(p, data->objs, data->count);

        vec3 view_dir = normalize(scale(rd, -1));

        vec3 light_dir = normalize(v3(0,0,-1));
        float diff = fmaxf(0.0f, dot(n, light_dir));
        float shadow = soft_shadow(add(p, scale(n, 0.01f)), light_dir, data->objs, data->count);

        float ambient = 0.4f;
        float light = ambient + (1.0f - ambient) * diff * shadow;
        light = fmaxf(0.0f, fminf(1.0f, light));

        color base = refl_color;

        vec3 refl_dir = normalize(reflect(rd, n));
        vec3 refl_origin = add(p, scale(n, 0.02f));

        color refl_col;
        float t2 = raymarch(refl_origin, refl_dir, data->objs, data->count, &refl_col);

        if(t2 < 0.0f){
            float sky = (refl_dir.y + 1.0f) * 0.5f;
            refl_col = (color){
                (unsigned char)(sky * 200),
                (unsigned char)(sky * 200),
                255
            };
        }

        float fresnel = powf(1.0f - fmaxf(0.0f, dot(n, view_dir)), 3.0f);

        float refl_strength = 0.5f;

        *rgb = (color){
            (unsigned char)(base.r * light * (1.0f - fresnel * refl_strength) + refl_col.r * fresnel * refl_strength),
            (unsigned char)(base.g * light * (1.0f - fresnel * refl_strength) + refl_col.g * fresnel * refl_strength),
            (unsigned char)(base.b * light * (1.0f - fresnel * refl_strength) + refl_col.b * fresnel * refl_strength)
        };
    } else {
        unsigned char sky = (unsigned char)((v + 1.0f) * 127);
        *rgb = (color){sky, sky, 255};
    }
}

static void render_3d_scene(render_data* data){
    foreach_pixel(ray, data);
}

#endif