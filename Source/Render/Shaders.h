#pragma once
// All GLSL for the renderer. GLSL 1.50 (OpenGL 3.2 core).

namespace au::shaders
{
// ------------------------------------------------------------------ fullscreen triangle
inline const char* fullscreenVS = R"(#version 150
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    vUv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
})";

// ------------------------------------------------------------------ shared helpers
inline const char* prelude = R"(#version 150
in vec2 vUv;
out vec4 frag;
uniform vec2 uRes;
uniform float uTime, uBass, uMid, uTreble, uBeat, uLevel, uBeatPhase;
uniform float uHue, uSat, uInt, uSpeed, uReact, uGain, uSize, uThick;
uniform float uBands[64];
const float PI = 3.14159265;
vec3 hsv(float h, float s, float v) {
    vec3 k = clamp(abs(fract(h + vec3(0.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
    return v * mix(vec3(1.0), k, s);
}
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + 17.31)); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1,0)), u.x), mix(hash(i + vec2(0,1)), hash(i + vec2(1,1)), u.x), u.y);
}
float fbm(vec2 p) { float a = 0.5, s = 0.0; mat2 m = mat2(1.6, 1.2, -1.2, 1.6); for (int i = 0; i < 5; ++i) { s += a * noise(p); p = m * p; a *= 0.5; } return s; }
float band(float x) { x = clamp(x, 0.0, 1.0) * 63.0; int i = int(x); float f = fract(x); return mix(uBands[i], uBands[min(i + 1, 63)], f); }
vec2 P() { return (gl_FragCoord.xy - 0.5 * uRes) / min(uRes.x, uRes.y); }
float stars(vec2 p, float dens, float t) {
    vec2 g = floor(p * dens); vec2 f = fract(p * dens) - 0.5;
    float h = hash(g); vec2 o = hash2(g) - 0.5;
    float d = length(f - o * 0.6);
    float tw = 0.6 + 0.4 * sin(t * (1.0 + 3.0 * h) + h * 40.0);
    return step(0.92, h) * smoothstep(0.08, 0.0, d) * tw * (h - 0.92) * 12.0;
}
)";

// ------------------------------------------------------------------ backgrounds
inline const char* bgLake = R"(
vec3 sky(vec2 p, float t) {
    vec3 top = hsv(uHue, uSat * 0.85, 0.035);
    vec3 hor = hsv(uHue + 0.03, uSat * 0.6, 0.10 + 0.16 * uBass * uReact);
    vec3 c = mix(hor, top, smoothstep(-0.05, 0.55, p.y));
    float n = fbm(vec2(p.x * 1.4 + t * 0.03, p.y * 3.5 - t * 0.01));
    c += hsv(uHue + 0.05, uSat * 0.5, 0.10) * n * smoothstep(0.0, 0.45, p.y);
    float d = length(p - vec2(0.0, 0.22));
    c += hsv(uHue + 0.04, uSat * 0.45, 1.0) * (0.004 / (d * d + 0.012)) * (0.25 + 0.75 * uLevel);
    c += vec3(stars(p, 70.0, t * 2.0)) * smoothstep(0.02, 0.35, p.y);
    return c;
}
void main() {
    vec2 p = P();
    float t = uTime * uSpeed;
    float hz = -0.06;
    vec3 c;
    if (p.y > hz) c = sky(p, t);
    else {
        float depth = hz - p.y;
        float persp = 0.04 / (depth + 0.04);
        vec2 wp = vec2(p.x * persp * 6.0, persp * 2.0 + t * 0.4);
        float w = fbm(wp * 1.5) - 0.5;
        float r = length(vec2(p.x, (p.y - hz) * 3.2));
        float ring = sin(r * 55.0 - uTime * 5.0) * exp(-r * 2.5) * (0.25 + uBeat * uReact) * 0.02;
        float dist = w * 0.05 * (depth + 0.05) * 3.0 + ring;
        vec2 rp = vec2(p.x + dist * 0.6, 2.0 * hz - p.y + dist);
        c = sky(rp, t) * (0.55 + 0.25 * smoothstep(0.4, 0.0, depth));
        float spec = smoothstep(0.08, 0.0, abs(p.x + dist * 1.5)) * smoothstep(0.5, 0.0, depth);
        c += hsv(uHue + 0.04, uSat * 0.4, 0.3) * spec * (0.3 + uLevel);
        c *= 0.55 + 0.45 * smoothstep(0.7, 0.0, depth);
        c += hsv(uHue, uSat, 0.08) * smoothstep(0.012, 0.0, abs(p.y - hz)) * (1.0 + uBass);
    }
    frag = vec4(c * uInt * 1.25, 1.0);
}
)";

