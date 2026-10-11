import { add, cross, hex, normalize, scale, sub, TAU, type Vec3 } from './math.ts';

export type Material = [number, number];
const DEFAULT: Material = [.7, 0];
export class Geometry {
  data: number[] = [];
  vertex(p: Vec3, n: Vec3, color: Vec3, material = DEFAULT): void {
    this.data.push(...p, ...n, ...color, ...material);
  }
  tri(a: Vec3, b: Vec3, c: Vec3, color: Vec3, material = DEFAULT, normal?: Vec3): void {
    const n = normal || normalize(cross(sub(b, a), sub(c, a)));
    this.vertex(a, n, color, material); this.vertex(b, n, color, material); this.vertex(c, n, color, material);
  }
  quad(a: Vec3, b: Vec3, c: Vec3, d: Vec3, color: Vec3, material = DEFAULT, normal?: Vec3): void {
    this.tri(a, b, c, color, material, normal); this.tri(a, c, d, color, material, normal);
  }
  box(p: Vec3, size: Vec3, color: Vec3, material = DEFAULT): void {
    const [x, y, z] = p, [w, h, d] = size.map(s => s / 2);
    this.quad([x-w,y+h,z-d],[x-w,y+h,z+d],[x+w,y+h,z+d],[x+w,y+h,z-d],color,material,[0,1,0]);
    this.quad([x-w,y-h,z-d],[x+w,y-h,z-d],[x+w,y-h,z+d],[x-w,y-h,z+d],color,material,[0,-1,0]);
    this.quad([x-w,y-h,z+d],[x+w,y-h,z+d],[x+w,y+h,z+d],[x-w,y+h,z+d],color,material,[0,0,1]);
    this.quad([x+w,y-h,z-d],[x-w,y-h,z-d],[x-w,y+h,z-d],[x+w,y+h,z-d],color,material,[0,0,-1]);
    this.quad([x-w,y-h,z-d],[x-w,y-h,z+d],[x-w,y+h,z+d],[x-w,y+h,z-d],color,material,[-1,0,0]);
    this.quad([x+w,y-h,z+d],[x+w,y-h,z-d],[x+w,y+h,z-d],[x+w,y+h,z+d],color,material,[1,0,0]);
  }
  bevelBox(p: Vec3, size: Vec3, bevel: number, color: Vec3, material = DEFAULT): void {
    const [w,h,d] = size.map(v => v / 2);
    const outline = [[-w+bevel,-d],[w-bevel,-d],[w,-d+bevel],[w,d-bevel],[w-bevel,d],[-w+bevel,d],[-w,d-bevel],[-w,-d+bevel]];
    for (let i = 0; i < 8; i++) {
      const a = outline[i], b = outline[(i+1)%8];
      const a0 = add(p,[a[0],-h,a[1]]), b0=add(p,[b[0],-h,b[1]]);
      const a1 = add(p,[a[0]*.9,h,a[1]*.94]), b1=add(p,[b[0]*.9,h,b[1]*.94]);
      this.quad(a0,b0,b1,a1,color,material);
      this.tri(add(p,[0,h,0]),a1,b1,color,material,[0,1,0]);
      this.tri(add(p,[0,-h,0]),b0,a0,color,material,[0,-1,0]);
    }
  }
  ellipsoid(p: Vec3, radius: Vec3, color: Vec3, segments=20, rings=12, material = DEFAULT): void {
    const point = (a: number, b: number): [Vec3,Vec3] => {
      const n: Vec3 = [Math.sin(b)*Math.cos(a),Math.cos(b),Math.sin(b)*Math.sin(a)];
      return [add(p,[n[0]*radius[0],n[1]*radius[1],n[2]*radius[2]]),normalize([n[0]/radius[0],n[1]/radius[1],n[2]/radius[2]])];
    };
    for(let j=0;j<rings;j++) for(let i=0;i<segments;i++) {
      const a=point(i/segments*TAU,j/rings*Math.PI),b=point((i+1)/segments*TAU,j/rings*Math.PI),c=point((i+1)/segments*TAU,(j+1)/rings*Math.PI),d=point(i/segments*TAU,(j+1)/rings*Math.PI);
      for(const v of [a,b,c,a,c,d]) this.vertex(v[0],v[1],color,material);
    }
  }
  cylinder(a: Vec3,b: Vec3,r1: number,r2: number,color: Vec3,segments=12,material=DEFAULT): void {
    const axis=normalize(sub(b,a));
    const u=normalize(cross(axis,Math.abs(axis[1])>.9?[1,0,0]:[0,1,0])),v=cross(axis,u);
    for(let i=0;i<segments;i++) {
      const off=(j:number)=>add(scale(u,Math.cos(j/segments*TAU)),scale(v,Math.sin(j/segments*TAU)));
      const n1=off(i),n2=off(i+1),p1=add(a,scale(n1,r1)),p2=add(a,scale(n2,r1)),p3=add(b,scale(n2,r2)),p4=add(b,scale(n1,r2));
      for(const [p,n] of [[p1,n1],[p2,n2],[p3,n2],[p1,n1],[p3,n2],[p4,n1]] as [Vec3,Vec3][]) this.vertex(p,n,color,material);
      this.tri(a,p2,p1,color,material,scale(axis,-1)); this.tri(b,p4,p3,color,material,axis);
    }
  }
  torus(p:Vec3,normal:Vec3,radius:number,tube:number,color:Vec3,segments=48,sides=8,material=DEFAULT):void {
    const n=normalize(normal),u=normalize(cross(n,Math.abs(n[1])>.9?[1,0,0]:[0,1,0])),v=cross(n,u);
    const point=(i:number,j:number):[Vec3,Vec3]=>{
      const radial=add(scale(u,Math.cos(i/segments*TAU)),scale(v,Math.sin(i/segments*TAU)));
      const norm=add(scale(radial,Math.cos(j/sides*TAU)),scale(n,Math.sin(j/sides*TAU)));
      return [add(p,add(scale(radial,radius),scale(norm,tube))),norm];
    };
    for(let i=0;i<segments;i++) for(let j=0;j<sides;j++) {
      const a=point(i,j),b=point(i+1,j),c=point(i+1,j+1),d=point(i,j+1);
      for(const v of [a,b,c,a,c,d]) this.vertex(v[0],v[1],color,material);
    }
  }
  diamond(p:Vec3,size:number,color:Vec3,material=DEFAULT):void {
    const top=add(p,[0,size*1.35,0]),bottom=add(p,[0,-size*1.35,0]);
    for(let i=0;i<4;i++) {
      const a=add(p,[Math.cos(i*TAU/4)*size,0,Math.sin(i*TAU/4)*size]);
      const b=add(p,[Math.cos((i+1)*TAU/4)*size,0,Math.sin((i+1)*TAU/4)*size]);
      this.tri(top,a,b,color,material);this.tri(bottom,b,a,color,material);
    }
  }
}

