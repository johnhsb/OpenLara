R"====(
#define MAX_LIGHTS			4
#define WATER_FOG_DIST		(1.0 / (6.0 * 1024.0))
#define WATER_COLOR_DIST	(1.0 / (2.0 * 1024.0))
#define UNDERWATER_COLOR	vec3(0.6, 0.9, 0.9)

varying vec3 vCoord;
varying vec2 vTexCoord;
varying vec4 vProjCoord;
varying vec3 vViewVec;
varying vec3 vLightVec;

#ifdef WATER_CAUSTICS
	varying vec3 vOldPos;
	varying vec3 vNewPos;
#endif

#if defined(WATER_COMPOSE) || defined(WATER_SIMULATE)
	varying vec2 vMaskCoord;
#endif

uniform vec4  uViewPos;
uniform mat4  uViewProj;
uniform vec4  uLightPos[MAX_LIGHTS];
uniform vec4  uPosScale[2];

uniform vec4  uTexParam;
uniform vec4  uParam;
uniform vec4  uRoomSize;

uniform sampler2D sNormal;

vec3 calcNormal(vec2 tc, float base) {
	float dx = texture2D(sNormal, vec2(tc.x + uTexParam.x, tc.y)).x - base;
	float dz = texture2D(sNormal, vec2(tc.x, tc.y + uTexParam.y)).x - base;
	return normalize( vec3(dx, 64.0 / (1024.0 * 8.0), dz) );
}

#ifdef VERTEX
	attribute vec4 aCoord;

	void main() {
		vec3 coord = aCoord.xyz * (1.0 / 32767.0);

		vTexCoord = (coord.xy * 0.5 + 0.5) * uTexParam.zw;

	#if defined(WATER_COMPOSE) || defined(WATER_SIMULATE)
		vMaskCoord = (coord.xy * 0.5 + 0.5) * uRoomSize.zw;
	#endif

		#if defined(WATER_MASK) || defined(WATER_COMPOSE) || defined(WATER_LAVA)

			float height = 0.0;

			vCoord = vec3(coord.x, height, coord.y) * uPosScale[1].xyz + uPosScale[0].xyz;

			vec4 cp = uViewProj * vec4(vCoord, 1.0);

			vProjCoord	= cp;
			gl_Position = cp;
		#else
			vProjCoord = vec4(0.0);
			vCoord	   = vec3(coord.xy, 0.0);
			#ifdef WATER_CAUSTICS
				vec3 rCoord = vec3(coord.x, coord.y, 0.0) * uPosScale[1].xzy;

				vec2 tc = (rCoord.xy * 0.5 + 0.5) * uTexParam.zw;
				vec4 info = texture2D(sNormal, tc);
				vec3 normal = calcNormal(tc, info.x).xzy;

				vec3 light = vec3(0.0, 0.0, 1.0);
				vec3 refOld = refract(-light, vec3(0.0, 0.0, 1.0), 0.75);
				vec3 refNew = refract(-light, normalize(normal + vec3(0.0, 0.0, 0.25)), 0.75);

				vOldPos = rCoord + refOld * (-1.0 / refOld.z) + refOld * ((-refOld.z - 1.0) / refOld.z);
				vNewPos = rCoord + refNew * ((info.r - 1.0) / refNew.z) + refOld * ((-refNew.z - 1.0) / refOld.z);

				gl_Position = vec4(vNewPos.xy + refOld.xy / refOld.z, 0.0, 1.0);
			#else
				gl_Position = vec4(coord.xyz, 1.0);
			#endif

			#ifdef WATER_RAYS
				vCoord = aCoord.xyz + uParam.xyz;
				vec4 cp = uViewProj * vec4(vCoord, 1.0);
				vProjCoord	= cp;
				gl_Position = cp;
			#endif
		#endif
		vViewVec  = uViewPos.xyz - vCoord.xyz;
		vLightVec = uLightPos[0].xyz - vCoord.xyz;

		#ifdef WATER_COMPOSE
			vViewVec.y  = abs(vViewVec.y);
			vLightVec.y = abs(vLightVec.y);
		#endif
	}
