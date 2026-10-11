import { Geometry } from './geometry.ts';
import { cross, identity, lookAt, multiply, normalize, orthographic, perspective, scale, sub, type Mat4, type Vec3 } from './math.ts';
import { fullVertex, meshFragment, meshVertex, postFragment, shadowFragment, shadowVertex, skyFragment, waterFragment, waterVertex } from './shaders.ts';
import type { Reflector } from './world.ts';

export type Quality='high'|'balanced'|'performance';
export interface Camera { position:Vec3; target:Vec3; fov:number; shift:number; }
export interface GpuMesh { vao:WebGLVertexArrayObject; buffer:WebGLBuffer; count:number; }
export interface RenderObject { mesh:GpuMesh; matrix:Mat4; }
class Program {
  readonly handle:WebGLProgram;
  private locations=new Map<string,WebGLUniformLocation|null>();
  constructor(private gl:WebGL2RenderingContext,vertex:string,fragment:string) {
    const compile=(type:number,source:string)=>{
      const shader=gl.createShader(type);
      if(!shader)throw new Error('Unable to allocate a graphics shader.');
      gl.shaderSource(shader,source);gl.compileShader(shader);
      if(!gl.getShaderParameter(shader,gl.COMPILE_STATUS)) {
        const log=gl.getShaderInfoLog(shader);gl.deleteShader(shader);throw new Error(`Shader compilation failed: ${log}`);
      }
      return shader;
    };
    const vs=compile(gl.VERTEX_SHADER,vertex),fs=compile(gl.FRAGMENT_SHADER,fragment),program=gl.createProgram();
    if(!program)throw new Error('Unable to create a graphics program.');
    gl.attachShader(program,vs);gl.attachShader(program,fs);gl.linkProgram(program);gl.deleteShader(vs);gl.deleteShader(fs);
    if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw new Error(`Shader linking failed: ${gl.getProgramInfoLog(program)}`);
    this.handle=program;
  }
  use(){this.gl.useProgram(this.handle);}
  at(name:string){if(!this.locations.has(name))this.locations.set(name,this.gl.getUniformLocation(this.handle,name));return this.locations.get(name)!;}
  float(name:string,value:number){this.gl.uniform1f(this.at(name),value);}
  int(name:string,value:number){this.gl.uniform1i(this.at(name),value);}
  vec3(name:string,value:Vec3|Float32Array){this.gl.uniform3fv(this.at(name),value);}
  mat4(name:string,value:Mat4){this.gl.uniformMatrix4fv(this.at(name),false,value);}
}
export class Renderer {
  readonly gl:WebGL2RenderingContext;
  quality:Quality='high';
  private meshProgram:Program;
  private skyProgram:Program;
  private waterProgram:Program;
  private shadowProgram:Program;
  private postProgram:Program;
  private shadowTexture:WebGLTexture;
  private shadowFramebuffer:WebGLFramebuffer;
  private sceneTexture:WebGLTexture;
  private sceneFramebuffer:WebGLFramebuffer;
  private sceneDepth:WebGLRenderbuffer;
  private msaaFramebuffer:WebGLFramebuffer;
  private msaaColor:WebGLRenderbuffer;
  private msaaDepth:WebGLRenderbuffer;
  private useMsaa=false;
  private hdr:boolean;
  private light:Mat4;
  private width=0;
  private height=0;
  private blankVao:WebGLVertexArrayObject;
  private meshes:GpuMesh[]=[];
  private reflectors:Reflector[]=[];
  private readonly modelIdentity=identity();
  private disposed=false;
  drawCalls=0;
  triangles=0;
  constructor(readonly canvas:HTMLCanvasElement) {
    const gl=canvas.getContext('webgl2',{alpha:false,antialias:false,powerPreference:'high-performance',preserveDrawingBuffer:false});
    if(!gl)throw new Error('This game needs WebGL 2. Enable hardware acceleration in your browser and reload.');
    this.gl=gl;
    this.hdr=!!gl.getExtension('EXT_color_buffer_float');
    this.meshProgram=new Program(gl,meshVertex,meshFragment);
    this.skyProgram=new Program(gl,fullVertex,skyFragment);
    this.waterProgram=new Program(gl,waterVertex,waterFragment);
    this.shadowProgram=new Program(gl,shadowVertex,shadowFragment);
    this.postProgram=new Program(gl,fullVertex,postFragment);
    this.blankVao=this.require(gl.createVertexArray(),'vertex array');
    this.shadowTexture=this.require(gl.createTexture(),'shadow texture');
    gl.bindTexture(gl.TEXTURE_2D,this.shadowTexture);
    gl.texImage2D(gl.TEXTURE_2D,0,gl.DEPTH_COMPONENT24,2048,2048,0,gl.DEPTH_COMPONENT,gl.UNSIGNED_INT,null);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);
    this.shadowFramebuffer=this.require(gl.createFramebuffer(),'shadow framebuffer');
    gl.bindFramebuffer(gl.FRAMEBUFFER,this.shadowFramebuffer);
    gl.framebufferTexture2D(gl.FRAMEBUFFER,gl.DEPTH_ATTACHMENT,gl.TEXTURE_2D,this.shadowTexture,0);
    gl.drawBuffers([gl.NONE]);gl.readBuffer(gl.NONE);
    this.checkFramebuffer('shadow');
    this.sceneTexture=this.require(gl.createTexture(),'scene texture');
    this.sceneFramebuffer=this.require(gl.createFramebuffer(),'scene framebuffer');
    this.sceneDepth=this.require(gl.createRenderbuffer(),'scene depth');
    this.msaaFramebuffer=this.require(gl.createFramebuffer(),'multisample framebuffer');
    this.msaaColor=this.require(gl.createRenderbuffer(),'multisample color');
    this.msaaDepth=this.require(gl.createRenderbuffer(),'multisample depth');
    this.light=multiply(orthographic(-230,230,-210,210,1,780),lookAt(scale([-.526,.754,.392],360),[0,16,0]));
    gl.enable(gl.DEPTH_TEST);gl.depthFunc(gl.LEQUAL);gl.disable(gl.CULL_FACE);
    this.resize();
  }
  private require<T>(value:T|null,name:string):T {if(!value)throw new Error(`Unable to allocate ${name}.`);return value;}
  private checkFramebuffer(name:string){if(this.gl.checkFramebufferStatus(this.gl.FRAMEBUFFER)!==this.gl.FRAMEBUFFER_COMPLETE)throw new Error(`The ${name} render target is unavailable on this device.`);}
  upload(geometry:Geometry):GpuMesh {
    const gl=this.gl,vao=this.require(gl.createVertexArray(),'mesh'),buffer=this.require(gl.createBuffer(),'vertex buffer');
    gl.bindVertexArray(vao);gl.bindBuffer(gl.ARRAY_BUFFER,buffer);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(geometry.data),gl.STATIC_DRAW);
    for(const [loc,size,offset] of [[0,3,0],[1,3,3],[2,3,6],[3,2,9]]) {
      gl.enableVertexAttribArray(loc);gl.vertexAttribPointer(loc,size,gl.FLOAT,false,44,offset*4);
    }
    gl.bindVertexArray(null);
    const mesh={vao,buffer,count:geometry.data.length/11};this.meshes.push(mesh);return mesh;
  }
  setReflectors(reflectors:Reflector[]){this.reflectors=reflectors.slice(0,12);}
  setQuality(quality:Quality){this.quality=quality;this.width=0;this.resize();}
  resize(){
    const gl=this.gl,rect=this.canvas.getBoundingClientRect(),ratio=Math.min(window.devicePixelRatio||1,this.quality==='high'?1.5:1)*(this.quality==='performance'?.72:1);
    const width=Math.max(1,Math.round(rect.width*ratio)),height=Math.max(1,Math.round(rect.height*ratio));
    if(width===this.width&&height===this.height)return;
    this.width=width;this.height=height;this.canvas.width=width;this.canvas.height=height;
    const format=this.hdr?gl.RGBA16F:gl.RGBA8;
    gl.bindTexture(gl.TEXTURE_2D,this.sceneTexture);
    gl.texImage2D(gl.TEXTURE_2D,0,format,width,height,0,gl.RGBA,this.hdr?gl.HALF_FLOAT:gl.UNSIGNED_BYTE,null);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);
    gl.bindFramebuffer(gl.FRAMEBUFFER,this.sceneFramebuffer);
    gl.framebufferTexture2D(gl.FRAMEBUFFER,gl.COLOR_ATTACHMENT0,gl.TEXTURE_2D,this.sceneTexture,0);
    gl.bindRenderbuffer(gl.RENDERBUFFER,this.sceneDepth);gl.renderbufferStorage(gl.RENDERBUFFER,gl.DEPTH_COMPONENT24,width,height);
    gl.framebufferRenderbuffer(gl.FRAMEBUFFER,gl.DEPTH_ATTACHMENT,gl.RENDERBUFFER,this.sceneDepth);
    this.checkFramebuffer('scene');
    const supported=gl.getInternalformatParameter(gl.RENDERBUFFER,format,gl.SAMPLES) as Int32Array;
    const sampleCounts=Array.from(supported).filter(n=>n>0);
    const samples=sampleCounts.length?Math.min(4,...sampleCounts):0;
    this.useMsaa=this.quality!=='performance'&&Number.isFinite(samples)&&samples>0;
    if(this.useMsaa){
      gl.bindFramebuffer(gl.FRAMEBUFFER,this.msaaFramebuffer);
      gl.bindRenderbuffer(gl.RENDERBUFFER,this.msaaColor);gl.renderbufferStorageMultisample(gl.RENDERBUFFER,samples,format,width,height);
      gl.framebufferRenderbuffer(gl.FRAMEBUFFER,gl.COLOR_ATTACHMENT0,gl.RENDERBUFFER,this.msaaColor);
      gl.bindRenderbuffer(gl.RENDERBUFFER,this.msaaDepth);gl.renderbufferStorageMultisample(gl.RENDERBUFFER,samples,gl.DEPTH_COMPONENT24,width,height);
      gl.framebufferRenderbuffer(gl.FRAMEBUFFER,gl.DEPTH_ATTACHMENT,gl.RENDERBUFFER,this.msaaDepth);
      this.useMsaa=gl.checkFramebufferStatus(gl.FRAMEBUFFER)===gl.FRAMEBUFFER_COMPLETE;
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER,null);
  }
  private draw(mesh:GpuMesh){this.gl.bindVertexArray(mesh.vao);this.gl.drawArrays(this.gl.TRIANGLES,0,mesh.count);this.drawCalls++;this.triangles+=mesh.count/3;}
  render(world:GpuMesh,objects:RenderObject[],camera:Camera,time:number,night:number,boost:number){
    if(this.disposed||this.gl.isContextLost())return;
    this.resize();this.drawCalls=0;this.triangles=0;
    const gl=this.gl,aspect=this.width/this.height,projection=perspective(camera.fov,aspect,.35,2200,camera.shift),vp=multiply(projection,lookAt(camera.position,camera.target));
    const atmosphere=(p:Program)=>{p.float('uTime',time);p.float('uNight',night);};
    gl.enable(gl.DEPTH_TEST);gl.depthMask(true);
    if(this.quality!=='performance'){
      gl.bindFramebuffer(gl.FRAMEBUFFER,this.shadowFramebuffer);gl.viewport(0,0,2048,2048);gl.clear(gl.DEPTH_BUFFER_BIT);
      gl.enable(gl.POLYGON_OFFSET_FILL);gl.polygonOffset(2,4);
      const p=this.shadowProgram;p.use();p.mat4('uLight',this.light);p.mat4('uModel',this.modelIdentity);this.draw(world);
      for(const object of objects){p.mat4('uModel',object.matrix);this.draw(object.mesh);}
      gl.disable(gl.POLYGON_OFFSET_FILL);
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER,this.useMsaa?this.msaaFramebuffer:this.sceneFramebuffer);gl.viewport(0,0,this.width,this.height);
    gl.clearColor(.3,.5,.7,1);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);
    gl.disable(gl.DEPTH_TEST);gl.depthMask(false);gl.bindVertexArray(this.blankVao);
    const forward=normalize(sub(camera.target,camera.position)),right=normalize(cross(forward,[0,1,0])),up=cross(right,forward);
    const sky=this.skyProgram;sky.use();atmosphere(sky);sky.vec3('uForward',forward);sky.vec3('uRight',right);sky.vec3('uUp',up);sky.float('uAspect',aspect);sky.float('uFov',Math.tan(camera.fov/2));sky.float('uShift',camera.shift);
    gl.drawArrays(gl.TRIANGLES,0,3);
    gl.enable(gl.DEPTH_TEST);gl.depthMask(true);
    const water=this.waterProgram;water.use();atmosphere(water);water.mat4('uViewProjection',vp);water.vec3('uCamera',camera.position);water.int('uRayTracing',this.quality==='high'?1:0);
    water.int('uEllipsoidCount',this.reflectors.length);
    water.vec3('uCenters[0]',new Float32Array(this.reflectors.flatMap(r=>r.center)));
    water.vec3('uRadii[0]',new Float32Array(this.reflectors.flatMap(r=>r.radii)));
    water.vec3('uColors[0]',new Float32Array(this.reflectors.flatMap(r=>r.color)));
    gl.drawArrays(gl.TRIANGLES,0,3);
    const p=this.meshProgram;p.use();atmosphere(p);p.mat4('uViewProjection',vp);p.mat4('uLight',this.light);p.vec3('uCamera',camera.position);p.int('uShadows',this.quality==='performance'?0:1);
    gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,this.shadowTexture);p.int('uShadow',0);
    p.mat4('uModel',this.modelIdentity);this.draw(world);
    for(const object of objects){p.mat4('uModel',object.matrix);this.draw(object.mesh);}
    if(this.useMsaa){
      gl.bindFramebuffer(gl.READ_FRAMEBUFFER,this.msaaFramebuffer);gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER,this.sceneFramebuffer);
      gl.blitFramebuffer(0,0,this.width,this.height,0,0,this.width,this.height,gl.COLOR_BUFFER_BIT,gl.NEAREST);
    }
    gl.bindFramebuffer(gl.FRAMEBUFFER,null);gl.viewport(0,0,this.width,this.height);gl.disable(gl.DEPTH_TEST);gl.depthMask(false);gl.bindVertexArray(this.blankVao);
    const post=this.postProgram;post.use();gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,this.sceneTexture);post.int('uScene',0);post.int('uBloom',this.quality==='performance'?0:1);post.float('uBoost',boost);
    gl.uniform2f(post.at('uTexel'),1/this.width,1/this.height);gl.drawArrays(gl.TRIANGLES,0,3);
    gl.depthMask(true);
  }
  get diagnostics(){return {renderer:'STRATUM / WebGL 2',quality:this.quality,rayTracing:this.quality==='high',hdr:this.hdr,msaa:this.useMsaa,resolution:[this.width,this.height],drawCalls:this.drawCalls,triangles:this.triangles};}
  dispose(){
    this.disposed=true;const gl=this.gl;
    for(const mesh of this.meshes){gl.deleteVertexArray(mesh.vao);gl.deleteBuffer(mesh.buffer);}
    for(const p of [this.meshProgram,this.skyProgram,this.waterProgram,this.shadowProgram,this.postProgram])gl.deleteProgram(p.handle);
    for(const texture of [this.shadowTexture,this.sceneTexture])gl.deleteTexture(texture);
    for(const fbo of [this.shadowFramebuffer,this.sceneFramebuffer,this.msaaFramebuffer])gl.deleteFramebuffer(fbo);
    for(const buffer of [this.sceneDepth,this.msaaColor,this.msaaDepth])gl.deleteRenderbuffer(buffer);
    gl.deleteVertexArray(this.blankVao);
  }
}
