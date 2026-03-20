#version 460 core
layout (location = 0) out vec4 FragOut;

in vec4 v_normal;
in vec4 v_directLighting;
in vec2 v_uv;

void main() {
    discard;

    if (v_directLighting.rgb == vec3(0,0,0)) {
       //FragOut.rgb = vec3(1,0,0);;
       //return;
       discard;
    }    
    else {
        FragOut.rgb = v_directLighting.rgb;
    }
    
	FragOut.a = 1.0;
}


void main2() {
    vec2 wrappedUV = fract(v_uv); 
    FragOut = vec4(wrappedUV, 0.0, 1.0);
}
