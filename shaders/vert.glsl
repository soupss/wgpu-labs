#version 330 core


void main()
{
 
    // Define the three positions of a triangle in Clip Space (-1.0 to 1.0)
    // These positions form a triangle that covers the entire screen,
    // which is often useful for simple 2D drawing or screen-space effects.
    int index = gl_VertexIndex;

    if (index == 0) {
        gl_Position = vec4(1,1,1, 1.0);
    }
    else if (index == 1) {
        gl_Position =vec4(0,0.5,1, 1.0);
    }
    else {
        gl_Position = vec4(1,0,1, 1.0);
    }
    
    // Retrieve the position based on the current vertex index
    
    // Assign the final position to the built-in output variable
    // The Z component is 0.0, and the W component must be 1.0 for Clip Space.
}