#else
	uniform sampler2D sDiffuse;
	uniform sampler2D sReflect;
	uniform sampler2D sMask;

	uniform vec4 uLightColor;

	#define PI	 3.141592653589793

	float calcFresnel(float NdotV, float f0) {
		return f0 + (1.0 - f0) * pow(1.0 - NdotV, 5.0);
	}

#ifdef WATER_DROP
	vec4 drop() {
		vec2 v = texture2D(sNormal, vTexCoord).xy;

		float drop = max(0.0, 1.0 - length(uParam.xy - vTexCoord / uTexParam.xy) / uParam.z);
		drop = 0.5 - cos(drop * PI) * 0.5;
		v.x += drop * uParam.w;

		return vec4(v, 0.0, 0.0);
	}
#endif

#ifdef WATER_SIMULATE
	float noise3D(vec3 x) { // https://www.shadertoy.com/view/XslGRr
		vec3 p = floor(x);
		vec3 f = fract(x);
		f = f * f * (3.0 - 2.0 * f);
		vec2 uv = (p.xy + vec2(37.0, 17.0) * p.z) + f.xy;
		vec2 rg = texture2D(sDiffuse, (uv + 0.5) / 32.0).yx;
		return mix(rg.x, rg.y, f.z) * 2.0 - 1.0;
	}

	vec4 simulate() {
		vec2 tc = vTexCoord;

		vec2 v = texture2D(sNormal, tc).xy; // height, speed

		vec3 d = vec3(uTexParam.xy, 0.0);
		vec4 f = vec4(texture2D(sNormal, tc + d.xz).x, texture2D(sNormal, tc + d.zy).x,
					  texture2D(sNormal, tc - d.xz).x, texture2D(sNormal, tc - d.zy).x);
		float average = dot(f, vec4(0.25));

	// integrate
		const float vel = 1.4;
		const float vis = 0.995;
		v.y += (average - v.x) * vel;
		v.y *= vis;
		v.x += v.y + noise3D(vec3(tc * 32.0, uParam.w)) * 0.00025;
		v *= texture2D(sMask, vMaskCoord).x;

		return vec4(v.xy, 0.0, 0.0);
	}
#endif

#ifdef WATER_CAUSTICS
	vec4 caustics() {
		float rOldArea = length(dFdx(vOldPos)) * length(dFdy(vOldPos));
		float rNewArea = length(dFdx(vNewPos)) * length(dFdy(vNewPos));
		rNewArea = max(rNewArea, 0.00002); // WebGL NVIDIA workaround >_<
		float value = clamp(rOldArea / rNewArea * 0.2, 0.0, 1.0);
		return vec4(value, 0.0, 0.0, 0.0);
	}
#endif

#ifdef WATER_RAYS
	float boxIntersect(vec3 rayPos, vec3 rayDir, vec3 center, vec3 hsize) {
		center -= rayPos;
		vec3 bMin = (center - hsize) / rayDir;
		vec3 bMax = (center + hsize) / rayDir;
		vec3 m = min(bMin, bMax);
		return max(0.0, max(m.x, max(m.y, m.z)));
	}

	vec4 rays() {
		#define RAY_STEPS 16.0

		vec3 viewVec = normalize(vViewVec);

		float t = boxIntersect(uViewPos.xyz, -viewVec, uPosScale[0].xyz, uPosScale[1].xyz);

		vec3 p0 = uViewPos.xyz - viewVec * t;
		vec3 p1 = vCoord.xyz;

		float dither = texture2D(sMask, gl_FragCoord.xy * (1.0 / 8.0)).x;
		vec3 step = (p1 - p0) / RAY_STEPS;
		vec3 pos  = p0 + step * dither;

		float sum = 0.0;
		for (float i = 0.0; i < RAY_STEPS; i++) {
			vec3 wpos = (pos - uPosScale[0].xyz) / uPosScale[1].xyz;
			vec2 tc = wpos.xz * 0.5 + 0.5;
			float light = texture2D(sReflect, tc).x;
			sum += light * (1.0 - (clamp(wpos.y, -1.0, 1.0) * 0.5 + 0.5));
			pos += step;
		}
		sum /= RAY_STEPS;
		sum *= uParam.w;

		return vec4(UNDERWATER_COLOR * sum, 1.0);
	}
