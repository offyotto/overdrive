import { Geometry, type Material } from './geometry.ts';
import { add, hex, mix3, scale, seededRandom, TAU, type Vec3 } from './math.ts';
import { Track } from '../game/track.ts';

export interface Reflector { center:Vec3; radii:Vec3; color:Vec3; }
export interface World { geometry:Geometry; reflectors:Reflector[]; }
export function createWorld(track:Track):World {
  const g=new Geometry(),random=seededRandom(91),reflectors:Reflector[]=[];
  const road=hex('#656f91'),edge=hex('#eeeee8'),orange=hex('#ff7847'),cyan=hex('#71f4ef'),rock=hex('#bd9390');
  const up:Vec3=[0,1,0];
  const p=(distance:number,lateral:number,lift=0)=>track.position(distance,lateral,lift);
  const band=(d:number,next:number,a:number,b:number,lift:number,color:Vec3,mat:Material=[.72,0])=>g.quad(p(d,a,lift),p(next,a,lift),p(next,b,lift),p(d,b,lift),color,mat,up);
  for(let i=0;i<track.samples.length-1;i++) {
    const d=track.samples[i].distance,next=track.samples[i+1].distance;
    band(d,next,-8.1,8.1,-.28,hex('#8998b1'));
    band(d,next,-7.5,7.5,0,road,[.96,0]);
    for(const side of [-1,1]) {
      band(d,next,side*6.55,side*6.68,.035,edge);
      band(d,next,side*7.1,side*7.52,.04,Math.floor(d/3)%2?orange:edge);
      band(d,next,side*7.75,side*8.05,.34,edge);
      band(d,next,side*7.8,side*7.95,.62,cyan,[.25,1.15]);
      const a=p(d,side*8,-.3),b=p(next,side*8,-.3),c=p(next,side*8,.5),dd=p(d,side*8,.5);
      g.quad(a,b,c,dd,edge);
      g.quad(p(d,side*7.7,.4),p(next,side*7.7,.4),p(next,side*7.7,1.15),p(d,side*7.7,1.15),hex('#a8cbd4'),[.4,0]);
      band(d,next,side*7.68,side*7.8,1.16,cyan,[.3,.7]);
    }
    if(Math.floor(d/5)%3===0)band(d,next,-.075,.075,.025,hex('#b6c3d7'));
  }
  for(let d=0;d<track.length;d+=13) {
    const s=track.at(d);
    for(const side of [-1,1]) {
      const base=p(d,side*7.8,.15);
      g.cylinder(base,add(base,[0,1.6,0]),.1,.075,hex('#f4efec'),8);
      g.ellipsoid(add(base,[0,1.67,0]),[.15,.15,.15],cyan,8,5,[.25,1.5]);
    }
    if(Math.floor(d/13)%3===0)g.cylinder(add(s.position,[0,-1,0]),[s.position[0],-12,s.position[2]],1.1,.55,hex('#d7d0c3'),10);
  }
  for(let lane=0;lane<12;lane++)for(let row=0;row<3;row++)band(19+row*.75,19+(row+1)*.75,-7+lane*14/12,-7+(lane+1)*14/12,.07,(lane+row)%2?hex('#eff9f7'):hex('#29394e'));
  const gate=track.at(22);
  for(const side of [-1,1]) {
    const b=p(22,side*8.5,-1);
    g.cylinder(b,add(b,[0,10,0]),.57,.4,edge,12);
    g.cylinder(add(b,[0,3,0]),add(b,[0,8.7,0]),.59,.42,orange,12,[.4,.15]);
  }
  g.cylinder(p(22,-9,9),p(22,9,9),.8,.8,orange,12,[.4,.15]);
  for(let i=-3;i<=3;i++) {
    const c=add(add(gate.position,scale(gate.right,i*1.35)),[0,9,0]);
    g.ellipsoid(c,[.36,.36,.36],edge,10,6,[.3,.7]);
  }
  for(const d of track.boostPads) {
    for(let k=0;k<6;k++) {
      const d0=d+k*.75;
      g.quad(p(d0,-4,.07),p(d0+1.35,0,.07),p(d0+.52,0,.07),p(d0-.83,-4,.07),cyan,[.3,.9],up);
      g.quad(p(d0+1.35,0,.07),p(d0,4,.07),p(d0-.83,4,.07),p(d0+.52,0,.07),cyan,[.3,.9],up);
    }
    for(const side of [-1,1]) {
      const a=p(d+2,side*8.1,0);
      g.cylinder(a,add(a,[0,5,0]),.12,.1,edge,8);
      const b=add(a,scale(track.at(d).tangent,2.7));
      g.tri(add(a,[0,5,0]),add(a,[0,3.5,0]),add(b,[0,4.25,0]),orange,[.6,.1]);
    }
  }
  function palm(base:Vec3,height:number,angle:number) {
    const lean:Vec3=[Math.cos(angle)*height*.15,0,Math.sin(angle)*height*.15];
    const top=add(add(base,[0,height,0]),lean);
    for(let i=0;i<7;i++) {
      const a=mix3(base,top,i/7),b=mix3(base,top,(i+1)/7);
      g.cylinder(a,b,.3*(1-i/12),.3*(1-(i+1)/12),i%2?hex('#b88671'):hex('#c19779'),8);
    }
    for(let j=0;j<7;j++) {
      const a=j/7*TAU+angle,len=height*(.43+random()*.16),direction:Vec3=[Math.cos(a),0,Math.sin(a)],side:Vec3=[-Math.sin(a),0,Math.cos(a)];
      for(let k=0;k<5;k++) {
        const t=k/5,t2=(k+1)/5;
        const center=(v:number)=>add(top,add(scale(direction,len*v),[0,Math.sin(v*Math.PI)*len*.21-v*v*len*.4,0]));
        const c=center(t),cc=center(t2),w=Math.sin(t*Math.PI)*len*.16,ww=Math.sin(t2*Math.PI)*len*.16;
        const col=hex(j%2?'#4aa992':'#63c997');
        g.quad(add(c,scale(side,-w)),add(cc,scale(side,-ww)),add(cc,[0,.13,0]),add(c,[0,.13,0]),col);
        g.quad(add(c,[0,.13,0]),add(cc,[0,.13,0]),add(cc,scale(side,ww)),add(c,scale(side,w)),hex('#7edcab'));
      }
    }
    for(let i=0;i<3;i++)g.ellipsoid(add(top,[Math.cos(i*2)*.35,-.35,Math.sin(i*2)*.35]),[.28,.32,.28],hex('#896b59'),8,5);
  }
  function island(center:Vec3,radius:number,depth:number,trees:number) {
    const segments=15,ring:number[]=[];
    for(let i=0;i<segments;i++)ring.push(.83+random()*.24);
    const vertex=(i:number,level:number):Vec3=>{
      const a=i/segments*TAU,r=radius*ring[i%segments]*[1,.97,.57,.12][level];
      return add(center,[Math.cos(a)*r,[0,-depth*.2,-depth*.62,-depth][level],Math.sin(a)*r]);
    };
    for(let i=0;i<segments;i++) {
      const tint=mix3(rock,hex('#667f9b'),random()*.35);
      g.tri(center,vertex(i+1,0),vertex(i,0),hex(i%3?'#46ad8b':'#62be93'),[.8,0],up);
      for(let l=0;l<3;l++)g.quad(vertex(i,l),vertex(i+1,l),vertex(i+1,l+1),vertex(i,l+1),tint);
      const a=vertex(i,0),b=vertex(i+1,0);
      g.quad(a,b,add(b,[0,-1.8,0]),add(a,[0,-1.8,0]),hex('#ead5ac'));
    }
    for(let i=0;i<trees;i++) {
      const a=random()*TAU,r=radius*(.42+random()*.39);
      const base=add(center,[Math.cos(a)*r,.2,Math.sin(a)*r]);
      const nearest=track.samples.reduce((d,s)=>Math.min(d,Math.hypot(s.position[0]-base[0],s.position[2]-base[2])),Infinity);
      if(nearest>10)palm(base,9+random()*7,a);
      if(nearest>14)for(let k=0;k<4;k++){
        const offset:Vec3=[Math.cos(k*1.8)*2.3,.8,Math.sin(k*1.8)*2.3];
        g.ellipsoid(add(base,offset),[1.8,1.35,1.6],hex(k%2?'#278e76':'#5ac59b'),12,8);
        if(k%2===0)g.ellipsoid(add(add(base,offset),[.6,1.25,.3]),[.32,.28,.32],hex('#ffc18c'),10,6);
      }
    }
    for(let i=0;i<5;i++) {
      const a=random()*TAU,rr=radius*(.95+random()*.15);
      g.ellipsoid(add(center,[Math.cos(a)*rr,-depth*.6-random()*12,Math.sin(a)*rr]),[2+random()*3,3+random()*4,2+random()*3],hex('#a6a6b9'),9,6);
    }
  }
  const islands:[Vec3,number,number,number][]=[
    [[-77,17,91],28,39,8],[[57,22,79],40,49,12],[[100,31,12],30,51,9],
    [[50,40,-76],43,64,13],[[-66,26,-76],45,50,13],[[-85,19,-11],30,40,9],[[-12,22,24],29,39,8],
    [[166,17,-117],35,60,13],[[-157,23,-163],49,70,16],[[191,6,92],22,46,7],
  ];
  for(const [c,r,d,t] of islands)island(c,r,d,t);
  for(const [c,r] of islands.slice(0,9)) {
    const center=add(c,[0,-13,0]),radii:Vec3=[r*.7,12,r*.7],color=hex('#9eacb4');
    g.ellipsoid(center,radii,color,18,10);
    reflectors.push({center,radii,color});
  }
  for(const [center,radius] of [ [[-190,-12,-260],65],[[190,-9,-240],60],[[290,-23,-80],70] ] as [Vec3,number][]) {
    const radii:Vec3=[radius,24,radius*.7],color=hex('#a4b7bf');
    g.ellipsoid(center,radii,color,22,12);reflectors.push({center,radii,color});
  }
  for(const [c,r] of islands.filter((_,i)=>i===2||i===4||i===7)) {
    const waterfall=add(c,[r*.72,-1,r*.3]);
    for(let k=0;k<5;k++) {
      const x=waterfall[0]+k*.62;
      g.quad([x,waterfall[1],waterfall[2]],[x+.6,waterfall[1],waterfall[2]],[x+.6,-37,waterfall[2]+5],[x,-37,waterfall[2]+5],hex(k%2?'#a1eee7':'#68d6dd'),[.2,.2]);
    }
    g.ellipsoid([waterfall[0]+1.5,-37,waterfall[2]+5],[7,.25,5],hex('#b7ede7'),18,8,[.7,.1]);
  }
  const tower:Vec3=[55,40,-83];
  g.cylinder(tower,add(tower,[0,34,0]),3,2.2,hex('#f4e7d0'),16);
  for(let i=0;i<3;i++)g.cylinder(add(tower,[0,8+i*8,0]),add(tower,[0,10+i*8,0]),2.83-i*.19,2.78-i*.19,orange,16);
  g.cylinder(add(tower,[0,33,0]),add(tower,[0,35,0]),4.2,4.2,hex('#344962'),16);
  g.cylinder(add(tower,[0,35,0]),add(tower,[0,40,0]),2.7,2.7,cyan,16,[.2,.6]);
  g.cylinder(add(tower,[0,40,0]),add(tower,[0,44,0]),4.2,0,orange,16);
  g.torus(add(tower,[0,21,0]),[.12,1,.24],12,.42,orange,64,8,[.35,.2]);
  g.torus(add(tower,[0,24,0]),[-.3,1,.13],16,.19,cyan,64,6,[.3,1.2]);
  for(let i=0;i<20;i++) {
    const a=i/20*TAU,r=290+random()*180;
    const c:Vec3=[Math.cos(a)*r,random()*40-22,Math.sin(a)*r];
    for(let j=0;j<5;j++)g.ellipsoid(add(c,[(j-2)*12,random()*6,random()*10]),[20+random()*10,8+random()*8,15+random()*12],hex('#edf0ef'),16,9,[.86,.04]);
  }
  function balloon(c:Vec3,size:number,color:Vec3) {
    const segments=24,rings=16;
    const pt=(i:number,j:number):Vec3=>{
      const t=j/rings*Math.PI,r=Math.sin(t)*(1.+Math.cos(t)*.16)*size;
      return add(c,[Math.cos(i/segments*TAU)*r,Math.cos(t)*size*1.2,Math.sin(i/segments*TAU)*r]);
    };
    for(let j=0;j<rings;j++)for(let i=0;i<segments;i++)g.quad(pt(i,j),pt(i+1,j),pt(i+1,j+1),pt(i,j+1),Math.floor(i/3)%2?color:hex('#fff0d7'),[.6,0]);
    const bottom=add(c,[0,-size*1.75,0]);
    g.bevelBox(bottom,[size*.42,size*.3,size*.42],.2,hex('#b98e69'));
    for(const x of [-1,1])for(const z of [-1,1])g.cylinder(add(bottom,[x*size*.18,0,z*size*.18]),add(c,[x*size*.2,-size,z*size*.2]),.065,.065,hex('#e2d6b7'),6);
  }
  balloon([111,91,-85],9,orange);
  balloon([-138,79,-128],6,hex('#82b9d9'));
  balloon([192,69,28],7,hex('#eec26e'));
  for(let i=0;i<16;i++) {
    const d=i/16*track.length,s=track.at(d),side=i%2?1:-1;
    const b=add(s.position,scale(s.right,side*10));
    g.cylinder(b,add(b,[0,5.2,0]),.08,.08,edge,7);
    const tangent=scale(s.tangent,2.4);
    g.tri(add(b,[0,5.2,0]),add(b,[0,3.7,0]),add(add(b,tangent),[0,4.6,0]),i%2?orange:cyan);
  }
  return {geometry:g,reflectors};
}
