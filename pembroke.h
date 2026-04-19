#pragma once

#include <omp.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define HAVE_SSE2 1
#else
#define HAVE_SSE2 0
#endif

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PI 3.14159265358979323846f

typedef struct {
    unsigned char r, g, b;
} color;

typedef void (*pixel_shader)(
    int x, int y,
    color* rgb,
    void* userdata
);

extern int VIDEO_WIDTH;
extern int VIDEO_HEIGHT;
extern int VIDEO_FPS;
extern float XMIN;
extern float XMAX;
extern float YMIN;
extern float YMAX;
extern unsigned char** VIDEO_FRAME;
extern float* MATRIX_STACK;
extern int MATRIX_STACK_COUNT;
extern int MATRIX_STACK_SIZE;
extern clock_t START_TIME;
extern clock_t ROLLING_TIME[128];
extern clock_t PREV_TIME;

#ifdef PEMBROKE_IMPLEMENTATION

int VIDEO_WIDTH = 640;
int VIDEO_HEIGHT = 480;
int VIDEO_FPS = 60;
unsigned char** VIDEO_FRAME = NULL;
float XMIN = -1.0f;
float XMAX = 1.0f;
float YMIN = -1.0f;
float YMAX = 1.0f;
int FRAME_COUNT = 0;
float* MATRIX_STACK = NULL;
int MATRIX_STACK_COUNT = 0;
int MATRIX_STACK_SIZE = 0;
clock_t START_TIME = 0;
clock_t ROLLING_TIME[128] = { 0 };
clock_t PREV_TIME = 0;

// ------------------------------------------------------------
// Make directory
// ------------------------------------------------------------
static void make_dir(const char* path) {
#if defined(_WIN32)
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

// ------------------------------------------------------------
// Fill a horizontal span
// ------------------------------------------------------------
static void fill_span(int y, int xstart, int xend, color c) {
    if (y < 0 || y >= VIDEO_HEIGHT) return;

    if (xstart < 0) xstart = 0;
    if (xend >= VIDEO_WIDTH) xend = VIDEO_WIDTH - 1;
    if (xend < xstart) return;

    unsigned char* row = VIDEO_FRAME[y];
    if (!row) return;

    unsigned char pattern[48];
    unsigned char rgb[3] = { c.b, c.g, c.r };
    for (int i = 0; i < 48; i++) pattern[i] = rgb[i % 3];

    unsigned char* dst = row + xstart * 3;
    int bytes = (xend - xstart + 1) * 3;

#if HAVE_SSE2
    __m128i v0 = _mm_loadu_si128((const __m128i*)(pattern + 0));
    __m128i v1 = _mm_loadu_si128((const __m128i*)(pattern + 16));
    __m128i v2 = _mm_loadu_si128((const __m128i*)(pattern + 32));

    while (bytes >= 48) {
        _mm_storeu_si128((__m128i*)(dst + 0), v0);
        _mm_storeu_si128((__m128i*)(dst + 16), v1);
        _mm_storeu_si128((__m128i*)(dst + 32), v2);
        dst += 48;
        bytes -= 48;
    }
    if (bytes > 0) {
        memcpy(dst, pattern, bytes);
    }
#else
    for (int i = 0; i < bytes; i++) {
        dst[i] = pattern[i % 3];
    }
#endif
}


static void ensure_matrix_stack_capacity() {
    if (MATRIX_STACK_COUNT >= MATRIX_STACK_SIZE) {
        MATRIX_STACK_SIZE *= 2;
        MATRIX_STACK = (float*)realloc(MATRIX_STACK, sizeof(float) * 9 * MATRIX_STACK_SIZE);
        if (!MATRIX_STACK) exit(1);
    }
}

// ------------------------------------------------------------
// Pushes the identity matrix
// ------------------------------------------------------------
static void push_matrix_identity() {
    ensure_matrix_stack_capacity();
    float* dst = &MATRIX_STACK[MATRIX_STACK_COUNT * 9];
    for (int i = 0; i < 9; i++) dst[i] = (i % 4 == 0) ? 1.0f : 0.0f;
    MATRIX_STACK_COUNT++;
}

// ------------------------------------------------------------
// Pushes a matrix
// ------------------------------------------------------------
static void push_matrix_mul(float m[9]) {
    ensure_matrix_stack_capacity();
    float* top = &MATRIX_STACK[(MATRIX_STACK_COUNT - 1) * 9];
    float res[9];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            res[r*3 + c] = top[r*3+0]*m[0*3+c] + top[r*3+1]*m[1*3+c] + top[r*3+2]*m[2*3+c];
        }
    }
    float* dst = &MATRIX_STACK[MATRIX_STACK_COUNT * 9];
    memcpy(dst, res, sizeof(res));
    MATRIX_STACK_COUNT++;
}