inline const char* bgNebula = R"(
void main() {
    vec2 p = P();
    float t = uTime * uSpeed * 0.25;
    float zoom = 1.0 - 0.06 * uBeat * uReact - 0.04 * uBass * uReact;
    vec2 q = p * zoom * 1.6;
    float a = t * 0.15; q = mat2(cos(a), -sin(a), sin(a), cos(a)) * q;
    vec2 w1 = vec2(fbm(q + vec2(0.0, t)), fbm(q + vec2(5.2, 1.3 - t)));
    vec2 w2 = vec2(fbm(q + 3.0 * w1 + vec2(1.7, 9.2) + t * 0.5), fbm(q + 3.0 * w1 + vec2(8.3, 2.8)));
    float f = fbm(q + 3.5 * w2);
    vec3 c1 = hsv(uHue, uSat, 0.9), c2 = hsv(uHue + 0.12, uSat * 0.9, 0.7), c3 = hsv(uHue - 0.1, uSat * 0.8, 0.35);
    vec3 col = mix(c3 * 0.15, c1, smoothstep(0.35, 0.95, f));
    col = mix(col, c2, clamp(length(w2) * 0.6, 0.0, 1.0) * 0.6);
    col *= f * f * (1.4 + 1.6 * uMid * uReact);
    float core = exp(-dot(p, p) * 5.0);
    col += hsv(uHue + 0.05, uSat * 0.4, 1.0) * core * (0.12 + 0.5 * uBass * uReact);
    col += vec3(stars(p * 1.1 + t * 0.02, 90.0, uTime * 2.0)) * 0.9;
    col += vec3(stars(p * 0.7 - t * 0.01, 45.0, uTime * 1.3)) * 0.6;
    frag = vec4(col * uInt, 1.0);
}
)";

inline const char* bgFlight = R"(
void main() {
    vec2 p = P();
    float t = uTime * uSpeed * (0.6 + 0.8 * uLevel);
    vec3 col = hsv(uHue, uSat, 0.03) + hsv(uHue + 0.1, uSat * 0.8, 0.12) * exp(-dot(p, p) * 3.0) * (0.6 + uBass * uReact);
    float streak = 0.4 + 2.5 * uBass * uReact + 1.5 * uBeat * uReact;
    for (int i = 0; i < 8; ++i) {
        float fi = float(i);
        float z = fract(fi / 8.0 + uTime * uSpeed * 0.18 * (1.0 + uBass * uReact));
        float scale = mix(9.0, 0.4, z);
        vec2 q = p * scale + vec2(fi * 17.3, fi * 9.1);
        vec2 g = floor(q); vec2 f = fract(q) - 0.5;
        float h = hash(g + fi);
        vec2 o = (hash2(g + fi * 3.0) - 0.5) * 0.7;
        vec2 d = f - o;
        vec2 dir = normalize(p + 1e-4);
        float along = dot(d, dir), across = dot(d, vec2(-dir.y, dir.x));
        float len = 0.01 + streak * 0.08 * z * z;
        float s = smoothstep(0.03 * (0.3 + z), 0.0, abs(across)) * smoothstep(len, 0.0, abs(along + len * 0.5));
        float fade = smoothstep(0.0, 0.25, z) * smoothstep(1.0, 0.85, z);
        col += hsv(uHue + 0.15 * (h - 0.5), uSat * 0.6, 1.4) * s * step(0.35, h) * fade;
    }
    float tun = abs(atan(p.y, p.x)) / PI;
    col += hsv(uHue + 0.05, uSat, 0.25) * fbm(vec2(tun * 8.0, 1.0 / (length(p) + 0.1) + t)) * smoothstep(0.1, 0.9, length(p)) * 0.6;
    frag = vec4(col * uInt, 1.0);
}
)";

