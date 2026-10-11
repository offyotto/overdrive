export const meshVertex = `#version 300 es
precision highp float;
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
layout(location=3) in vec2 aMaterial;
uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat4 uLight;
out vec3 vWorld;
out vec3 vNormal;
out vec3 vColor;
out vec2 vMaterial;
out vec4 vShadow;
void main(){
  vec4 world=uModel*vec4(aPosition,1.);
  vWorld=world.xyz; vNormal=normalize(mat3(uModel)*aNormal);
  vColor=aColor; vMaterial=aMaterial; vShadow=uLight*world;
  gl_Position=uViewProjection*world;
}`;
const atmosphere = `
uniform float uTime;
uniform float uNight;
const vec3 SUN=vec3(-.526,.754,.392);
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.),f.x),f.y);}
float fbm(vec2 p){return noise(p)*.57+noise(p*2.04)*.27+noise(p*4.01)*.12+noise(p*8.03)*.04;}
vec3 sky(vec3 rd){
  float h=pow(max(rd.y,0.),.32);
  vec3 day=mix(vec3(.40,.65,.72),vec3(.022,.20,.49),h);
  vec3 night=mix(vec3(.17,.17,.36),vec3(.018,.032,.12),h);
  vec3 color=mix(day,night,uNight);
  float glow=pow(max(dot(rd,SUN),0.),8.);
  color+=vec3(.36,.16,.065)*glow*(1.-uNight);
  float sun=pow(max(dot(rd,SUN),0.),850.);
  color+=vec3(3.,2.3,1.3)*sun*(1.-uNight);
  if(rd.y>.01){
    vec2 uv=rd.xz/(rd.y+.25)*1.1+vec2(uTime*.006,0.);
    float cloud=smoothstep(.56,.78,fbm(uv))*smoothstep(.02,.3,rd.y);
    color=mix(color,mix(vec3(1.05,.92,.82),vec3(.29,.31,.5),uNight),cloud*.78);
    float star=step(.9984,hash(floor(rd.xz/(rd.y+.3)*750.)));
    color+=star*uNight*smoothstep(.08,.5,rd.y)*.8;
  }
  return color;
}
`;
export const meshFragment = `#version 300 es
precision highp float;
in vec3 vWorld;
in vec3 vNormal;
in vec3 vColor;
in vec2 vMaterial;
in vec4 vShadow;
uniform vec3 uCamera;
uniform sampler2D uShadow;
uniform bool uShadows;
out vec4 fragColor;
${atmosphere}
float shadow(vec3 n){
  if(!uShadows)return 1.;
  vec3 p=vShadow.xyz/vShadow.w*.5+.5;
  if(p.x<0.||p.y<0.||p.x>1.||p.y>1.||p.z>1.)return 1.;
  float bias=max(.00048*(1.-dot(n,SUN)),.00018),s=0.;
  vec2 texel=1./vec2(textureSize(uShadow,0));
  for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++)s+=p.z-bias>texture(uShadow,p.xy+vec2(x,y)*texel).r?.35:1.;
  return s/9.;
}
void main(){
  vec3 n=normalize(vNormal),v=normalize(uCamera-vWorld),h=normalize(SUN+v);
  float rough=vMaterial.x;
  vec3 base=pow(vColor,vec3(2.2));
  if(rough>.92)base*=.985+.03*hash(vWorld.xz*30.);
  float ndl=max(dot(n,SUN),0.),sh=shadow(n);
  vec3 hemi=mix(vec3(.12,.18,.28),vec3(.34,.47,.62),n.y*.5+.5);
  vec3 light=hemi*mix(.82,.5,uNight)+vec3(1.65,1.25,.84)*ndl*sh*(1.-uNight*.78);
  light+=vec3(.08,.13,.20)*max(dot(n,normalize(vec3(.8,.25,.6))),0.);
  vec3 color=base*light;
  float spec=pow(max(dot(n,h),0.),mix(160.,8.,rough))*(1.-rough)*sh;
  color+=vec3(1.,.92,.81)*spec*.55*(1.-uNight*.55);
  float fresnel=pow(1.-max(dot(n,v),0.),4.);
  color+=sky(reflect(-v,n))*fresnel*(1.-rough)*.28;
  color+=base*vMaterial.y*(1.+uNight*1.8);
  float fog=1.-exp(-length(uCamera-vWorld)*.00065);
  color=mix(color,sky(normalize(vWorld-uCamera)),fog);
  fragColor=vec4(color,1.);
}`;
export const fullVertex = `#version 300 es
precision highp float;
out vec2 vUv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);vUv=p;gl_Position=vec4(p*2.-1.,0.,1.);}`;
export const skyFragment = `#version 300 es
precision highp float;
in vec2 vUv;
uniform vec3 uForward;
uniform vec3 uRight;
uniform vec3 uUp;
uniform float uAspect;
uniform float uFov;
uniform float uShift;
out vec4 fragColor;
${atmosphere}
void main(){vec2 p=vUv*2.-1.;p.x+=uShift;vec3 rd=normalize(uForward+uRight*p.x*uAspect*uFov+uUp*p.y*uFov);fragColor=vec4(sky(rd),1.);}`;
export const waterVertex = `#version 300 es
precision highp float;
uniform mat4 uViewProjection;
out vec3 vWorld;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);vWorld=vec3((p.x*2.-1.)*3000.,-38.,(p.y*2.-1.)*3000.);gl_Position=uViewProjection*vec4(vWorld,1.);}`;
export const waterFragment = `#version 300 es
precision highp float;
in vec3 vWorld;
uniform vec3 uCamera;
uniform bool uRayTracing;
uniform int uEllipsoidCount;
uniform vec3 uCenters[12];
uniform vec3 uRadii[12];
uniform vec3 uColors[12];
out vec4 fragColor;
${atmosphere}
vec3 traceReflection(vec3 origin,vec3 direction){
  vec3 result=sky(direction);float closest=2000.;
  if(!uRayTracing)return result;
  for(int i=0;i<12;i++){
    if(i>=uEllipsoidCount)break;
    vec3 oc=(origin-uCenters[i])/uRadii[i],rd=direction/uRadii[i];
    float a=dot(rd,rd),b=dot(oc,rd),c=dot(oc,oc)-1.;
    float discriminant=b*b-a*c;
    if(discriminant>0.){
      float t=(-b-sqrt(discriminant))/a;
      if(t>.1&&t<closest){
        closest=t;vec3 hit=origin+direction*t;
        vec3 n=normalize((hit-uCenters[i])/(uRadii[i]*uRadii[i]));
        result=pow(uColors[i],vec3(2.2))*(.45+max(dot(n,SUN),0.)*.8);
        result=mix(result,sky(direction),1.-exp(-t*.0015));
      }
    }
  }
  return result;
}
void main(){
  vec2 p=vWorld.xz;
  vec3 n=normalize(vec3(sin(p.x*.13+uTime*.7)*.032+sin(p.y*.32-uTime)*.013,1.,cos(p.y*.10+uTime*.5)*.026));
  vec3 view=normalize(uCamera-vWorld),rd=reflect(-view,n);
  float fresnel=.13+.75*pow(1.-max(dot(n,view),0.),4.);
  vec3 reflection=traceReflection(vWorld+vec3(0,.03,0),rd);
  float waves=pow(max(0.,sin(p.x*.11+p.y*.065+uTime*.4)*sin(p.y*.14-uTime*.3)),12.);
  vec3 water=mix(vec3(.025,.32,.42),vec3(.007,.05,.13),uNight);
  water+=vec3(.025,.065,.068)*waves;
  vec3 color=mix(water,reflection,fresnel);
  float spec=pow(max(dot(reflect(-SUN,n),view),0.),130.);
  color+=vec3(1.8,1.3,.7)*spec*(1.-uNight);
  float fog=1.-exp(-length(uCamera-vWorld)*.0008);
  fragColor=vec4(mix(color,sky(normalize(vWorld-uCamera)),fog),1.);
}`;
export const shadowVertex = `#version 300 es
precision highp float;
layout(location=0) in vec3 aPosition;
uniform mat4 uModel;
uniform mat4 uLight;
void main(){gl_Position=uLight*uModel*vec4(aPosition,1.);}`;
export const shadowFragment = `#version 300 es
precision highp float;
void main(){}`;
export const postFragment = `#version 300 es
precision highp float;
in vec2 vUv;
uniform sampler2D uScene;
uniform vec2 uTexel;
uniform bool uBloom;
uniform float uBoost;
out vec4 fragColor;
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
void main(){
  vec2 uv=vUv;
  vec3 color=texture(uScene,uv).rgb;
  if(uBloom){
    vec3 bloom=vec3(0.);
    for(int i=0;i<8;i++){
      float a=float(i)*.785398;
      vec3 c=texture(uScene,uv+vec2(cos(a),sin(a))*uTexel*5.).rgb;
      bloom+=max(c-.8,0.);
    }
    color+=bloom*.07;
  }
  if(uBoost>.01){
    vec2 delta=(uv-.5)*.018*uBoost;
    color=mix(color,(texture(uScene,uv+delta).rgb+color+texture(uScene,uv-delta).rgb)/3.,uBoost*.5);
  }
  color=pow(aces(color*1.06),vec3(1./2.2));
  color=mix(vec3(dot(color,vec3(.2126,.7152,.0722))),color,1.09);
  float vignette=1.-dot(uv-.5,uv-.5)*.18;
  fragColor=vec4(color*vignette,1.);
}`;