// ------------------------------------------------------------
// Pops a matrix from the stack
// ------------------------------------------------------------
static void pop_matrix() {
    if (MATRIX_STACK_COUNT > 1) MATRIX_STACK_COUNT--;
}

// ------------------------------------------------------------
// Pushes a translation transformation
// ------------------------------------------------------------
static void push_translate(float tx, float ty) {
    float m[9] = {1,0,tx, 0,1,ty, 0,0,1};
    push_matrix_mul(m);
}

// ------------------------------------------------------------
// Pushes a rotation transformation
// ------------------------------------------------------------
static void push_rotate(float angle) {
    float c = cosf(angle), s = sinf(angle);
    float m[9] = {c,-s,0, s,c,0, 0,0,1};
    push_matrix_mul(m);
}

// ------------------------------------------------------------
// Pushes a scale transformation
// ------------------------------------------------------------
static void push_scale(float s) {
    float m[9] = {s,0,0, 0,s,0, 0,0,1};
    push_matrix_mul(m);
}

// ------------------------------------------------------------
// Transforms a point
// ------------------------------------------------------------
static void transform_point(float x, float y, float out[2]) {
    float* top = &MATRIX_STACK[(MATRIX_STACK_COUNT - 1) * 9];
    out[0] = top[0]*x + top[1]*y + top[2];
    out[1] = top[3]*x + top[4]*y + top[5];
}

// ------------------------------------------------------------
// Remove directory
// ------------------------------------------------------------
static void remove_dir(const char* path) {
#if defined(_WIN32)

    WIN32_FIND_DATAA fd;
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*.*", path);

    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
            continue;

        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            remove_dir(full);
            _rmdir(full);
        }
        else {
            DeleteFileA(full);
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    _rmdir(path);

#else

    DIR* d = opendir(path);
    if (!d) return;

    struct dirent* e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);

        struct stat st;
        stat(full, &st);

        if (S_ISDIR(st.st_mode)) {
            remove_dir(full);
            rmdir(full);
        }
        else {
            unlink(full);
        }
    }

    closedir(d);
    rmdir(path);

#endif
}

// ------------------------------------------------------------
// Writing 32-bit little endian to destination
// ------------------------------------------------------------
static void write_le32(unsigned char* dst, unsigned int v) {
    dst[0] = (unsigned char)(v);
    dst[1] = (unsigned char)(v >> 8);
    dst[2] = (unsigned char)(v >> 16);
    dst[3] = (unsigned char)(v >> 24);
}

// ------------------------------------------------------------
// Initialize video system
// ------------------------------------------------------------
static void video_init(int width, int height, int fps) {
    VIDEO_WIDTH = width;
    VIDEO_HEIGHT = height;
    VIDEO_FPS = fps;

    make_dir("video");
    FRAME_COUNT = 0;

    MATRIX_STACK_SIZE = 8;
    MATRIX_STACK_COUNT = 0;
    MATRIX_STACK = (float*)malloc(sizeof(float) * 9 * MATRIX_STACK_SIZE);
    if (!MATRIX_STACK) exit(1);
    for (int i = 0; i < 9; i++) MATRIX_STACK[i] = (i % 4 == 0) ? 1.0f : 0.0f;
    MATRIX_STACK_COUNT = 1;
    VIDEO_FRAME = (unsigned char**)malloc(VIDEO_HEIGHT * sizeof(unsigned char*));
    if (!VIDEO_FRAME)exit(1);
    for (int i = 0; i < VIDEO_HEIGHT; i++) {
        VIDEO_FRAME[i] = (unsigned char*)malloc(VIDEO_WIDTH * 3);
        if (!VIDEO_FRAME[i])exit(1);
    }
    START_TIME = clock();
    PREV_TIME = START_TIME;
}

