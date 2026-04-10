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

## API Reference

Below are the main API types and functions provided by `pembroke.h` with a short description and usage notes.

- `typedef struct color` — RGB color container with byte channels `r`, `g`, `b`.
- `typedef void (*pixel_shader)(int x, int y, color* rgb, void* userdata)` — callback signature for `foreach_pixel`.

- `extern int VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS` — global video settings (updated by `video_init`).
- `extern unsigned char** VIDEO_FRAME` — pointer to the frame pixel rows (internal backing store; rows are BGR triples).

- `video_init(int width, int height, int fps)` — Initialize the video system, allocate frame buffers and create `video/` directory. Must be called before rendering or calling other APIs.

- `save_frame(void)` — Write the current in-memory frame to a BMP file in the `video/` folder. Filename uses a frame counter.

- `video_end(void)` — Free frame buffers, call `ffmpeg` to encode `video/frame_XXXXX.bmp` into `render.mp4`, and remove the temporary `video/` directory.

- `clear_frame(unsigned char gray)` — Fast clear of the framebuffer to a grayscale value (each channel set to `gray`).

- `fill_frame(color c)` — Fill the entire frame with a solid `color`.

- `set_pixel(int x, int y, color c)` — Set a single pixel at `(x,y)` to `c`. Bounds-checked.

- `fill_circle(int cx, int cy, int radius, color c)` — Draw a filled circle (uses integer radius and scan conversion).

- `draw_line(int x0, int y0, int x1, int y1, color c)` — Bresenham-style line rasterization between two points.

- `draw_polygon(int* x, int* y, int amt, color c)` — Draw polygon outline connecting `amt` points stored in the `x` and `y` arrays.

- `fill_tri(int* x, int* y, color c)` — Fill a triangle specified by three points. Uses a scanline rasterizer (with SIMD-accelerated span writes where available) for speed.

- `fill_rect(int cx, int cy, int w, int h, color c)` — Draw a filled axis-aligned rectangle with top-left at `(cx,cy)`.

- `foreach_pixel(pixel_shader fn, void* userdata)` — Iterate every pixel in the frame and invoke `fn(x,y,&rgb,userdata)` to compute a color; results are written into the frame.

- `progress(int video_frames)` — Print a simple progress message (frame count and elapsed/total time) to stdout during rendering.

- `write_latex(const char* latex, int x, int y, color c)` — Render a LaTeX string into an image (via the included `render.js`/MathJax + ImageMagick pipeline) and composite it onto the current frame at `(x,y)` using the provided text color `c`.

- `latex_to_bmp(const char* latex, const char* bmp_path)` — Helper to render LaTeX to a BMP file (used internally by `write_latex`).

- `typedef struct image { int w, int h; unsigned char* pixels; }` — Simple RGB image container (pixels are BGR triples matching the frame layout).
- `image load_bmp(const char* filename)` — Load a 24-bit BMP from disk into an `image` struct. Caller must `free(image.pixels)` when done.

- `blit_image(image* img, int dst_x, int dst_y)` — Copy `img` pixels into the current frame at `(dst_x,dst_y)` (clipped to frame bounds).

- `blit_latex_mask(const image* mask, int x0, int y0, color text_color)` — Composite a mask image (generated from LaTeX) on top of the current frame: uses the mask's alpha channel to blend `text_color` over the background.

- `graph_rpn(const char* rpn_src, int weight, color rgb)` — Parse an RPN expression string and draw its graph across the frame horizontally with point weight `weight` and color `rgb`.

- `rpn_parse`, `rpn_eval`, `graph_rpn_struct` — Lower-level RPN helpers exposed in the header for parsing and evaluating reverse-polish expressions (used by `graph_rpn`).

# Notes
- The library uses the BGR byte order for frame rows to match BMP pixel layout. When constructing `color` values by hand, use `{ r, g, b }` order but remember internal storage is written as B,G,R.
- `write_latex` depends on `node`, `MathJax` and `ImageMagick` being available on PATH. `video_end` calls `ffmpeg` to encode the frames.
- Many helper functions (file/dir utilities, BMP writer, low-level internals) are defined in the header — but the functions listed above are the primary APIs for drawing and rendering.
