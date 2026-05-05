# Pembroke
Pembroke is a single file C library for creating and animating math videos.

# Dependencies
* Mathjax and Node.js - for LaTeX to SVG
* Librsvg - for converting SVG to PNG
* FFmpeg (included for Windows) - for piping frames into a video
* stb_image.h (included) - for loading images

# Usage
The following is an minimal example of creating a video with Pembroke.

```c
#define PEMBROKE_IMPLEMENTATION
#include "pembroke.h"

int main() {
    // Initialize a video:
    // width = 620 px, height = 480 px, framerate = 60 fps, processing = fast
    video_init(620, 480, 60, true);

    for (int i = 0; i < 60; i++) {
        // Clear frame with white background
        fill_frame((color){ 255, 255, 255 });

        // Draw LaTeX math formula at the top-left corner
        write_latex("\\frac{dy}{dx}e^x=e^x", 0, 0, 100, 0.0f, (color){0, 0, 0});

        // Pipe frame into FFmpeg
        save_frame();
    }

    // Deinitialize video
    video_end();
}
```