// ------------------------------------------------------------
// Set pixel to color
// ------------------------------------------------------------
static void set_pixel(int x, int y, color c) {
    if (x < 0 || x >= VIDEO_WIDTH)  return;
    if (y < 0 || y >= VIDEO_HEIGHT) return;
    int idx = x * 3;
    VIDEO_FRAME[y][idx + 0] = c.b;
    VIDEO_FRAME[y][idx + 1] = c.g;
    VIDEO_FRAME[y][idx + 2] = c.r;
}

// ------------------------------------------------------------
// Converts HSV to RGB
// ------------------------------------------------------------
static color hsv(float h, float s, float v) {
    float r = 0, g = 0, b = 0;
    if (s <= 0.0f) {
        r = g = b = v;
    }
    else {
        float hh = h * 6.0f;
        if (hh >= 6.0f) hh = 0.0f;
        int i = (int)hh;
        float ff = hh - (float)i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - (s * ff));
        float t = v * (1.0f - (s * (1.0f - ff)));
        switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        }
    }
    color c; c.r = (unsigned char)(fminf(255.0f, r * 255.0f)); c.g = (unsigned char)(fminf(255.0f, g * 255.0f)); c.b = (unsigned char)(fminf(255.0f, b * 255.0f));
    return c;
}

// ------------------------------------------------------------
// Fill frame with solid color
// ------------------------------------------------------------
static void fill_frame(color c) {
#pragma omp parallel for
    for (int y = 0; y < VIDEO_HEIGHT; y++) {
        fill_span(y, 0, VIDEO_WIDTH, c);
    }
}

// ------------------------------------------------------------
// Prints progress on video render
// ------------------------------------------------------------
static void progress(int video_frames) {
    clock_t average_time = 0;
    for (int i = 0; i < 128; i++) {
        average_time += ROLLING_TIME[i];
    }
    average_time /= 128;
    printf(
        "\x1b[H"
        "Frames: %d / %d\n"
        "Time: %.2fsec / %.2fsec\n"
        "Elapsed: %.2fsec\n"
        "Time Left: %.2fsec",
        FRAME_COUNT, video_frames,
        (float)FRAME_COUNT / (float)VIDEO_FPS, (float)video_frames / (float)VIDEO_FPS,
        (float)(clock() - START_TIME) / (CLOCKS_PER_SEC),
        average_time / (float)CLOCKS_PER_SEC * (float)(video_frames - FRAME_COUNT)
    );
}

// ------------------------------------------------------------
// Set pixel colors, like a fragment shader
// ------------------------------------------------------------
static void foreach_pixel(pixel_shader fn, void* userdata) {
#pragma omp parallel for
    for (int y = 0; y < VIDEO_HEIGHT; y++) {
#pragma omp simd
        for (int x = 0; x < VIDEO_WIDTH; x++) {
            int idx = x * 3;
            color rgb;
            fn(x, y, &rgb, userdata);
            set_pixel(x, y, rgb);
        }
    }
}

// ------------------------------------------------------------
// Draw filled circle
// ------------------------------------------------------------
static void fill_circle(int cx, int cy, int radius, color c) {
    int r2 = radius * radius;

#pragma omp parallel for
    for (int dy = -radius + 1; dy <= radius; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= VIDEO_HEIGHT)
            continue;

        int dx = (int)sqrtf((float)(r2 - dy * dy));
        int x1 = cx - dx;
        int x2 = cx + dx;

        if (x1 < 0) x1 = 0;
        if (x2 >= VIDEO_WIDTH) x2 = VIDEO_WIDTH - 1;

#pragma omp simd
        for (int x = x1; x <= x2; x++) {
            set_pixel(x, y, c);
        }
    }
}