#endif

	vec4 mask() {
		return vec4(0.0);
	}

	void applyFog(inout vec3 color, float dist) {
		dist *= step(vCoord.y, uViewPos.y);
		color.xyz *= mix(vec3(1.0), UNDERWATER_COLOR, clamp(dist * WATER_COLOR_DIST, 0.0, 2.0));

		float fog = clamp(1.0 / exp(dist * WATER_FOG_DIST), 0.0, 1.0);
		color.xyz = mix(UNDERWATER_COLOR * 0.2, color.xyz, fog);
	}

#if defined(WATER_COMPOSE) || defined(WATER_LAVA)
	float hash12(vec2 p) { // https://www.shadertoy.com/view/4djSRW (no sin, the same on every GPU)
		vec3 p3 = fract(vec3(p.xyx) * 0.1031);
		p3 += dot(p3, p3.yzx + 33.33);
		return fract((p3.x + p3.y) * p3.z);
	}

	float valueNoise(vec2 p) {
		vec2 i = floor(p);
		vec2 f = fract(p);
		f = f * f * (3.0 - 2.0 * f);
		return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), f.x),
				   mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), f.x), f.y);
	}
#endif

#ifdef WATER_COMPOSE

	// the white foam on the steepest waves (the wake of the boat, a splash): the most where the water still moves fast
	// (just behind the boat), less as the waves calm down, in soft patches of a noise fixed in the water
	float calcFoam(vec3 normal, float speed) {
		float slope = length(normal.xz);
		float amount = smoothstep(0.4, 0.85, slope);
		if (amount <= 0.0)
			return 0.0;
		amount *= 0.5 + 0.5 * smoothstep(0.001, 0.006, abs(speed));
		vec2 p = vCoord.xz;
		float pattern = valueNoise(p * (1.0 / 32.0)) * 0.6 + valueNoise(p * (1.0 / 14.0)) * 0.4;
		return amount * smoothstep(0.4, 0.75, pattern + amount * 0.3);
	}

	// each surface of a room has its own simulation, the waves stop at its sides: they die out over the last half a sector
	// there, the surface of the next room doesn't meet them with a line
	float calcEdgeFade() {
		vec2 uv = vTexCoord / uTexParam.zw;
		vec2 texels = uTexParam.zw / uTexParam.xy;
		vec2 d = min(uv, 1.0 - uv) * texels;
		return clamp(min(d.x, d.y) * (1.0 / 32.0), 0.0, 1.0);
	}

	vec4 compose() {
		vec3 viewVec = normalize(vViewVec);

		vec2 value = texture2D(sNormal, vTexCoord).xy;
		vec3 normal = calcNormal(vTexCoord, value.x);
		float edge = calcEdgeFade();
		normal = normalize(vec3(normal.x * edge, normal.y, normal.z * edge));

		vec2 dudv = (uViewProj * vec4(normal.x, 0.0, normal.z, 0.0)).xy;

		vec3 rv = reflect(-viewVec, normal);
		vec3 lv = normalize(vLightVec);

		float spec = pow(max(0.0, dot(rv, lv)), 64.0) * 0.5;

		vec2 tc = vProjCoord.xy / vProjCoord.w * 0.5 + 0.5;

		vec4 refrA = texture2D(sDiffuse, uParam.xy * clamp(tc + dudv * uParam.z, 0.0, 0.999) );
		vec4 refrB = texture2D(sDiffuse, uParam.xy * tc);
		vec4 refr  = vec4(mix(refrA.xyz, refrB.xyz, refrA.w), 1.0);
		vec4 refl  = texture2D(sReflect, vec2(tc.x, 1.0 - tc.y) + dudv * uParam.w);

		float fresnel = calcFresnel(abs(dot(normal, viewVec)), 0.12);

		vec4 color = mix(refr, refl, fresnel);
		color.xyz += spec * 1.5;
		float luma = dot(color.xyz, vec3(0.299, 0.587, 0.114));
		color.xyz = mix(color.xyz, vec3(0.8, 0.85, 0.85) * (0.6 + 0.4 * luma), calcFoam(normal, value.y) * 0.6);
		color.w = texture2D(sMask, vMaskCoord).x;
		applyFog(color.xyz, vViewVec.y / viewVec.y);

		return color;
	}