inline const char* bgAurora = R"(
void main() {
    vec2 p = P();
    float t = uTime * uSpeed * 0.3;
    vec3 col = mix(hsv(uHue + 0.1, uSat * 0.7, 0.07), hsv(uHue + 0.15, uSat * 0.9, 0.012), smoothstep(-0.3, 0.6, p.y));
    col += vec3(stars(p, 80.0, uTime * 2.0)) * 0.8;
    for (int k = 0; k < 4; ++k) {
        float fk = float(k);
        float base = 0.05 + 0.08 * fk + 0.06 * sin(p.x * (1.1 + 0.3 * fk) + t * (1.0 + 0.2 * fk) + fk);
        base += 0.07 * (fbm(vec2(p.x * 1.5 + fk * 3.0, t * 0.6)) - 0.5) + 0.04 * uBass * uReact;
        float above = p.y - base;
        float curtain = exp(-max(above, 0.0) * (6.0 - fk)) * smoothstep(-0.015, 0.02, above);
        float rays = 0.45 + 0.55 * noise(vec2(p.x * 28.0 + fk * 10.0 + sin(t + fk) * 2.0, t * 2.0));
        float amp = (0.35 + 0.65 * band(0.15 + 0.2 * fk)) * (0.6 + uMid * uReact);
        vec3 cc = mix(hsv(uHue - 0.02 + 0.05 * fk, uSat, 1.0), hsv(uHue + 0.35, uSat * 0.9, 1.0), clamp(above * 3.0, 0.0, 1.0));
        col += cc * curtain * rays * amp * 0.45;
    }
    float ridge = -0.28 + 0.06 * fbm(vec2(p.x * 2.5, 1.0)) + 0.03 * sin(p.x * 4.0);
    col = mix(col, hsv(uHue + 0.1, 0.4, 0.01), smoothstep(ridge + 0.003, ridge - 0.003, p.y));
    frag = vec4(col * uInt * 1.2, 1.0);
}
)";

inline const char* bgRetro = R"(
void main() {
    vec2 p = P();
    float t = uTime * uSpeed;
    float hz = -0.05;
    vec3 col;
    if (p.y > hz) {
        col = mix(hsv(uHue + 0.05, uSat, 0.35), hsv(uHue - 0.08, uSat, 0.04), smoothstep(hz, 0.55, p.y));
        vec2 sc = p - vec2(0.0, 0.16);
        float R = 0.25 * (1.0 + 0.06 * uBass * uReact);
        float d = length(sc);
        float below = clamp(-(sc.y - 0.03) / (R + 0.03), 0.0, 1.0);
        float stripes = step(below * 0.55, fract(sc.y * 26.0 - t * 0.25));
        float sun = smoothstep(R, R - 0.004, d) * stripes;
        vec3 sunCol = mix(hsv(uHue + 0.14, uSat, 1.0), hsv(uHue - 0.06, uSat, 0.95), clamp(0.5 - sc.y / (2.0 * R), 0.0, 1.0));
        col = mix(col, sunCol, sun);
        col += hsv(uHue - 0.2, uSat, 0.6) * exp(-d * 5.0) * (0.25 + 0.5 * uLevel);
        float m = hz + 0.08 * fbm(vec2(p.x * 3.0, 2.0)) + 0.03 * abs(sin(p.x * 7.0));
        col = mix(col, hsv(uHue, uSat, 0.03) + hsv(uHue + 0.5, uSat, 0.1) * smoothstep(m - 0.01, m, p.y), smoothstep(m + 0.002, m - 0.002, p.y));
        col += vec3(stars(p, 70.0, uTime)) * smoothstep(0.2, 0.5, p.y) * 0.6;
    } else {
        float depth = hz - p.y;
        float z = 0.18 / depth;
        vec2 g = vec2(p.x * z, z + t * 1.6 * (1.0 + 0.7 * uBass * uReact));
        vec2 f = abs(fract(g * vec2(1.6, 1.0)) - 0.5) / vec2(fwidth(g.x * 1.6), fwidth(g.y));
        float line = 1.0 - min(min(f.x, f.y), 1.0);
        float fade = smoothstep(0.0, 0.05, depth);
        col = hsv(uHue + 0.5, uSat * 0.8, 0.04) + hsv(uHue + 0.5 + 0.1 * uBeat, uSat, 1.2) * line * fade * (0.6 + 0.8 * uBeat * uReact);
        col += hsv(uHue - 0.1, uSat, 0.5) * exp(-depth * 18.0);
    }
    frag = vec4(col * uInt, 1.0);
}
)";