// ------------------------------------------------------------
// Draw a line
// ------------------------------------------------------------
static void draw_line(int x0, int y0, int x1, int y1, color c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        set_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ------------------------------------------------------------
// Draw outlined polygon
// ------------------------------------------------------------
static void draw_polygon(int* x, int* y, int amt, color c) {
#pragma omp parallel for
    for (int i = 1; i < amt; i++) {
        draw_line(x[i - 1], y[i - 1], x[i], y[i], c);
    }
    draw_line(x[0], y[0], x[amt - 1], y[amt - 1], c);
}

// ------------------------------------------------------------
// Draw filled triangle
// ------------------------------------------------------------
static void fill_tri(int* x, int* y, color c) {
    int x0 = x[0], y0 = y[0];
    int x1 = x[1], y1 = y[1];
    int x2 = x[2], y2 = y[2];

    int miny = y0;
    if (y1 < miny) miny = y1;
    if (y2 < miny) miny = y2;

    int maxy = y0;
    if (y1 > maxy) maxy = y1;
    if (y2 > maxy) maxy = y2;

    if (miny < 0) miny = 0;
    if (maxy >= VIDEO_HEIGHT) maxy = VIDEO_HEIGHT - 1;

    if (miny > maxy) return;

    for (int py = miny; py <= maxy; py++) {
        float xs[3]; int xc = 0;

        if ((py >= y0 && py < y1) || (py >= y1 && py < y0)) {
            if (y1 != y0) xs[xc++] = x0 + (py - y0) * (float)(x1 - x0) / (float)(y1 - y0);
        }
        if ((py >= y1 && py < y2) || (py >= y2 && py < y1)) {
            if (y2 != y1) xs[xc++] = x1 + (py - y1) * (float)(x2 - x1) / (float)(y2 - y1);
        }
        if ((py >= y2 && py < y0) || (py >= y0 && py < y2)) {
            if (y0 != y2) xs[xc++] = x2 + (py - y2) * (float)(x0 - x2) / (float)(y0 - y2);
        }

        if (xc < 2) continue;

        if (xc == 2) {
            if (xs[0] > xs[1]) { float t = xs[0]; xs[0] = xs[1]; xs[1] = t; }
        } else {
            if (xs[0] > xs[1]) { float t = xs[0]; xs[0] = xs[1]; xs[1] = t; }
            if (xs[1] > xs[2]) { float t = xs[1]; xs[1] = xs[2]; xs[2] = t; }
            if (xs[0] > xs[1]) { float t = xs[0]; xs[0] = xs[1]; xs[1] = t; }
        }

        for (int i = 0; i + 1 < xc; i += 2) {
            int xstart = (int)ceilf(xs[i]);
            int xend   = (int)floorf(xs[i+1]);
            if (xend < 0 || xstart >= VIDEO_WIDTH) continue;
            if (xstart < 0) xstart = 0;
            if (xend >= VIDEO_WIDTH) xend = VIDEO_WIDTH - 1;
            fill_span(py, xstart, xend, c);
        }
    }
}

// ------------------------------------------------------------
// Draw filled rectangle
// ------------------------------------------------------------
static void fill_polygon(int* x, int* y, int n, color c) {
    if (n < 3) return;

    int miny = y[0], maxy = y[0];
    for (int i = 1; i < n; i++) {
        if (y[i] < miny) miny = y[i];
        if (y[i] > maxy) maxy = y[i];
    }

    if (miny < 0) miny = 0;
    if (maxy >= VIDEO_HEIGHT) maxy = VIDEO_HEIGHT - 1;
    if (miny > maxy) return;

    typedef struct {
        int ymin, ymax;
        float x_at_ymin;
        float dxdy;
    } Edge;

    Edge* edges = (Edge*)malloc(sizeof(Edge) * n);
    if (!edges) return;

    int edgeCount = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;

        int x0 = x[i], y0 = y[i];
        int x1 = x[j], y1 = y[j];

        if (y0 == y1) continue;

        if (y0 > y1) {
            int tmp;
            tmp = y0; y0 = y1; y1 = tmp;
            tmp = x0; x0 = x1; x1 = tmp;
        }

        if (y1 <= 0 || y0 >= VIDEO_HEIGHT) continue;

        float dxdy = (float)(x1 - x0) / (float)(y1 - y0);

        Edge e;
        e.ymin = y0;
        e.ymax = y1;
        e.dxdy = dxdy;
        e.x_at_ymin = (float)x0;

        edges[edgeCount++] = e;
    }

    if (edgeCount == 0) {
        free(edges);
        return;
    }

#pragma omp parallel for
    for (int yscan = miny; yscan <= maxy; yscan++) {
        float inter[128];
        int count = 0;

        for (int e = 0; e < edgeCount; e++) {
            if (yscan < edges[e].ymin || yscan >= edges[e].ymax)
                continue;

            float dy = (float)(yscan - edges[e].ymin);
            float xint = edges[e].x_at_ymin + edges[e].dxdy * dy;
            if (count < (int)(sizeof(inter) / sizeof(inter[0])))
                inter[count++] = xint;
        }

        if (count < 2) continue;

        for (int i = 0; i < count - 1; i++) {
            for (int j = i + 1; j < count; j++) {
                if (inter[j] < inter[i]) {
                    float tmp = inter[i];
                    inter[i] = inter[j];
                    inter[j] = tmp;
                }
            }
        }

        for (int i = 0; i + 1 < count; i += 2) {
            int x0 = (int)ceilf(inter[i]);
            int x1 = (int)floorf(inter[i + 1]);

            if (x1 < 0 || x0 >= VIDEO_WIDTH) continue;
            if (x0 < 0) x0 = 0;
            if (x1 >= VIDEO_WIDTH) x1 = VIDEO_WIDTH - 1;

            if (x0 <= x1)
                fill_span(yscan, x0, x1, c);
        }
    }

    free(edges);
}

