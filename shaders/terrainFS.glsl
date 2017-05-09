precision highp float;
varying vec3 v_normal;
uniform vec4 u_diffuse;
varying vec3 noisefs;

// THIS IS THE SHADER WHICH WILL PROVIDE THE TEXTURE TO THE TERRAIN.
// VALUE NOISE IS THE NOISE GENERATOR USED HERE (THE TERRAIN USES RIDGED VERSION). IT IS A MODIFICATION OF IQ'S VALUE NOISE GENERATOR.
// DERIVATIVES OF THE NOISE ARE USED TO SIMULATE EROSION EFFECTS.
// CHANGING THE SPEED IN THE GUI WILL ANIMATE THE NOISE AND EROSION [FOR VISUALIZATION].

//varying vec2 vUv;
varying vec3 pos;

//uniform float time;
//uniform int NoiseType;
//uniform bool Preset1;

// NOISE FUNCTIONS FROM:
//      http://www.science-and-fiction.org/rendering/noise.html
float rand2D(in vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
float rand3D(in vec3 co){
    return fract(sin(dot(co.xyz ,vec3(12.9898,78.233,144.7272))) * 43758.5453);
}

// iq's value noise algorithm:
vec4 noised( in vec3 x )
{
    vec3 p = floor(x);
    vec3 w = fract(x);

    vec3 u = w*w*w*(w*(w*6.0-15.0)+10.0);
    vec3 du = 30.0*w*w*(w*(w-2.0)+1.0);

    float a = rand3D( p+vec3(0,0,0) );
    float b = rand3D( p+vec3(1,0,0) );
    float c = rand3D( p+vec3(0,1,0) );
    float d = rand3D( p+vec3(1,1,0) );
    float e = rand3D( p+vec3(0,0,1) );
    float f = rand3D( p+vec3(1,0,1) );
    float g = rand3D( p+vec3(0,1,1) );
    float h = rand3D( p+vec3(1,1,1) );

    float k0 =   a;
    float k1 =   b - a;
    float k2 =   c - a;
    float k3 =   e - a;
    float k4 =   a - b - c + d;
    float k5 =   a - c - e + g;
    float k6 =   a - b - e + f;
    float k7 = - a + b + c - d + e - f - g + h;

    return vec4( -1.0+2.0*(k0 + k1*u.x + k2*u.y + k3*u.z + k4*u.x*u.y + k5*u.y*u.z + k6*u.z*u.x + k7*u.x*u.y*u.z),
                  2.0* du * vec3( k1 + k4*u.y + k6*u.z + k7*u.y*u.z,
                                  k2 + k5*u.z + k4*u.x + k7*u.z*u.x,
                                  k3 + k6*u.x + k5*u.y + k7*u.x*u.y ) );
}


const mat3 m3  = mat3( 0.00, 0.80, 0.60,
                      -0.80, 0.36,-0.48,
                      -0.60,-0.48, 0.64 );
const mat3 m3i = mat3( 0.00,-0.80,-0.60,
                       0.80, 0.36,-0.48,
                       0.60,-0.48, 0.64 );
// FBM - PROCESSING FOR OCTAVES AND ACCUMULATING DERIVATIVES
vec4 fbm( in vec3 x)
{
    float f = 2.0;  // could be 2.0
    float s = 0.5;  // could be 0.5
    float a = 0.0;
    float b = 0.5;
    vec3  d = vec3(0.0);
    mat3  m  = mat3( 1.00, 0.00, 0.00,
                   0.00, 1.00, 0.00,
                   0.00, 0.00, 1.00);
    //int oct = octaves;
    for( int i=0; i < 10; i++ ) // octaves = 8..
    {
        vec4 n = noised(x);
		//n = 1.0 - abs(n);
        a += b*n.x;          // accumulate values
        d += b*m*n.yzw;      // accumulate derivatives
        b *= s;
        x = f*m3*x;
        m = f*m3i*m;
    }
	return vec4( a, d );
}

vec2 voronoi( in vec3 x)
{
    vec3 p = floor( x );
    vec3 f = fract( x );

//	float id = 0.0;
    vec2 res = vec2( 8.0 );
    for( int k=-1; k<=1; k++ )
        for( int j=-1; j<=1; j++ )
            for( int i=-1; i<=1; i++ )
            {
                vec3 b = vec3( float(i), float(j), float(k) );
                vec3 r = vec3( b ) - f + rand3D( p + b );
                float d = dot( r, r );

                if( d < res.x )
                {
        			//id = dot( p+b , vec3(1.0,57.0,113.0 ) );
                    res = vec2( d, res.x );
                }
                else if( d < res.y )
                {
                    res.y = d;
                }
            }

    //return pow( 1.0/res, 1.0/16.0 );
    return vec2(sqrt(res));
}

void main()
{
    vec4 n = fbm(pos);
    vec2 voro = voronoi((200.0*pos)); //90

    // SMOOTH VORONOI
    //float voro = voronoi((5.0*pos));

    float nfs = noisefs[0]*0.5+0.5;

    vec3 col;
    vec3 colBase;
    vec3 colSnow;
    vec3 colForest;

    //float vn = voro;
    float vn = (1.0+(sin((n[0])*10.0)/2.0))*voro[1]*voro[0];//smoothstep(0.0,0.9,voro[0]);

    // BASE
    colBase = mix(vec3(0.1,0.1,0.1),vec3(0.45,0.35,0.3),vn);
    colBase = mix(colBase,vec3(0.15,0.15,0.15),n[0]*nfs);

    // FOREST
    colForest = mix(vec3(0.1,0.15,0.05),vec3(0.15,0.20,0.13),vn);// mix(vec3(0.1,0.15,0.05),vec3(0.15,0.20,0.13),vn);
    colForest = mix(colForest,colBase,n[0]*nfs);
    colBase = mix(colForest,colBase,0.5+0.5*n[0]*nfs);

    // SNOW
    colSnow = vec3(1.,0.98,0.98); // SNOW: 255,250,250
    //if((noisefs[0])>0.5)
    col = mix(colBase,colSnow,(nfs-3.0*n[0]-2.0*n[1]-n[2])/10.0);

    col = max(colBase,col); // CLAMPING LOWEST VALUES TO BASE COLOR


    // DIFFUSE LIGHTING
    vec4 color = vec4(col, 0.);
    vec4 diffuse = vec4(0., 0., 0., 1.);
    diffuse = vec4(0.95, 0.95, 0.95, 1.);//u_diffuse;
    vec3 normal = normalize(v_normal);
    diffuse.xyz *= max(dot(v_normal,normalize(vec3(1.,1.,2.))), 0.05);
    color.xyz *= diffuse.xyz;
    color = vec4(color.rgb * diffuse.a, diffuse.a);
    //gl_FragColor = color;

    //col = sqrt(col); // gamma correction

    float gamma = 2.2;
    //fragColor.rgb = pow(col.rgb, vec3(1.0/gamma));

    gl_FragColor = vec4( pow(color.rgb, vec3(1.0/gamma)) , 1.0 );
}
/*
void main(void) {
    vec3 normal = normalize(v_normal);
    vec4 color = vec4(0., 0., 0., 0.);
    vec4 diffuse = vec4(0., 0., 0., 1.);
    diffuse = u_diffuse;
    diffuse.xyz *= max(dot(normal,vec3(0.,0.,1.)), 0.);
    color.xyz += diffuse.xyz;
    color = vec4(color.rgb * diffuse.a, diffuse.a);
    gl_FragColor = color;
}*/