#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#endif 

typedef struct {
    unsigned char r, g, b;
} color;

typedef void (*pixel_shader)(
    int x, int y,
    color* rgb,
    void* userdata
);

typedef enum {
    ROTATE,
	SCALE,
	TRANSLATE_X,
	TRANSLATE_Y
} transform_type;

typedef struct {
	transform_type type;
    float value;
} transformation;

extern int VIDEO_WIDTH;
extern int VIDEO_HEIGHT;
extern int VIDEO_FPS;
extern unsigned char** VIDEO_FRAME;

#ifdef PEMBROKE_IMPLEMENTATION

int VIDEO_WIDTH = 640;
int VIDEO_HEIGHT = 480;
int VIDEO_FPS = 60;
unsigned char** VIDEO_FRAME = NULL;
static int FRAME_COUNT = 0;
static int TRANSFORMATION_INDEX = 0;
static int TRANSFORMATION_STACK_SIZE = 1;
static transformation* TRANSFORMATION_STACK = (transformation*)malloc(sizeof(transformation) * TRANSFORMATION_STACK_SIZE);


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

    VIDEO_FRAME = (unsigned char**)malloc(VIDEO_HEIGHT * sizeof(unsigned char*));
    if (!VIDEO_FRAME)exit(1);
    for (int i = 0; i < VIDEO_HEIGHT; i++) {
        VIDEO_FRAME[i] = (unsigned char*)malloc(VIDEO_WIDTH * 3);
        if (!VIDEO_FRAME[i])exit(1);
    }
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
// Fill frame with solid color
// ------------------------------------------------------------
static void fill_frame(color c) {
    for (int y = 0; y < VIDEO_HEIGHT; y++) {
        for (int x = 0; x < VIDEO_WIDTH; x++) {
            set_pixel(x, y, c);
        }
    }
}

// ------------------------------------------------------------
// Clear frame with grayscale color
// ------------------------------------------------------------
static void clear_frame(unsigned char alpha) {
    for (int y = 0; y < VIDEO_HEIGHT; y++) {
        memset(VIDEO_FRAME[y], alpha, VIDEO_WIDTH * 3);
    }
}

// ------------------------------------------------------------
// Prints progress on video render
// ------------------------------------------------------------
static void progress(int video_frames) {
    printf("\x1b[HFrames: %d / %d\nTime: %.2fsec / %.2fsec", FRAME_COUNT, video_frames, (float)FRAME_COUNT / (float)VIDEO_FPS, (float)video_frames / (float)VIDEO_FPS);
}

// ------------------------------------------------------------
// Set pixel colors, like a fragment shader
// ------------------------------------------------------------
static void foreach_pixel(pixel_shader fn, void* userdata) {
    for (int y = 0; y < VIDEO_HEIGHT; y++) {
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

    for (int dy = -radius + 1; dy <= radius; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= VIDEO_HEIGHT)
            continue;

        int dx = (int)sqrtf((float)(r2 - dy * dy));
        int x1 = cx - dx;
        int x2 = cx + dx;

        if (x1 < 0) x1 = 0;
        if (x2 >= VIDEO_WIDTH) x2 = VIDEO_WIDTH - 1;

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
    for (int i = 1; i < amt; i++) {
        draw_line(x[i - 1], y[i - 1], x[i], y[i], c);
    }
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
            for (int px = xstart; px <= xend; px++) set_pixel(px, py, c);
        }
    }
}

// ------------------------------------------------------------
// Draw filled rectangle
// ------------------------------------------------------------
static void fill_polygon(int* x, int* y, int amt, color c) {
    int minx = 0;
    int maxx = 0;
    int miny = 0;
    int maxy = 0;
    for (int i = 0; i < amt; i++) {
        minx = min(minx, x[i]);
        maxx = max(maxx, x[i]);
        miny = min(miny, y[i]);
        maxy = max(maxy, y[i]);
    }
    for (int i = miny; i <= maxy; i++) {
        for (int j = minx; j <= maxx; j++) {
            int crossings = 0;
            for (int k = 0; k < amt; k++) {
                int k1 = (k + 1) % amt;
                if ((y[k] > i) != (y[k1] > i)) {
                    float atX = (float)(x[k1] - x[k]) * (float)(i - y[k]) / (float)(y[k1] - y[k]) + x[k];
                    if (atX > j) crossings++;
                }
            }
            if (crossings % 2 == 1) {
                set_pixel(j, i, c);
            }
		}
    }
}   