// ------------------------------------------------------------------ spectrum layer (additive)
inline const char* spPolar = R"(
void main() {
    vec2 p = P();
    float r = length(p);
    float u = abs(atan(p.x, p.y)) / PI;
    float b = band(u * 0.92) * uGain;
    float R0 = 0.24 * uSize * (1.0 + 0.05 * uBeat);
    float h = b * 0.2 * uSize;
    float segs = 64.0;
    float seg = abs(fract(u * segs) - 0.5);
    float bar = smoothstep(0.5, 0.5 - 0.15 - 0.2 * uThick, 0.5 - seg + 0.0) ;
    bar = smoothstep(0.32 - 0.25 * uThick, 0.2 - 0.25 * uThick, seg);
    float inBar = smoothstep(R0 - 0.002, R0 + 0.002, r) * smoothstep(R0 + h + 0.003, R0 + h - 0.002, r);
    vec3 col = hsv(uHue + 0.12 * u, uSat, 1.0) * inBar * bar * (0.6 + 1.2 * b);
    float ringW = 0.002 + 0.004 * uThick;
    col += hsv(uHue, uSat * 0.8, 1.0) * smoothstep(ringW, 0.0, abs(r - R0 + 0.008)) * (0.6 + uLevel);
    col += hsv(uHue, uSat, 1.0) * exp(-abs(r - R0) * 30.0) * 0.12 * (0.3 + uBass);
    float inner = smoothstep(R0 - 0.01, R0 - 0.04, r);
    col += hsv(uHue, uSat * 0.7, 0.05) * inner * uBass;
    frag = vec4(col, 1.0);
}
)";

inline const char* spLine = R"(
float curve(float x) {
    float hw = 0.5 * uRes.x / min(uRes.x, uRes.y);
    float u = abs(x) / hw * 0.95;
    float s = 0.0;
    for (int k = -3; k <= 3; ++k) s += band(u + float(k) * 0.008) * (4.0 - abs(float(k)));
    return s / 16.0 * uGain;
}
void main() {
    vec2 p = P();
    float y0 = -0.02;
    float amp = 0.24 * uSize;
    float e = 0.003;
    float b = curve(p.x);
    float y = y0 + b * amp;
    float dy = (curve(p.x + e) - curve(p.x - e)) / (2.0 * e) * amp;
    float k = inversesqrt(1.0 + min(dy * dy, 25.0));
    float d = abs(p.y - y) * k;
    float yr = y0 - b * amp * 0.6;
    float dr = abs(p.y - yr) * k;
    float w = 0.0015 + 0.004 * uThick;
    vec3 c = hsv(uHue + 0.12 * p.x, uSat, 1.0);
    vec3 col = c * (smoothstep(w, 0.0, d) * 1.5 + exp(-d * 70.0) * 0.3) * (0.7 + uLevel);
    col += c * (smoothstep(w, 0.0, dr) * 0.35 + exp(-dr * 70.0) * 0.08);
    float fill = step(min(y, yr), p.y) * step(p.y, max(y, yr));
    col += c * fill * 0.035 * (0.5 + b);
    frag = vec4(col, 1.0);
}
)";

