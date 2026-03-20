#version 460 core
layout (location = 0) out vec4 FragOut;

in vec4 v_normal;
in vec4 v_directLighting;
in vec2 v_uv;
in vec3 v_baseColor;

void main() {
    if (v_directLighting.rgb == vec3(0,0,0)) {
        //FragOut.rgba = vec4(1.0, 0.0, 0.0, 1.0);
        discard;
    }
    else {
        FragOut.rgba = vec4(v_directLighting.rgb, 1.0);
    }
}

void main2() {
    vec2 wrappedUV = fract(v_uv);
    FragOut = vec4(wrappedUV, 0.0, 1.0);
}

void m55ain() {
    FragOut.rgb = v_baseColor;
	FragOut.a = 1.0;
}