// ------------------------------------------------------------
// Draw filled rectangle
// ------------------------------------------------------------
static void fill_rect(int cx, int cy, int w, int h, color c) {
#pragma omp parallel for
    for (int i = 0; i < h; i++) {
		fill_span(cy + i, cx, cx + w - 1, c);
    }
}

typedef enum {
    RPN_TOK_NUM,
    RPN_TOK_X,
    RPN_TOK_T,
    RPN_TOK_ADD,
    RPN_TOK_SUB,
    RPN_TOK_MUL,
    RPN_TOK_DIV,
    RPN_TOK_SIN,
    RPN_TOK_COS,
    RPN_TOK_TAN,
    RPN_TOK_EXP,
    RPN_TOK_LOG,
    RPN_TOK_INVALID
} RPN_TokenType;

typedef struct {
    RPN_TokenType type;
    float value;
} RPN_Token;

typedef struct {
    RPN_Token* tokens;
    int count;
} RPN_Program;


// ------------------------------------------------------------
// Evaluates RPN
// ------------------------------------------------------------
static float rpn_eval(const RPN_Program* prog, float x) {
    float stack[64];
    int sp = 0;

    for (int i = 0; i < prog->count; i++) {
        RPN_Token t = prog->tokens[i];

        switch (t.type) {
        case RPN_TOK_NUM:
            stack[sp++] = t.value;
            break;

        case RPN_TOK_X:
            stack[sp++] = x;
            break;

        case RPN_TOK_T:
            stack[sp++] = (float)FRAME_COUNT / (float)VIDEO_FPS;
            break;

        case RPN_TOK_ADD: {
            float b = stack[--sp];
            float a = stack[--sp];
            stack[sp++] = a + b;
        } break;

        case RPN_TOK_SUB: {
            float b = stack[--sp];
            float a = stack[--sp];
            stack[sp++] = a - b;
        } break;

        case RPN_TOK_MUL: {
            float b = stack[--sp];
            float a = stack[--sp];
            stack[sp++] = a * b;
        } break;

        case RPN_TOK_DIV: {
            float b = stack[--sp];
            float a = stack[--sp];
            stack[sp++] = a / b;
        } break;

        case RPN_TOK_SIN:
            stack[sp - 1] = sinf(stack[sp - 1]);
            break;

        case RPN_TOK_COS:
            stack[sp - 1] = cosf(stack[sp - 1]);
            break;

        case RPN_TOK_TAN:
            stack[sp - 1] = tanf(stack[sp - 1]);
            break;

        case RPN_TOK_EXP:
            stack[sp - 1] = expf(stack[sp - 1]);
            break;

        case RPN_TOK_LOG:
            stack[sp - 1] = logf(stack[sp - 1]);
            break;

        default:
            break;
        }
    }

    return stack[0];
}


