#define PEMBROKE_IMPLEMENTATION
#include "pembroke.h"

#define BENCH(name, iterations, function_call) do { \
    double start_time = omp_get_wtime(); \
    for (int i = 0; i < iterations; i++) { \
        function_call \
    } \
    double end_time = omp_get_wtime(); \
    double total_elapsed = end_time - start_time; \
    printf("\n==========================================\n"); \
    printf(" BENCHMARK: %s\n", name); \
    printf("------------------------------------------\n"); \
    printf(" Iterations:  %d\n", iterations); \
    printf(" Total Time:  %.6f sec\n", total_elapsed); \
    printf(" Avg per Op:  %.6f ms\n", (total_elapsed / iterations) * 1000.0); \
    printf(" FPS (Est):   %.2f\n", 1.0 / (total_elapsed / iterations)); \
    printf("==========================================\n"); \
} while (0)

int main() {
    VIDEO_FPS = 60;
    VIDEO_WIDTH = 640;
    VIDEO_HEIGHT = 480;
    VIDEO_FRAME = (unsigned char**)malloc(sizeof(unsigned char*) * VIDEO_HEIGHT);
    for(int i = 0; i < VIDEO_HEIGHT; i++) {
        VIDEO_FRAME[i] = (unsigned char*)malloc(VIDEO_WIDTH * 3);
    }

    BENCH("fill_frame", 1000, fill_frame((color){ 128, 64, 192 });); image img;
    BENCH("load_image", 1, img = load_image("capital_red_A.png"););
    BENCH("blit_image", 1000, blit_image(&img, 0, 0, VIDEO_WIDTH, 0););
    BENCH("write_latex", 1000, write_latex("E=mc^2", 150, 150, 300, 0.0f, (color){ 255, 255, 255 }););
    BENCH("fill_span", 1000, fill_span(0, 0, VIDEO_WIDTH, (color){ 255, 0, 0 }););
    BENCH("fill_rect", 1000, fill_rect(0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, (color){ 0, 255, 0 }););
    BENCH("draw_line", 1000, draw_line(0, 0, VIDEO_WIDTH - 1, VIDEO_HEIGHT - 1, (color){ 0, 0, 255 }););
    BENCH("set_pixel", 1000, set_pixel(0, 0, (color){ 255, 255, 0 }););

    for(int i = 0; i < VIDEO_HEIGHT; i++) {
        free(VIDEO_FRAME[i]);
    }
    free(VIDEO_FRAME);
    free(img.pixels);
}