export function kartGeometry(paint:string):Geometry {
  const g=new Geometry(),body=hex(paint),white=hex('#f7f1e7'),dark=hex('#17233d'),rubber=hex('#182337'),metal=hex('#a7c3cf'),glow=hex('#69ebff');
  g.bevelBox([0,.55,0],[2.15,.32,3.9],.35,dark,[.35,0]);
  g.bevelBox([0,.88,-.08],[2.0,.66,3.35],.42,body,[.28,0]);
  g.bevelBox([0,1.1,1.05],[1.7,.45,1.2],.28,body,[.3,0]);
  g.box([0,1.337,1.08],[.31,.025,.93],white,[.4,0]);
  g.box([0,.92,1.75],[.32,.37,.03],white);
  g.bevelBox([0,.5,1.94],[2.35,.19,.42],.12,dark,[.3,0]);
  g.box([0,.605,1.98],[1.4,.025,.18],glow,[.2,1.1]);
  g.bevelBox([0,1.18,-.78],[1.2,.72,1.15],.2,dark);
  for(const side of [-1,1]) {
    g.bevelBox([side*1.04,.74,0],[.38,.4,1.35],.15,body,[.25,0]);
    g.box([side*1.22,.63,0],[.04,.08,.94],glow,[.2,1.6]);
    for(const z of [-1.22,1.23]) {
      const c:Vec3=[side*1.22,.63,z];
      g.torus(c,[1,0,0],.45,.235,rubber,24,10,[.85,0]);
      g.cylinder(add(c,[-.2,0,0]),add(c,[.2,0,0]),.36,.36,dark,20,[.4,0]);
      g.torus(add(c,[side*.235,0,0]),[1,0,0],.285,.047,metal,20,6,[.23,0]);
      g.cylinder(add(c,[side*.22,0,0]),add(c,[side*.255,0,0]),.15,.15,body,12,[.25,0]);
      for(let k=0;k<5;k++) {
        const a=k/5*TAU;
        g.cylinder(add(c,[side*.24,Math.cos(a)*.13,Math.sin(a)*.13]),add(c,[side*.24,Math.cos(a)*.28,Math.sin(a)*.28]),.028,.028,metal,5,[.3,0]);
      }
    }
    g.bevelBox([side*.65,1.08,1.59],[.5,.15,.19],.05,white,[.3,1.8]);
    g.cylinder([side*.65,.7,-1.53],[side*.65,.78,-2.06],.19,.23,metal,12,[.25,0]);
    g.cylinder([side*.65,.78,-2.07],[side*.65,.78,-2.09],.16,.16,glow,12,[.2,2]);
    g.box([side*.75,1.34,-1.36],[.13,.72,.14],dark);
  }
  g.bevelBox([0,1.68,-1.55],[2.55,.17,.63],.13,body,[.3,0]);
  g.box([0,1.775,-1.55],[.35,.02,.52],white);
  g.ellipsoid([0,1.49,-.27],[.43,.49,.37],body,16,10,[.68,0]);
  g.bevelBox([0,1.52,.055],[.35,.42,.13],.055,white);
  g.ellipsoid([0,2.17,-.27],[.6,.63,.56],white,28,18,[.3,0]);
  g.ellipsoid([0,2.22,.087],[.54,.3,.35],dark,24,14,[.09,0]);
  g.ellipsoid([-.17,2.31,.376],[.16,.055,.035],hex('#bedee9'),12,6,[.05,.4]);
  g.ellipsoid([0,2.69,-.24],[.17,.115,.32],body,14,8,[.3,0]);
  for(const s of [-1,1]) {
    g.cylinder([s*.58,2.13,-.27],[s*.65,2.13,-.27],.15,.15,body,14,[.4,0]);
    g.cylinder([s*.31,1.69,-.23],[s*.47,1.42,.39],.16,.13,body,10);
    g.ellipsoid([s*.43,1.46,.5],[.17,.16,.16],white,12,8);
  }
  g.torus([0,1.44,.52],[0,.7,.7],.41,.055,dark,20,6);
  return g;
}