inline const char* spHex = R"(
vec4 hexCell(vec2 p) {
    vec2 s = vec2(1.0, 1.7320508);
    vec4 hc = floor(vec4(p, p - vec2(0.5, 1.0)) / s.xyxy) + 0.5;
    vec4 h = vec4(p - hc.xy * s, p - (hc.zw + 0.5) * s);
    return dot(h.xy, h.xy) < dot(h.zw, h.zw) ? vec4(h.xy, hc.xy) : vec4(h.zw, hc.zw + 0.5);
}
void main() {
    vec2 p = P();
    float sc = 34.0 / uSize;
    vec4 hc = hexCell(p * sc);
    vec2 cellPos = hc.zw * vec2(1.0, 1.7320508) / sc;
    float hw = 0.5 * uRes.x / min(uRes.x, uRes.y);
    float b = band(abs(cellPos.x) / hw * 0.95) * uGain;
    float bottom = -0.5;
    float level = bottom + b * 0.85 * uSize;
    vec2 a = abs(hc.xy);
    float hexd = max(dot(a, normalize(vec2(1.0, 1.7320508))), a.x);
    float edge = smoothstep(0.5, 0.5 - 0.06 - 0.12 * uThick, hexd);
    float lit = smoothstep(level + 0.005, level - 0.005, cellPos.y);
    float topGlow = exp(-abs(cellPos.y - level) * 25.0);
    vec3 c = hsv(uHue + 0.25 * (cellPos.y - bottom), uSat, 1.0);
    vec3 col = c * edge * (lit * (0.35 + 0.8 * b) + topGlow * 0.6 * b);
    col += c * (1.0 - edge) * edge * 0.0;
    col += c * smoothstep(0.5, 0.45, hexd) * smoothstep(0.38, 0.45, hexd) * lit * 0.6;
    frag = vec4(col, 1.0);
}
)";

// ------------------------------------------------------------------ ornament (mask texture, additive)
inline const char* ornamentFS = R"(#version 150
in vec2 vUv; out vec4 frag;
uniform vec2 uRes; uniform sampler2D uMask;
uniform float uAngle, uScale, uHue, uSat, uGlow, uBright;
vec3 hsv(float h, float s, float v) { vec3 k = clamp(abs(fract(h + vec3(0.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0); return v * mix(vec3(1.0), k, s); }
void main() {
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / min(uRes.x, uRes.y);
    float c = cos(uAngle), s = sin(uAngle);
    vec2 q = mat2(c, -s, s, c) * p / uScale;
    vec2 uv = q * 0.5 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) { frag = vec4(0.0); return; }
    float m = texture(uMask, uv).r;
    float halo = textureLod(uMask, uv, 4.5).r;
    float edge = clamp(m - textureLod(uMask, uv, 2.0).r * 0.7, 0.0, 1.0);
    vec3 col = hsv(uHue, uSat, 1.0) * (m * 0.55 + edge * 0.9) * uBright + hsv(uHue + 0.03, uSat, 1.0) * halo * uGlow;
    frag = vec4(col, 1.0);
}
)";

// ------------------------------------------------------------------ particles (instanced quads, additive)
inline const char* particleVS = R"(#version 150
in vec4 iPos;      // xyz, size
in vec3 iCol;
out vec2 vLocal; out vec3 vCol;
uniform mat4 uView;
uniform vec2 uRes;
uniform float uCamDist, u2D, uSizeMul;
void main() {
    vec2 corner = vec2((gl_VertexID & 1) == 1 ? 1.0 : -1.0, (gl_VertexID & 2) == 2 ? 1.0 : -1.0);
    vLocal = corner; vCol = iCol;
    float mn = min(uRes.x, uRes.y);
    vec2 sc = vec2(mn / uRes.x, mn / uRes.y);
    float radPx = max(iPos.w * mn * uSizeMul, 0.75);
    if (u2D > 0.5) {
        vec2 ndc = iPos.xy * 2.0 * sc;
        gl_Position = vec4(ndc + corner * radPx * 2.0 / uRes, 0.0, 1.0);
        if (radPx < 1.0) vCol *= radPx;
    } else {
        vec4 v = uView * vec4(iPos.xyz, 1.0);
        float w = max(-v.z, 0.05);
        float f = 2.4142135;   // 1/tan(22.5 deg)
        vec4 clip = vec4(v.x * f * sc.x, v.y * f * sc.y, 0.0, w);
        float r = radPx * uCamDist / w;
        if (r < 0.9) { vCol *= (r / 0.9) * (r / 0.9); r = 0.9; }
        clip.xy += corner * r * 2.0 / uRes * w;
        clip.z = (w - 0.1) / 100.0 * w;
        gl_Position = clip;
    }
})";

