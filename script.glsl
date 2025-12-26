// NOTE: GLSL header in/outs are defined in openglcanvas.cpp
// including:
//     uniform vec2 iResolution;
//     uniform float iTime;
//     out vec4 FragColor;  

void mainImage( out vec4 fragColor, in vec2 fragCoord );

void main()
{
    mainImage(FragColor, gl_FragCoord.xy);      
}
const vec3 BLACK = vec3(0.0);
const vec3 WHITE = vec3(1.0);
const vec3 RED = vec3(1.0, 0, 0);
const vec3 GREEN = vec3(0, 1.0f, 0);

const vec3 BACKGROUND = vec3(0.968);

float vertLine(vec2 uv, float xPos) {
    const float WIDTH = 0.002f;
    return step(xPos-WIDTH, uv.x) * step(uv.x, xPos+WIDTH);
}

float hortLine(vec2 uv, float yPos) {
    const float WIDTH = 0.002f;
    return step(yPos-WIDTH, uv.y) * step(uv.y, yPos+WIDTH);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Assume that the aspect ratio (x/y) is >1
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.y;
    // move the middle of the
    float adjustment = (iResolution.x-iResolution.y)/(2.0 * iResolution.y);
    uv.x -= adjustment;
    uv -= vec2(0.5);
    
    vec2 CENTER = vec2(0);
    float RADIUS = 0.5;
    float HALF_EDGE_MAG = 0.01;
    
    float dist = distance(uv, CENTER);
    
    // piecewise function that goes 0->1->0
    float circleEdge = step(-dist, -(RADIUS-HALF_EDGE_MAG)) * step(dist, RADIUS+HALF_EDGE_MAG);
    
    // clear to background
    vec3 col = BACKGROUND;
    
    // Time varying pixel color
    vec3 circleColor = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));
    col = mix(col, circleColor, circleEdge);
    
    // outline the region from (-0.5,-0.5) -> (0.5, 0.5)
    float vert0 = vertLine(uv, -0.5); 
    float vert1 = vertLine(uv, 0.5);
    float hort0 = hortLine(uv, -0.5);
    float hort1 = hortLine(uv, 0.5);
    col = mix(col, RED, vert0 + vert1);
    col = mix(col, GREEN, hort0 + hort1);

    // Output to screen
    fragColor = vec4(col,1.0);    
}

