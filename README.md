# Pembroke
Pembroke is a single file C library for creating and animating math videos.
# Dependencies
* Node.js
* MathJax
* ImageMagick
# Usage
The following is an minimal example of creating a video with Pembroke.
```c
#define PEMBROKE_IMPLEMENTATION
#include "pembroke.h"

int main() {
	/* 
	   Initialize a video with a width of 620 pixels, 
	   a height of 480 pixels,
	   and a frame rate of 60 frames per second.
	*/
	
	video_init(620, 480, 60);
	for (int i = 0; i < 60; i++) {
		// clear frame with white background
		clear_frame(255);

		// draw LaTeX math formula at the center of the frame
		write_latex("\\frac{dy}{dx}e^x = e^x", 310, 240);

		// save frame as a BMP image
		save_frame();
	}

	// save video as "render.mp4"
	video_end();
}