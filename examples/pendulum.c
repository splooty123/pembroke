#define PEMBROKE_IMPLEMENTATION
#include "pembroke.h"

static float wrap01(float v) {
    v -= floorf(v);
    return v;
}

static color hsv_to_rgb(float h, float s, float v) {
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

/* compute double pendulum derivatives for state [t1,t2,w1,w2] */
static void dp_deriv(const float s[4], float out[4]) {
    const float g = 9.81f;
    float t1 = s[0], t2 = s[1], w1 = s[2], w2 = s[3];
    float delta = t2 - t1;
    float den = 2.0f - cosf(2.0f * delta);
    if (den == 0.0f) den = 1e-6f;

    float w1_dot = (-g * (2.0f) * sinf(t1) - g * sinf(t1 - 2.0f * t2) - 2.0f * sinf(delta) * (w2 * w2 + w1 * w1 * cosf(delta))) / den;
    float w2_dot = (2.0f * sinf(delta) * (w1 * w1 + g * cosf(t1) + w2 * w2 * cosf(delta))) / den;

    out[0] = w1;
    out[1] = w2;
    out[2] = w1_dot;
    out[3] = w2_dot;
}

/* RK4 step for state array */
static void rk4_step(float s[4], float dt) {
    float k1[4], k2[4], k3[4], k4[4], tmp[4];
    dp_deriv(s, k1);
    for (int i = 0; i < 4; i++) tmp[i] = s[i] + 0.5f * dt * k1[i];
    dp_deriv(tmp, k2);
    for (int i = 0; i < 4; i++) tmp[i] = s[i] + 0.5f * dt * k2[i];
    dp_deriv(tmp, k3);
    for (int i = 0; i < 4; i++) tmp[i] = s[i] + dt * k3[i];
    dp_deriv(tmp, k4);
    for (int i = 0; i < 4; i++) s[i] += (dt / 6.0f) * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
}

int main(void) {
    const int W = 640, H = 480, FPS = 30;
    video_init(W, H, FPS);

    const int GRID_W = 240;
    const int GRID_H = 160;
    const float PI = 3.14159265358979323846f;

    /* allocate grid values */
    float* grid = (float*)malloc(sizeof(float) * GRID_W * GRID_H);
    if (!grid) return 1;

    /* integration parameters */
    const int STEPS = 200;
    const float DT = 0.02f;

    /* compute fractal values: final angle2 normalized */
    for (int j = 0; j < GRID_H; j++) {
        for (int i = 0; i < GRID_W; i++) {
            float a1 = -PI + 2.0f * PI * ((float)i / (float)(GRID_W - 1));
            float a2 = -PI + 2.0f * PI * ((float)j / (float)(GRID_H - 1));
            float s[4] = { a1, a2, 0.0f, 0.0f };
            for (int t = 0; t < STEPS; t++) rk4_step(s, DT);
            float v = s[1];
            v = wrap01((v + PI) / (2.0f * PI));
            grid[j * GRID_W + i] = v;
        }
    }

    const int FRAMES = FPS * 6; /* 6 seconds */
    for (int f = 0; f < FRAMES; f++) {
        float hue_shift = (float)f / (float)FRAMES;
        clear_frame(0);
        color bg; bg.r = 6; bg.g = 8; bg.b = 12;
        fill_frame(bg);

        /* draw grid as small rectangles */
        int cell_w = (W + GRID_W - 1) / GRID_W;
        int cell_h = (H + GRID_H - 1) / GRID_H;
        for (int j = 0; j < GRID_H; j++) {
            for (int i = 0; i < GRID_W; i++) {
                float v = grid[j * GRID_W + i];
                float hue = wrap01(v + hue_shift);
                color col = hsv_to_rgb(hue, 1.0f, 1.0f);
                int x = i * cell_w;
                int y = j * cell_h;
                fill_rect(x, y, cell_w, cell_h, col);
            }
        }

        save_frame();
        progress(FRAMES);
    }

    free(grid);
    video_end();
    return 0;
}

