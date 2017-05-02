precision highp float;
varying vec3 v_normal;
uniform vec4 u_diffuse;

//pos
varying vec3 pos_fs;

void main(void) {
    vec3 normal = normalize(v_normal);
    vec4 color = vec4(0., 0., 0., 0.);
    vec4 diffuse = vec4(0., 0., 0., 1.);
    diffuse = u_diffuse;
    diffuse.xyz *= max(dot(normal,vec3(0.,0.,1.)), 0.);
    color.xyz += diffuse.xyz;
    color = vec4(color.rgb * diffuse.a, diffuse.a);
    
    gl_FragColor = color;
    
    //vec3 p = abs(normalize(pos_fs));
    //gl_FragColor = vec4( v_normal, 0.0);
    //gl_FragColor = vec4( p, 0.0);
}