// ------------------------------------------------------------
// Converts RPN token
// ------------------------------------------------------------
static RPN_TokenType rpn_parse_op(const char* s) {
    if (!strcmp(s, "+"))   return RPN_TOK_ADD;
    if (!strcmp(s, "-"))   return RPN_TOK_SUB;
    if (!strcmp(s, "*"))   return RPN_TOK_MUL;
    if (!strcmp(s, "/"))   return RPN_TOK_DIV;
    if (!strcmp(s, "sin")) return RPN_TOK_SIN;
    if (!strcmp(s, "cos")) return RPN_TOK_COS;
    if (!strcmp(s, "tan")) return RPN_TOK_TAN;
    if (!strcmp(s, "exp")) return RPN_TOK_EXP;
    if (!strcmp(s, "log")) return RPN_TOK_LOG;
    return RPN_TOK_INVALID;
}

// ------------------------------------------------------------
// Parses RPN expression
// ------------------------------------------------------------
static RPN_Program rpn_parse(const char* src) {
    RPN_Token* toks = (RPN_Token*)malloc(sizeof(RPN_Token) * 128);
    int count = 0;

    char buf[64];
    int bi = 0;
    for (int i = 0;; i++) {
        char c = src[i];
        if (c == ' ' || c == '\t' || c == '\0') {
            if (bi > 0) {
                buf[bi] = '\0';
                char* end;
                float v = strtof(buf, &end);

                if (*end == '\0') {
                    toks[count].type = RPN_TOK_NUM;
                    toks[count].value = v;
                    count++;
                }
                else if (!strcmp(buf, "x")) {
                    toks[count].type = RPN_TOK_X;
                    toks[count].value = 0;
                    count++;
                }
                else if (!strcmp(buf, "t")) {
                    toks[count].type = RPN_TOK_T;
                    toks[count].value = 0;
                    count++;
                }
                else {
                    RPN_TokenType op = rpn_parse_op(buf);
                    if (op == RPN_TOK_INVALID) {
                        printf("Unknown token: %s\n", buf);
                        exit(1);
                    }
                    toks[count].type = op;
                    toks[count].value = 0;
                    count++;
                }
                bi = 0;
            }
            if (c == '\0') break;
        }
        else {
            if (bi < 63) buf[bi++] = c;
        }
    }
    RPN_Program p = { toks, count };
    return p;
}

// ------------------------------------------------------------
// Sets graphing window dimensions
// ------------------------------------------------------------
static void set_graphing_window(float xmin, float xmax, float ymin, float ymax) {
    XMIN = xmin;
    XMAX = xmax;
    YMIN = ymin;
    YMAX = ymax;
}

// ------------------------------------------------------------
// Maps y position to screen coordinate
// ------------------------------------------------------------
static inline int map_to_screen(float y) {
    float t = (y - YMIN) / (YMAX - YMIN);
    float py = (1.0f - t) * (VIDEO_HEIGHT - 1);
    return (int)py;
}

// ------------------------------------------------------------
// Graphs RPN expression
// ------------------------------------------------------------
static void graph_rpn_struct(const RPN_Program* prog, int weight, color rgb) {
    float dx = (XMAX - XMIN) / (float)VIDEO_WIDTH;

    float prev_x_val = XMIN;
    float prev_y_val = rpn_eval(prog, prev_x_val);
    int prev_sx = 0;
    int prev_sy = map_to_screen(prev_y_val);

    for (int px = 1; px < VIDEO_WIDTH; px++) {
        float curr_x_val = XMIN + px * dx;
        float curr_y_val = rpn_eval(prog, curr_x_val);
        int curr_sy = map_to_screen(curr_y_val);

        if (weight <= 1) {
            draw_line(px - 1, prev_sy, px, curr_sy, rgb);
        } else {
            // For thicker lines, we draw multiple offset lines to simulate stroke weight
            for (int w = -weight / 2; w <= weight / 2; w++) {
                draw_line(px - 1, prev_sy + w, px, curr_sy + w, rgb);
            }
        }

        prev_y_val = curr_y_val;
        prev_sy = curr_sy;
    }
}

// ------------------------------------------------------------
// Graphs RPN string
// ------------------------------------------------------------
static void graph_rpn(const char* rpn_src, int weight, color rgb) {
    RPN_Program prog = rpn_parse(rpn_src);
    graph_rpn_struct(&prog, weight, rgb);
    free(prog.tokens);
}

