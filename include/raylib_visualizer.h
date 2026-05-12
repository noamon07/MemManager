#ifndef RAYLIB_VISUALIZER_H
#define RAYLIB_VISUALIZER_H

/**
 * @brief Launches the graphical memory visualization window.
 * 
 * This function is blocking and enters the main rendering loop. 
 * It initializes the OpenGL context, sets up the camera/viewports, 
 * and begins drawing the state of the Nursery, General Arena, 
 * and Handle Table until the window is closed.
 * 
 * Typically called from a dedicated thread or at the end of 'main' 
 * to allow the memory manager tests to run concurrently or under 
 * visual supervision.
 */
void RunMemoryVisualizer();

#endif /* RAYLIB_VISUALIZER_H */