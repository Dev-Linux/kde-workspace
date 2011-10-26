uniform sampler2D sampler;
uniform vec4 modulation;
uniform float saturation;
uniform int debug;
uniform int u_forceAlpha;

varying vec2 varyingTexCoords;

//varying vec4 color;

void main() {
    vec4 tex = texture2D(sampler, varyingTexCoords);

    tex.a += float(u_forceAlpha);

    if( saturation != 1.0 ) {
        vec3 desaturated = tex.rgb * vec3( 0.30, 0.59, 0.11 );
        desaturated = vec3( dot( desaturated, tex.rgb ));
        tex.rgb = tex.rgb * vec3( saturation ) + desaturated * vec3( 1.0 - saturation );
    }

    tex *= modulation;

    /*if (debug != 0) {
        tex.g += 0.5;
    }*/

    gl_FragColor = tex;
}
