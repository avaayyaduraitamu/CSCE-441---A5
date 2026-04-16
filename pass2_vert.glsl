#version 120

uniform sampler2D posTexture;
uniform sampler2D norTexture;
uniform sampler2D keTexture;
uniform sampler2D kdTexture;

uniform vec2 windowSize;
uniform int mode;
uniform int blurOn; // <--- FIXED: Added missing uniform

uniform vec3 lightPos[10];
uniform vec3 lightCol[10];
uniform int numLights;

// Poisson Disk for blurring
vec2 poissonDisk[] = vec2[](
    vec2(-0.220147, 0.976896), vec2(-0.735514, 0.693436),
    vec2(-0.200476, 0.310353), vec2( 0.180822, 0.454146),
    vec2( 0.292754, 0.937414), vec2( 0.564255, 0.207879),
    vec2( 0.178031, 0.024583), vec2( 0.613912,-0.205936),
    vec2(-0.385540,-0.070092), vec2( 0.962838, 0.378319),
    vec2(-0.886362, 0.032122), vec2(-0.466531,-0.741458),
    vec2( 0.006773,-0.574796), vec2(-0.739828,-0.410584),
    vec2( 0.590785,-0.697557), vec2(-0.081436,-0.963262),
    vec2( 1.000000,-0.100160), vec2( 0.622430, 0.680868)
);

vec3 sampleTextureArea(sampler2D tex, vec2 tex0) {
    const int N = 18; 
    const float blur = 0.005;
    vec3 val = vec3(0.0);
    for(int i = 0; i < N; i++) {
        val += texture2D(tex, tex0.xy + poissonDisk[i] * blur).rgb;
    }
    val /= float(N);
    return val;
}

void main() {
    vec2 texCoords = gl_FragCoord.xy / windowSize;
    vec3 pos, nor, ke, kd;

    if (blurOn == 1) {
        pos = sampleTextureArea(posTexture, texCoords);
        nor = normalize(sampleTextureArea(norTexture, texCoords)); 
        ke  = sampleTextureArea(keTexture, texCoords);
        kd  = sampleTextureArea(kdTexture, texCoords);
    } else {
        pos = texture2D(posTexture, texCoords).rgb;
        nor = normalize(texture2D(norTexture, texCoords).rgb);
        ke  = texture2D(keTexture, texCoords).rgb;
        kd  = texture2D(kdTexture, texCoords).rgb;
    }

    if (mode == 1) { gl_FragColor = vec4(pos, 1.0); return; }
    if (mode == 2) { gl_FragColor = vec4(nor, 1.0); return; }
    if (mode == 3) { gl_FragColor = vec4(kd, 1.0);  return; }
    if (mode == 4) { gl_FragColor = vec4(ke, 1.0);  return; }

    vec3 n = normalize(nor);
    vec3 v = normalize(-pos); // Defined 'v' here
    vec3 lighting = ke; 

    for(int i = 0; i < numLights; i++) {
        vec3 L_vec = lightPos[i] - pos; 
        float d = length(L_vec);
        vec3 l = normalize(L_vec);
        
        // Removed 'vec3 v' from here because it's already defined above!
        vec3 h = normalize(l + v);

        float a0 = 1.0;
        float a1 = 0.0429;
        float a2 = 0.9857;
        float atten = 1.0 / (a0 + a1 * d + a2 * d * d);

        vec3 boostedLight = lightCol[i] * 3.0; 

        float diff = max(dot(l, n), 0.0);
        float spec = pow(max(dot(h, n), 0.0), 100.0);

        lighting += (kd * boostedLight * diff + vec3(1.0) * boostedLight * spec) * atten;
    }

    gl_FragColor = vec4(lighting, 1.0);
}