inline const char* particleFS = R"(#version 150
in vec2 vLocal; in vec3 vCol; out vec4 frag;
uniform float uSoft;
void main() {
    float d = length(vLocal);
    if (d > 1.0) discard;
    float a = mix(smoothstep(1.0, 0.0, d) * smoothstep(1.0, 0.25, d) * 1.6, 1.0 - smoothstep(0.75, 1.0, d), uSoft);
    if (uSoft > 0.5) a = (1.0 - smoothstep(0.8, 1.0, d)) * (0.55 + 0.45 * smoothstep(0.5, 0.95, d));
    frag = vec4(vCol * a, 1.0);
})";

// ------------------------------------------------------------------ video layer
inline const char* videoFS = R"(#version 150
in vec2 vUv; out vec4 frag;
uniform vec2 uRes, uVidSize; uniform sampler2D uTex;
uniform float uFramed, uOpacity, uBright, uContrast, uSat, uGrain, uScan, uAberr, uTime, uBeat;
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
void main() {
    vec2 frame = uRes;
    vec2 uv = gl_FragCoord.xy / uRes;
    float va = uVidSize.x / uVidSize.y, fa = frame.x / frame.y;
    vec2 q;
    float inside = 1.0, border = 0.0;
    if (uFramed < 0.5) {
        q = uv - 0.5; if (fa > va) q.y *= va / fa; else q.x *= fa / va; q += 0.5;   // cover
    } else {
        vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / min(uRes.x, uRes.y);
        float h = 0.52, w = h * va; if (w > 0.85 * fa) { w = 0.85 * fa; h = w / va; }
        if (fa < 1.0) { w = min(0.8, 0.8 * va); h = w / va; }
        q = p / vec2(w, h) + 0.5;
        vec2 dd = abs(q - 0.5) - 0.5;
        float od = max(dd.x * w, dd.y * h);
        inside = step(od, 0.0);
        border = smoothstep(0.012, 0.0, abs(od - 0.003)) * (0.5 + uBeat);
    }
    q.y = 1.0 - q.y;
    float ab = uAberr * 0.006;
    vec3 c = vec3(texture(uTex, q + vec2(ab, 0.0)).r, texture(uTex, q).g, texture(uTex, q - vec2(ab, 0.0)).b);
    c = (c - 0.5) * uContrast + 0.5;
    c *= uBright;
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(l), c, uSat);
    c += (hash(gl_FragCoord.xy + fract(uTime * 7.1) * 311.0) - 0.5) * uGrain * 0.35;
    c *= 1.0 - uScan * 0.45 * (0.5 + 0.5 * sin(gl_FragCoord.y * 3.14159 * 0.5));
    c = max(c, 0.0);
    frag = vec4(c * inside + vec3(0.9) * border, uOpacity * max(inside, border));
})";

// ------------------------------------------------------------------ text backing panel (darkening)
inline const char* panelFS = R"(#version 150
in vec2 vUv; out vec4 frag;
uniform vec2 uRes; uniform vec4 uBox; uniform sampler2D uLum; uniform float uAmount;
void main() {
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / min(uRes.x, uRes.y);
    vec2 c = 0.5 * (uBox.xy + uBox.zw), hs = 0.5 * (uBox.zw - uBox.xy) + vec2(0.07, 0.05);
    vec2 d = abs(p - c) - hs;
    float sd = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - 0.04;
    float shape = smoothstep(0.06, -0.02, sd);
    vec3 bg = texture(uLum, gl_FragCoord.xy / uRes).rgb;
    float lum = dot(bg, vec3(0.299, 0.587, 0.114));
    float a = shape * smoothstep(0.18, 0.55, lum) * uAmount * 0.75;
    frag = vec4(0.0, 0.0, 0.0, a);
})";