// ------------------------------------------------------------
// Draw filled rectangle
// ------------------------------------------------------------
static void fill_rect(int cx, int cy, int w, int h, color c) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            set_pixel(j + cx, i + cy, c);
        }
    }
}

// ------------------------------------------------------------
// Push a tranformation onto the transformation stack
// ------------------------------------------------------------
static void push_transformation(transform_type type, float value) {
	TRANSFORMATION_STACK[TRANSFORMATION_INDEX++] = (transformation){ type, value };
    if (TRANSFORMATION_INDEX >= TRANSFORMATION_STACK_SIZE) {
        TRANSFORMATION_STACK = (transformation*)realloc(sizeof(transformation) * (TRANSFORMATION_SIZE *= 2));
    }
}

// ------------------------------------------------------------
// Pop a tranformation off the transformation stack
// ------------------------------------------------------------
static void push_transformation(transform_type type, float value) {
    TRANSFORMATION_INDEX--;
    if (TRANSFORMATION_INDEX < TRANSFORMATION_STACK_SIZE / 2) {
        TRANSFORMATION_STACK = (transformation*)realloc(sizeof(transformation) * (TRANSFORMATION_SIZE /= 2));
    }
}

// ------------------------------------------------------------
// Pop a tranformation off the transformation stack
// ------------------------------------------------------------
static float* transform_point(float x, float y) {
	ret = (float*)malloc(sizeof(float) * 2);
    for (int i = TRANSFORMATION_INDEX; i > 0; i--) {
        switch (TRANSFORMATION_STACK[i - 1].type) {
            case ROTATE: {
                float angle = TRANSFORMATION_STACK[i - 1].value;
                float cos_a = cosf(angle);
                float sin_a = sinf(angle);
                float tx = ret[0] * cos_a - ret[1] * sin_a;
                float ty = ret[0] * sin_a + ret[1] * cos_a;
                ret[0] = tx;
                ret[1] = ty;
			} break;
            case SCALE: {
                float s = TRANSFORMATION_STACK[i - 1].value;
                ret[0] *= s;
                ret[1] *= s;
            } break;
            case TRANSLATE_X: {
                float tx = TRANSFORMATION_STACK[i - 1].value;
                ret[0] += tx;
            } break;
			case TRANSLATE_Y: {
                float ty = TRANSFORMATION_STACK[i - 1].value;
                ret[1] += ty;
			} break;
        }
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
                    toks[count++] = (RPN_Token){ RPN_TOK_NUM, v };
                }
                else if (!strcmp(buf, "x")) {
                    toks[count++] = (RPN_Token){ RPN_TOK_X, 0 };
                }
                else if (!strcmp(buf, "t")) {
                    toks[count++] = (RPN_Token){ RPN_TOK_T, 0 };
                }
                else {
                    RPN_TokenType op = rpn_parse_op(buf);
                    if (op == RPN_TOK_INVALID) {
                        printf("Unknown token: %s\n", buf);
                        exit(1);
                    }
                    toks[count++] = (RPN_Token){ op, 0 };
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
// Graphs RPN expression
// ------------------------------------------------------------
static void graph_rpn_struct(const RPN_Program* prog, int weight, color rgb)
{
    for (int i = 0; i < VIDEO_WIDTH; i++) {
        float xn = (float)i / (float)VIDEO_WIDTH * 2.0f - 1.0f;
        float y = rpn_eval(prog, xn);

        int py = (int)(VIDEO_HEIGHT / 2 - y);
        fill_circle(i, py, weight, rgb);
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
// Loads a BMP as image struct
// ------------------------------------------------------------
static image load_bmp(const char* filename) {
    image img = { 0 };

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("Could not open %s\n", filename);
        return img;
    }

    unsigned char header[54];
    fread(header, 1, 54, fp);

    int w = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
    int h = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);

    int row_bytes = w * 3;
    int padding = (4 - (row_bytes % 4)) % 4;

    unsigned char* data = (unsigned char*)malloc(w * h * 3);

    for (int y = h - 1; y >= 0; y--) {
        fread(data + y * row_bytes, 1, row_bytes, fp);
        fseek(fp, padding, SEEK_CUR);
    }

    fclose(fp);

    img.w = w;
    img.h = h;
    img.pixels = data;
    return img;
}

// ------------------------------------------------------------
// Blits image struct onto frame
// ------------------------------------------------------------
static void blit_image(image* img, int dst_x, int dst_y) {
    for (int y = 0; y < img->h; y++) {
        int sy = dst_y + y;
        if (sy < 0 || sy >= VIDEO_HEIGHT) continue;

        for (int x = 0; x < img->w; x++) {
            int sx = dst_x + x;
            if (sx < 0 || sx >= VIDEO_WIDTH) continue;

            int src_idx = (y * img->w + x) * 3;
            int dst_idx = sx * 3;

            VIDEO_FRAME[sy][dst_idx + 0] = img->pixels[src_idx + 0];
            VIDEO_FRAME[sy][dst_idx + 1] = img->pixels[src_idx + 1];
            VIDEO_FRAME[sy][dst_idx + 2] = img->pixels[src_idx + 2];
        }
    }
}

// ------------------------------------------------------------
// Blits latex mask image struct onto frame
// ------------------------------------------------------------
static void blit_latex_mask(const image* mask, int x0, int y0, color text_color) {
    if (!mask) return;
    if (!mask->pixels) return;
    if (!VIDEO_FRAME) return;

    int w = mask->w;
    int h = mask->h;
    if (w <= 0 || h <= 0) return;

    unsigned char* px = mask->pixels;

    for (int y = 0; y < h; y++) {
        int yy = y0 + y;
        if (yy < 0 || yy >= VIDEO_HEIGHT) continue;

        for (int x = 0; x < w; x++) {
            int xx = x0 + x;
            if (xx < 0 || xx >= VIDEO_WIDTH) continue;

            int idx = (y * w + x) * 3;

            unsigned char cov = px[idx + 2];

            float a = cov / 255.0f;

            int fb = xx * 3;

            float br = VIDEO_FRAME[yy][fb + 2];
            float bg = VIDEO_FRAME[yy][fb + 1];
            float bb = VIDEO_FRAME[yy][fb + 0];

            VIDEO_FRAME[yy][fb + 2] = (unsigned char)(br * a + text_color.r * (1.0f - a));
            VIDEO_FRAME[yy][fb + 1] = (unsigned char)(bg * a + text_color.g * (1.0f - a));
            VIDEO_FRAME[yy][fb + 0] = (unsigned char)(bb * a + text_color.b * (1.0f - a));
        }
    }
}

// ------------------------------------------------------------
// Saves LaTeX as BMP
// ------------------------------------------------------------
static int latex_to_bmp(const char* latex, const char* bmp_path) {
    char* text = (char*)malloc((strlen(latex) + 32) * sizeof(char));
    snprintf(text, (strlen(latex) + 32) * sizeof(char), "node render.js \"%s\" > tmp.svg", latex);
    system(text); free(text);
    system("magick -density 300 tmp.svg -resize 512x -define bmp:format=bmp3 mask.bmp");
    remove("tmp.svg");

    return 0;
}

// ------------------------------------------------------------
// Writes LaTeX to frame
// ------------------------------------------------------------
static void write_latex(const char* latex, int x, int y, color c) {
    size_t cmd_size = strlen(latex) + 64;
    char* cmd = (char*)malloc(cmd_size);
    snprintf(cmd, cmd_size, "node render.js \"%s\" > tmp.svg", latex);
    system(cmd);
    free(cmd);

    system("magick -density 1000 tmp.svg -resize 512x -define bmp:format=bmp3 mask.bmp");
    remove("tmp.svg");

    image mask = load_bmp("mask.bmp");
    remove("mask.bmp");

    blit_latex_mask(&mask, x, y, c);

    free(mask.pixels);
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
    free(TRANSFORMATION_STACK)

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -framerate %d -i video/frame_%%05d.bmp "
        "-s %dx%d -pix_fmt yuv420p render.mp4",
        VIDEO_FPS, VIDEO_WIDTH, VIDEO_HEIGHT
    );
    system(cmd);
    remove_dir("video");
}

#endif