#endif


#ifdef WATER_LAVA
	uniform vec4 uFogParams;

	float fbm(vec2 p) {
		float v = 0.0;
		float a = 0.5;
		for (int i = 0; i < 4; i++) {
			v += valueNoise(p) * a;
			p = p * 2.03 + vec2(17.1, 9.7);
			a *= 0.5;
		}
		return v;
	}

	// the plates of the crust (1) drifting on the molten lava (0), and the swell of both
	float lavaCrust(vec2 q, float t) {
		return smoothstep(0.43, 0.5, fbm(q * 1.1 + vec2(t * 0.011, -t * 0.007)));
	}

	float lavaHeight(vec2 q, float t) {
		float crust  = lavaCrust(q, t);
		float molten = fbm(q * 2.6 - vec2(t * 0.05, t * 0.03));
		return crust * (0.55 + 0.45 * fbm(q * 7.0)) + molten * 0.12;
	}

	// the hot glow: dark red at the edge of the crust, orange to yellow in the open lava
	vec3 lavaGlow(float x) {
		vec3 c = mix(vec3(0.22, 0.01, 0.0), vec3(0.95, 0.22, 0.02), smoothstep(0.0, 0.7, x));
		return c + vec3(0.5, 0.5, 0.15) * smoothstep(0.7, 1.0, x);
	}

	// the bubbles of the molten lava: a dome swells, it bursts and a ring of glow runs out from it (a few in a cell of
	// 0.4 of a sector, each on a cycle of its own); xyz - the colour of a dome, w - how much of the pixel it covers,
	// glow - the light of the bursts to add
	vec4 lavaBubbles(vec2 p, float t, vec3 viewVec, inout vec3 glow) {
		vec2 g  = p * 2.5;
		vec2 gi = floor(g);
		vec4 res = vec4(0.0);
		for (int y = -1; y <= 1; y++)
			for (int x = -1; x <= 1; x++) {
				vec2  cell = gi + vec2(float(x), float(y));
				float h    = hash12(cell);
				if (h < 0.45) continue; // no bubble in this cell
				float period = 2.5 + 3.0 * hash12(cell + 7.3);
				float phase  = fract(t / period + h * 7.0);
				vec2  c      = cell + 0.2 + 0.6 * vec2(hash12(cell + 1.7), hash12(cell + 3.1));
				vec2  d      = g - c;
				float dist   = length(d);
				float rmax   = 0.18 + 0.17 * hash12(cell + 5.9);
				if (phase < 0.8) { // swelling
					float r = rmax * smoothstep(0.0, 0.8, phase);
					if (dist < r) {
						float z = sqrt(1.0 - dist * dist / (r * r));
						vec3  n = normalize(vec3(d.x / r, z * 1.2, d.y / r));
						float shade = 0.45 + 0.55 * clamp(dot(n, normalize(vec3(0.3, 1.0, 0.2))), 0.0, 1.0);
						float spec  = pow(max(0.0, dot(reflect(-viewVec, n), vec3(0.0, 1.0, 0.0))), 24.0);
					// a skin of darker lava, hot at the rim where it stretches thin
						vec3 skin = lavaGlow(0.35 + 0.4 * (1.0 - z)) * shade + vec3(1.0, 0.6, 0.25) * spec * 0.35;
						res = vec4(skin, smoothstep(r, r * 0.85, dist));
					}
				} else { // burst: a ring runs out and fades
					float k = (phase - 0.8) / 0.2;
					float r = rmax * (1.0 + k * 1.5);
					float ring = 1.0 - smoothstep(0.0, 0.05 + 0.05 * k, abs(dist - r));
					ring *= (1.0 - k);
					glow += lavaGlow(0.85) * ring * 0.6;
					// the hole left in the middle glows bright for a moment
					glow += lavaGlow(1.0) * (1.0 - smoothstep(0.0, rmax * 0.8, dist)) * (1.0 - k) * 0.5;
				}
			}
		return res;
	}

	vec4 lava() {
		float t = uParam.x;
		vec2  p = vCoord.xz * (1.0 / 1024.0);

	// the lava flows slowly, it warps itself
		vec2 w = vec2(fbm(p * 0.7 + vec2(t * 0.020, t * 0.013)),
		              fbm(p * 0.7 + vec2(5.2, 1.3) - vec2(t * 0.017, t * 0.011)));
		vec2 q = p * 1.6 + (w - 0.5) * 1.4;

		float crust  = lavaCrust(q, t);
		float molten = fbm(q * 2.6 - vec2(t * 0.05, t * 0.03));

	// the relief of the crust for the light
		float e  = 0.02;
		float h  = lavaHeight(q, t);
		float hx = lavaHeight(q + vec2(e, 0.0), t);
		float hz = lavaHeight(q + vec2(0.0, e), t);
		vec3 normal = normalize(vec3((h - hx) * 6.0, 1.0, (h - hz) * 6.0));

		vec3 viewVec = normalize(vViewVec);

	// the open lava glows by itself and pulses slowly, the cracks near the edges of the crust glow dimmer
		float heat  = 0.2 + 0.8 * molten * molten * 1.6;
		heat *= 0.9 + 0.1 * sin(t * 1.7 + molten * 12.0);
	// bright threads where the lava runs fastest
		float vein  = 1.0 - smoothstep(0.0, 0.09, abs(fbm(q * 3.2 + w * 2.0 - vec2(t * 0.04, 0.0)) - 0.5));
		vein *= vein * (0.5 + 0.5 * molten); // soft and uneven, brighter where the lava is hotter
		heat = clamp(heat + vein * 0.25, 0.0, 1.0) * (1.0 - crust);
		float edge  = crust * (1.0 - crust) * 4.0; // the rim of a plate, half molten
		vec3  glow  = lavaGlow(heat) * (1.0 - crust) + lavaGlow(0.55) * edge * 0.5;

	// the crust is dark rock lit from the lava under it, with a dull sheen
		float shade = 0.35 + 0.65 * clamp(dot(normal, normalize(vec3(0.3, 1.0, 0.2))), 0.0, 1.0);
		vec3  rock  = vec3(0.09, 0.035, 0.025) * shade * (0.7 + 0.6 * fbm(q * 9.0));
		float sheen = pow(max(0.0, dot(reflect(-viewVec, normal), vec3(0.0, 1.0, 0.0))), 16.0) * 0.08;
		rock += vec3(1.0, 0.35, 0.1) * sheen;

		vec3 color = mix(glow, rock, crust) + glow * crust * 0.15;

	// the bubbles come up in the open lava only
		vec3  burst  = vec3(0.0);
		vec4  bubble = lavaBubbles(p, t, viewVec, burst);
		float open   = 1.0 - smoothstep(0.1, 0.5, crust);
		color = mix(color, bubble.xyz, bubble.w * open) + burst * open;

	// the haze of the distance, as on the rooms
		float fog = max(0.0, length((uViewPos.xyz - vCoord) * uFogParams.w) - FOG_START_DIST * uFogParams.w);
		fog *= fog;
		color = mix(uFogParams.xyz, color, clamp(1.0 / exp(fog), 0.0, 1.0));

		return vec4(color, 1.0);
	}
#endif

	vec4 process() {
		#ifdef WATER_DROP
			return drop();
		#endif

		#ifdef WATER_SIMULATE
			return simulate();
		#endif

		#ifdef WATER_CAUSTICS
			return caustics();
		#endif

		#ifdef WATER_RAYS
			return rays();
		#endif

		#ifdef WATER_MASK
			return mask();
		#endif

		#ifdef WATER_COMPOSE
			return compose();
		#endif

		#ifdef WATER_LAVA
			return lava();
		#endif

		return vec4(1.0, 0.0, 1.0, 1.0);
	}

	void main() {
	    fragColor = process();
	}
#endif
)===="