typedef struct {
    int w, h;
    unsigned char* pixels;
} image;

// ------------------------------------------------------------
// Loads image from file
// ------------------------------------------------------------
static image load_image(const char* filename) {
    image img = {0, 0, NULL};
    int channels;
    const char* ext = strrchr(filename, '.');

    if (ext && strcmp(ext, ".svg") == 0) {
        system("mkdir -p tmp_img");
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rsvg-convert -f png -h 1080 %s -o tmp_img/conv.png", filename);
        system(cmd);
        
        img.pixels = stbi_load("tmp_img/conv.png", &img.w, &img.h, &channels, 4);
        remove("tmp_img/conv.png");
    } else {
        img.pixels = stbi_load(filename, &img.w, &img.h, &channels, 4);
    }

    if (!img.pixels) {
        fprintf(stderr, "Error: Could not load image %s\n", filename);
    }
    return img;
}

// ------------------------------------------------------------
// Blits image struct onto frame
// ------------------------------------------------------------
void blit_image(image* img, int x, int y, float target_width, float angle) {
    if (!img || !img->pixels || img->w == 0) return;

    float ratio = target_width / (float)img->w;
    int sw = (int)target_width;
    int sh = (int)(img->h * ratio);

    float rad = -angle * PI / 180.0f;
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    float diag = sqrtf(sw*sw + sh*sh);
    int half_diag = (int)(diag / 2.0f) + 1;

    int min_tx = x - half_diag;
    int max_tx = x + half_diag;
    int min_ty = y - half_diag;
    int max_ty = y + half_diag;

    #pragma omp parallel for
    for (int ty = min_ty; ty <= max_ty; ty++) {
        if (ty < 0 || ty >= VIDEO_HEIGHT) continue;

        for (int tx = min_tx; tx <= max_tx; tx++) {
            if (tx < 0 || tx >= VIDEO_WIDTH) continue;

            int dx = tx - x;
            int dy = ty - y;

            float rx = dx * cos_a - dy * sin_a;
            float ry = dx * sin_a + dy * cos_a;

            float px = rx + sw / 2.0f;
            float py = ry + sh / 2.0f;

            if (px >= 0 && px < sw && py >= 0 && py < sh) {
                int src_x = (int)(px / ratio);
                int src_y = (int)(py / ratio);

                if (src_x >= 0 && src_x < img->w && src_y >= 0 && src_y < img->h) {
                    int idx = (src_y * img->w + src_x) * 4;
                    unsigned char a = img->pixels[idx + 3];

                    if (a > 0) {
                        unsigned char r = img->pixels[idx];
                        unsigned char g = img->pixels[idx + 1];
                        unsigned char b = img->pixels[idx + 2];
                        set_pixel(tx, ty, (color){r, g, b});
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------
// Blits latex mask image struct onto frame
// ------------------------------------------------------------
void blit_latex_mask(image* img, int x, int y, float target_width, float angle, color c) {
    if (!img || !img->pixels || img->w == 0) return;

    // 1. Calculate scaling and dimensions
    float ratio = target_width / (float)img->w;
    int sw = (int)target_width;
    int sh = (int)(img->h * ratio);

    // 2. Setup rotation (Inverse: rotate by -angle to sample)
    float rad = -angle * PI / 180.0f;
    float cos_a = cosf(rad);
    float sin_a = sinf(rad);

    // 3. Define the bounding box on screen
    // Using the diagonal ensures we cover the full rotation area
    float diag = sqrtf((float)sw * sw + (float)sh * sh);
    int half_diag = (int)(diag / 2.0f) + 1;

    int min_tx = x - half_diag;
    int max_tx = x + half_diag;
    int min_ty = y - half_diag;
    int max_ty = y + half_diag;

    #pragma omp parallel for
    for (int ty = min_ty; ty <= max_ty; ty++) {
        if (ty < 0 || ty >= VIDEO_HEIGHT) continue;

        for (int tx = min_tx; tx <= max_tx; tx++) {
            if (tx < 0 || tx >= VIDEO_WIDTH) continue;

            int dx = tx - x;
            int dy = ty - y;

            float rx = dx * cos_a - dy * sin_a;
            float ry = dx * sin_a + dy * cos_a;

            float px = rx + sw / 2.0f;
            float py = ry + sh / 2.0f;

            if (px >= 0 && px < sw && py >= 0 && py < sh) {
                int src_x = (int)(px / ratio);
                int src_y = (int)(py / ratio);

                if (src_x >= 0 && src_x < img->w && src_y >= 0 && src_y < img->h) {
                    int idx = (src_y * img->w + src_x) * 4;
                    unsigned char intensity = img->pixels[idx]; 

                    if (intensity > 0) {
                        set_pixel(tx, ty, (color){
                            (unsigned char)((c.r * intensity) / 255),
                            (unsigned char)((c.g * intensity) / 255),
                            (unsigned char)((c.b * intensity) / 255)
                        });
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------
// Writes LaTeX to frame
// ------------------------------------------------------------
static void write_latex(const char* latex, int x, int y, float scale, float angle, color c) {
    char cmd[2048];
    char cache_path[1024];
    
    char safe_name[256];
    strncpy(safe_name, latex, 255);
    for(int i = 0; safe_name[i]; i++) {
        if (safe_name[i] == '\\' || safe_name[i] == '{' || safe_name[i] == '}' || safe_name[i] == ' ') 
            safe_name[i] = '_';
    }

    make_dir("latex_cache");
    snprintf(cache_path, sizeof(cache_path), "latex_cache/%s.png", safe_name);

    if (access(cache_path, F_OK) == -1) {
        snprintf(cmd, sizeof(cmd), "node render.js \"%s\" > tmp.svg", latex);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "rsvg-convert -f png -h 500 -o %s tmp.svg", cache_path);
        system(cmd);
        remove("tmp.svg");
    }

    int width, height, channels;
    image img;
    img.pixels = stbi_load(cache_path, &img.w, &img.h, &channels, 4);
    
    if (img.pixels) {
        blit_latex_mask(&img, x, y, scale, angle, c);
        stbi_image_free(img.pixels);
    }
}

// ------------------------------------------------------------
// Save current frame as BMP
// ------------------------------------------------------------
static void save_frame(void) {
    char filename[256];
    snprintf(filename, sizeof(filename), "video/frame_%05d.bmp", FRAME_COUNT++);

    FILE* fp = fopen(filename, "wb");
    if (!fp) return;

    int w = VIDEO_WIDTH;
    int h = VIDEO_HEIGHT;
    int channels = 3;

    int row_bytes = w * channels;
    int padding = (4 - (row_bytes % 4)) % 4;

    unsigned char header[54] = {
        'B','M',
        0,0,0,0,
        0,0,0,0,
        54,0,0,0,
        40,0,0,0,
        0,0,0,0,
        0,0,0,0,
        1,0,
        24,0,
        0,0,0,0,
        0,0,0,0,
        0x13,0x0B,0,0,
        0x13,0x0B,0,0,
        0,0,0,0,
        0,0,0,0
    };

    int filesize = 54 + (row_bytes + padding) * h;
    write_le32(&header[18], w);
    write_le32(&header[22], h);
    write_le32(&header[2], filesize);

    fwrite(header, 1, 54, fp);

    unsigned char pad[3] = { 0,0,0 };

    for (int y = h - 1; y >= 0; y--) {
        unsigned char* row = VIDEO_FRAME[y];
        fwrite(row, 1, row_bytes, fp);
        fwrite(pad, 1, padding, fp);
    }

    ROLLING_TIME[FRAME_COUNT % 128] = clock() - PREV_TIME;
    PREV_TIME = clock();

    fclose(fp);
}

// ------------------------------------------------------------
// Saves video
// ------------------------------------------------------------
void video_end(void) {
    for (int i = 0; i < VIDEO_HEIGHT; i++) {
        free(VIDEO_FRAME[i]);
    }
    free(VIDEO_FRAME);
    if (MATRIX_STACK) free(MATRIX_STACK);

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -framerate %d -i video/frame_%%05d.bmp "
        "-s %dx%d -pix_fmt yuv420p render.mp4",
        VIDEO_FPS, VIDEO_WIDTH, VIDEO_HEIGHT
    );
    system(cmd);
    remove_dir("video");
    remove_dir("latex_cache");
}

#endif