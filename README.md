# Pembroke
Pembroke is a single file C library for creating and animating math videos.
# Dependencies
* Node.js - for running MathJax
* MathJax - for rendering LaTeX math formulas to SVG
* ImageMagick - for converting SVG to BMP
* FFmpeg (included) - for encoding BMP frames into a video
# Usage
The following is an minimal example of creating a video with Pembroke.
```c
#define PEMBROKE_IMPLEMENTATION
#include "pembroke.h"

int main() {
    // Initialize a video:
    // width = 620 px, height = 480 px, framerate = 60 fps
    video_init(620, 480, 60);

    for (int i = 0; i < 60; i++) {
        // Clear frame with white background
        clear_frame(255);

        // Draw LaTeX math formula at the top-left corner
        write_latex("\\frac{dy}{dx} e^x = e^x", 0, 0, (color){0, 0, 0});

        // Save the current frame as a BMP
        save_frame();
    }

    // Encode all frames into "render.mp4"
    video_end();
}
```

# API functions

- `make_dir(const char* path)`
        create a directory (cross-platform).
- `remove_dir(const char* path)`
        recursively remove a directory.
- `write_le32(unsigned char* dst, unsigned int v)   `
        write a 32-bit little-endian value into `dst`.
- `video_init(int width, int height, int fps) `
        initialize the video system and allocate the frame buffer.
- `set_pixel(int x, int y, color c) `
        set a pixel in the current frame to the color `c`.
- `fill_frame(color c)  `
        fill the entire frame with the color `c`.
- `clear_frame(unsigned char alpha) `
        clear the frame using a grayscale value (0..255).
- `progress(int video_frames)` 
        print render progress and elapsed time to stdout.
- `foreach_pixel(pixel_shader fn, void* userdata)` 
        call `fn` for every pixel to produce colors.
- `fill_circle(int cx, int cy, int radius, color c)`
        draw a filled circle.
- `fill_rect(int cx, int cy, int w, int h, color c)` 
        draw a filled rectangle.
- `rpn_eval(const RPN_Program* prog, float x)` 
        evaluate an RPN program at `x`.
- `rpn_parse_op(const char* s)` 
        parse an RPN operator token string.
- `rpn_parse(const char* src)` 
        parse an RPN expression (space-separated tokens) into an `RPN_Program`.
- `graph_rpn_struct(const RPN_Program* prog, int weight, color rgb)` 
        plot an RPN program onto the frame.
- `graph_rpn(const char* rpn_src, int weight, color rgb)` 
        parse and graph an RPN expression string.
- `load_bmp(const char* filename)` 
        load a 24-bit BMP into an `image` struct (`w`, `h`, `pixels`).
- `blit_image(image* img, int dst_x, int dst_y)` 
        blit an `image` onto the current frame.
- `blit_latex_mask(const image* mask, int x0, int y0, color text_color)` 
        composite a LaTeX mask image onto the frame using alpha from the mask.
- `latex_to_bmp(const char* latex, const char* bmp_path)` 
        helper that renders LaTeX (via external tools) to a BMP (uses `node` + ImageMagick).
- `write_latex(const char* latex, int x, int y, color c)` 
        render LaTeX and blit it into the frame at `(x,y)` using color `c`.
- `save_frame(void)` 
        save the current frame to `video/frame_XXXXX.bmp`.
- `video_end(void)` 
        free frame buffers and encode frames into `render.mp4` using `ffmpeg`.