precision highp float;
attribute vec3 a_position;
attribute vec3 a_normal;
varying vec3 v_normal;
uniform mat3 u_normalMatrix;
uniform mat4 u_modelViewMatrix;
uniform mat4 u_projectionMatrix;


//pos
varying vec3 pos_fs;

float Noise3D(int x, int y, int z)
{
    float ft = fract(sin(dot(vec3(x,y,z), vec3(12.989, 78.233, 157))) * 43758.5453);
    //int a = int(ft);
    return ft;
}


float SmoothNoise3D(int X, int Y, int Z)
{
    float far = (Noise3D(X-1, Y+1, Z+1) + Noise3D(X+1, Y+1, Z+1) + Noise3D(X-1, Y+1, Z-1) + Noise3D(X+1, Y+1, Z-1) + Noise3D(X-1, Y-1, Z+1) + Noise3D(X+1, Y-1, Z+1) + Noise3D(X-1, Y-1, Z-1) + Noise3D(X+1, Y-1, Z-1)) / 64.0;//80.0;

    float medium = (Noise3D(X-1, Y+1, Z) + Noise3D(X+1, Y+1, Z) + Noise3D(X-1, Y-1, Z) + Noise3D(X+1, Y-1, Z) + Noise3D(X, Y+1, Z+1) + Noise3D(X, Y+1, Z-1) + Noise3D(X, Y-1, Z+1) + Noise3D(X, Y-1, Z-1) + Noise3D(X-1, Y, Z+1) + Noise3D(X+1, Y, Z+1) + Noise3D(X-1, Y, Z-1) + Noise3D(X+1, Y, Z-1)) / 32.0;//60.0;

    float closest = (Noise3D(X-1, Y, Z) + Noise3D(X+1, Y, Z) + Noise3D(X, Y-1, Z) + Noise3D(X, Y+1, Z) + Noise3D(X, Y, Z+1) + Noise3D(X, Y, Z-1)) / 16.0;//19.999;
    
    float self = Noise3D(X, Y, Z) / 4.0;
    
    
    return self + closest + medium + far;  
}


float Interpolate(float a, float b, float x)
{
    float t = (1.0 - cos(x * 3.14159)) * 0.5;
    
    return a * (1.0 - t) + b * t;
}

float InterpolateNoise3D(float x, float y, float z)
{
    int int_X = int(x);
    int int_Y = int(y);
    int int_Z = int(z);
    
    float float_X = x - float(int_X);
    float float_Y = y - float(int_Y);
    float float_Z = z - float(int_Z);
    
    //8 Points on the lattice sorrunding the given point
    float p1 = SmoothNoise3D(int_X, int_Y, int_Z);
    float p2 = SmoothNoise3D(int_X + 1, int_Y, int_Z);
    float p3 = SmoothNoise3D(int_X, int_Y + 1, int_Z);
    float p4 = SmoothNoise3D(int_X + 1, int_Y + 1, int_Z);
    float p5 = SmoothNoise3D(int_X, int_Y, int_Z + 1);
    float p6 = SmoothNoise3D(int_X + 1, int_Y, int_Z + 1);
    float p7 = SmoothNoise3D(int_X, int_Y + 1, int_Z + 1);
    float p8 = SmoothNoise3D(int_X + 1, int_Y + 1, int_Z + 1);
    
    float i1 = Interpolate(p1, p2, float_X);
    float i2 = Interpolate(p3, p4, float_X);
    float i3 = Interpolate(p5, p6, float_X);
    float i4 = Interpolate(p7, p8, float_X);
    
    float n1 = Interpolate(i1, i2, float_Y);
    float n2 = Interpolate(i3, i4, float_Y);
    
    float t1 = Interpolate(n1, n2, float_Z);
    
    return t1;
}


float Generate_Noise3D(vec3 pos, float persistance, int octaves)
{
    float total = 0.0;
    float p = persistance;
    int n = octaves;

    //int i = 0;
    for(int i=0; i < 8; i++) 
    {
    float frequency = pow(float(2), float(i)) / 12000000.0;
    float amplitude = pow(p, float(i));
    
    total = total + InterpolateNoise3D((pos.x)* frequency, (pos.y) * frequency, (pos.z) * frequency) * amplitude;
    
    }
    
    return total;
}


void main(void) {

    //generate noise

    //vec4 pos = u_modelViewMatrix * vec4(new_pos,1.0);
    vec4 pos = u_modelViewMatrix * vec4(a_position,1.0);
    v_normal = u_normalMatrix * a_normal; 

    float noise = Generate_Noise3D(vec3(a_position), 0.9, 10);
    // noise = Noise3D(int(a_position.x), int(a_position.y), int(a_position.z));
    
    //vec3 new_pos = a_position * 0.5 + v_normal * noise * 0.5;
    
    //pos = u_modelViewMatrix * vec4(new_pos,1.0);
    
    //vec4 new_pos = pos - vec4(a_normal,0.0) * noise * 1000000.0;//noise;
    
    //vec4 new_pos = pos * 0.5;
    
    //pos = pos + vec4(v_normal,1.0) * 10.0;
    //gl_Position = u_projectionMatrix * pos;
    
    
    //vec3 new_pos = a_position*0.5;//(noise * 0.5 + 0.5) * a_normal * 10000.0;
    
    vec3 temp_a_pos = a_position;
    vec3 scale_vec = temp_a_pos * v_normal;
    temp_a_pos = (temp_a_pos + a_normal * noise * 1000000.0);// * 1000000.0);
    
    vec4 new_pos = u_modelViewMatrix * vec4(temp_a_pos,1.0);
    
    //new_pos[1] /= 2.0;
    
    pos_fs = a_position;
    gl_Position = u_projectionMatrix * new_pos;

    

}