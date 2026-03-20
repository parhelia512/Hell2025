#version 460 core

layout (location = 0) out vec4 FragOut;

// Your SoA Buffer
layout(std430, binding = 6) buffer ProbeBuffer { vec4 sh_data[]; };

flat in int v_probeIndex;
flat in ivec3 v_voxelCoord;
in vec3 v_worldPos;
in vec3 v_normal;

uniform int u_probeCount;

// SH Constants for reconstruction
const float SH_C0 = 0.28209479177;
const float SH_C1 = 0.4886025119;
const float SH_C2 = 1.09254843059;
const float SH_C3 = 0.31539156525;
const float SH_C4 = 0.54627421529;

void main() {
    int probeIdx = v_probeIndex;
    int baseIdx = probeIdx * 9; // Start of this probe's memory block

    // Reconstruct the SH basis functions for the current surface normal
    float n[9];
    vec3 dir = normalize(v_normal);

    n[0] = SH_C0;
    n[1] = -SH_C1 * dir.y;
    n[2] =  SH_C1 * dir.z;
    n[3] = -SH_C1 * dir.x;
    n[4] =  SH_C2 * dir.x * dir.y;
    n[5] = -SH_C2 * dir.y * dir.z;
    n[6] =  SH_C3 * (3.0 * dir.z * dir.z - 1.0);
    n[7] = -SH_C2 * dir.x * dir.z;
    n[8] =  SH_C4 * (dir.x * dir.x - dir.y * dir.y);

    // Accumulate RGB by multiplying coefficients by the basis
    vec3 irradiance = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        vec3 coeff = sh_data[baseIdx + i].rgb;
        irradiance += coeff * n[i];
    }

    irradiance = max(vec3(0.0), irradiance);
    FragOut = vec4(irradiance, 1.0);


    //float value = sh_data[baseIdx].w;
    //FragOut = vec4(vec3(value), 1.0);

}