#version 120

varying vec3 vPosEye; 
varying vec3 vNorEye; 

uniform vec3 ke;
uniform vec3 kd;

void main() {
    gl_FragData[0].xyz = vPosEye;           // Position Texture
    gl_FragData[1].xyz = normalize(vNorEye); // Normal Texture
    gl_FragData[2].xyz = ke;                // Emissive Texture
    gl_FragData[3].xyz = kd;                // Diffuse Texture
}