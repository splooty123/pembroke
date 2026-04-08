#define VIDEO_TOOL_IMPLEMENTATION
#include "video-tool.h"

int main() {
    video_init(1980, 1060, 60);
    for (int i = 0; i < 300; i++) {
        clear_frame(255);
        graph_rpn("x x * 5000 * t * 497.5 -", 4, (color) { 0, 0, 0 });
        write_latex("x^2", 20, 20, (color) { 0, 0, 0 });
        save_frame();
        progress(300);
    }
    video_end();
}