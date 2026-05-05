# Pembroke
Pembroke is a single file C library for creating and animating math videos.

# Dependencies
* Node.js - for running MathJax
* MathJax - for rendering LaTeX math formulas to SVG
<<<<<<< HEAD
* Librsvg - for converting SVG to BMP
* FFmpeg (included for Windows) - for encoding BMP frames into a video
=======
* librsvg - for converting SVG to PNG
* stb_image.h (included) - for loading PNG images
* FFmpeg (included for windows) - for encoding BMP frames into a video
>>>>>>> 84be9ae (added more image support)

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
<<<<<<< HEAD
        fill_frame((color){ 255, 255, 255 });

        // Draw LaTeX math formula at the top-left corner
        write_latex("\\frac{dy}{dx} e^x = e^x", 0, 0, 100, 0.0f, (color){0, 0, 0});
=======
        fill_frame((color){255, 255, 255});

        // Draw LaTeX math formula at the top-left corner
        write_latex("\\frac{dy}{dx} e^x = e^x", 0, 0, 1.0f, 0.0f, (color){0, 0, 0});
>>>>>>> 84be9ae (added more image support)

        // Pipe frame into FFmpeg
        save_frame();
    }

    // Deinitialize video
    video_end();
}
```