// ------------------------------------------------------------------ bloom
inline const char* brightFS = R"(#version 150
in vec2 vUv; out vec4 frag; uniform sampler2D uTex; uniform vec2 uTexel;
void main() {
    vec3 a = texture(uTex, vUv + uTexel * vec2(-1, -1)).rgb + texture(uTex, vUv + uTexel * vec2(1, -1)).rgb
           + texture(uTex, vUv + uTexel * vec2(-1, 1)).rgb + texture(uTex, vUv + uTexel * vec2(1, 1)).rgb;
    a *= 0.25;
    float l = max(a.r, max(a.g, a.b));
    float k = max(l - 0.55, 0.0); k = k * k / (k + 0.25 + 1e-4) ;
    frag = vec4(a * (k / max(l, 1e-4)) + a * 0.04, 1.0);
})";

inline const char* downFS = R"(#version 150
in vec2 vUv; out vec4 frag; uniform sampler2D uTex; uniform vec2 uTexel;
void main() {
    vec3 c = texture(uTex, vUv).rgb * 0.5;
    c += texture(uTex, vUv + uTexel * vec2(-1, -1)).rgb * 0.125;
    c += texture(uTex, vUv + uTexel * vec2( 1, -1)).rgb * 0.125;
    c += texture(uTex, vUv + uTexel * vec2(-1,  1)).rgb * 0.125;
    c += texture(uTex, vUv + uTexel * vec2( 1,  1)).rgb * 0.125;
    frag = vec4(c, 1.0);
})";

inline const char* upFS = R"(#version 150
in vec2 vUv; out vec4 frag; uniform sampler2D uTex; uniform vec2 uTexel; uniform float uWeight;
void main() {
    vec3 c = texture(uTex, vUv).rgb * 4.0;
    c += texture(uTex, vUv + uTexel * vec2(-1, 0)).rgb * 2.0 + texture(uTex, vUv + uTexel * vec2(1, 0)).rgb * 2.0;
    c += texture(uTex, vUv + uTexel * vec2(0, -1)).rgb * 2.0 + texture(uTex, vUv + uTexel * vec2(0, 1)).rgb * 2.0;
    c += texture(uTex, vUv + uTexel * vec2(-1, -1)).rgb + texture(uTex, vUv + uTexel * vec2(1, -1)).rgb;
    c += texture(uTex, vUv + uTexel * vec2(-1, 1)).rgb + texture(uTex, vUv + uTexel * vec2(1, 1)).rgb;
    frag = vec4(c / 16.0 * uWeight, 1.0);
})";

// ------------------------------------------------------------------ final composite
inline const char* compositeFS = R"(#version 150
in vec2 vUv; out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform vec2 uRes;
uniform float uBloomAmt, uChroma, uVignette, uExposure, uTime;
vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
void main() {
    vec2 uv = vUv;
    vec2 d = (uv - 0.5);
    vec2 off = d * uChroma * 0.014 * (0.3 + length(d) * 2.0);
    vec3 c;
    c.r = texture(uScene, uv + off).r + texture(uBloom, uv + off * 1.5).r * uBloomAmt * 1.4;
    c.g = texture(uScene, uv).g + texture(uBloom, uv).g * uBloomAmt * 1.4;
    c.b = texture(uScene, uv - off).b + texture(uBloom, uv - off * 1.5).b * uBloomAmt * 1.4;
    vec2 pv = d * vec2(uRes.x / min(uRes.x, uRes.y), uRes.y / min(uRes.x, uRes.y));
    c *= mix(1.0, smoothstep(1.25, 0.25, length(pv)), uVignette);
    c = aces(c * uExposure * 1.1);
    c = pow(c, vec3(0.92));
    c += (hash(gl_FragCoord.xy) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
})";

// ------------------------------------------------------------------ blit to screen
inline const char* blitFS = R"(#version 150
in vec2 vUv; out vec4 frag; uniform sampler2D uTex; uniform vec4 uRect; // x,y,w,h in pixels of viewport
uniform vec2 uView;
void main() {
    vec2 pix = gl_FragCoord.xy;
    vec2 uv = (pix - uRect.xy) / uRect.zw;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) { frag = vec4(0.0, 0.0, 0.0, 1.0); return; }
    frag = vec4(texture(uTex, uv).rgb, 1.0);
})";
}
