import { clamp, damp, mod, seededRandom } from '../engine/math.ts';
import { Track } from './track.ts';

export type Phase='menu'|'countdown'|'racing'|'paused'|'finished';
export type Item='turbo'|'shield'|'pulse';
export type RaceClass=50|100|150;
export interface Input { throttle:boolean; brake:boolean; steer:number; drift:boolean; }
export interface Racer {
  id:number; name:string; color:string; distance:number; lateral:number; lateralVelocity:number;
  speed:number; yaw:number; drifting:boolean; driftCharge:number; boost:number; energy:number;
  coins:number; item:Item|null; shield:number; slowed:number; finishTime:number|null; personality:number;
}
export interface RaceEvent { kind:'countdown'|'go'|'boost'|'drift'|'pickup'|'coin'|'impact'|'lap'|'finish'|'shield'|'pulse'; text?:string; value?:number; }
export const PAINTS=[{name:'Solar orange',hex:'#ff7545'},{name:'Lagoon blue',hex:'#47b9e5'},{name:'Cosmic lilac',hex:'#b49ae7'},{name:'Mint condition',hex:'#62d9b0'},{name:'Cherry pop',hex:'#ed6076'},{name:'Polar white',hex:'#e8eeee'}];
const NAMES=['You','Nova','Milo','Indie','Jett','Rio','Cleo','Dash'];
const COLORS=['#ff7545','#52c6db','#aa8ee2','#f1c867','#ee6d93','#64d5a5','#769eef','#df98ca'];
export class Race {
  readonly track:Track;
  phase:Phase='menu';
  raceClass:RaceClass=100;
  racers:Racer[]=[];
  elapsed=0;
  countdown=3.5;
  laps=3;
  lapTimes:number[]=[];
  lapStartedAt=0;
  events:RaceEvent[]=[];
  bestLap:number|null=null;
  private random=seededRandom(82);
  private impactCooldown=0;
  private lastCountdown=4;
  private resumePhase:'countdown'|'racing'='racing';
  constructor(track:Track){this.track=track;this.resetRacers();}
  get player(){return this.racers[0];}
  get maxSpeed(){return this.raceClass===50?39:this.raceClass===100?51:65;}
  get lap(){return Math.min(this.laps,Math.floor(Math.max(0,this.player.distance)/this.track.length)+1);}
  get position(){return this.standings.findIndex(r=>r.id===0)+1;}
  get standings(){return [...this.racers].sort((a,b)=>a.finishTime!==null&&b.finishTime!==null?a.finishTime-b.finishTime:a.finishTime!==null?-1:b.finishTime!==null?1:b.distance-a.distance);}
  private resetRacers(){
    this.racers=NAMES.map((name,i)=>{
      const slot=i===0?3:i<4?i-1:i;
      return {id:i,name,color:COLORS[i],distance:-slot*3.4,lateral:slot%2?2.1:-2.1,lateralVelocity:0,speed:0,yaw:0,drifting:false,driftCharge:0,boost:0,energy:100,coins:0,item:null,shield:0,slowed:0,finishTime:null,personality:.82+i*.015};
    });
  }
  start(raceClass:RaceClass,paint:string){
    this.raceClass=raceClass;this.resetRacers();this.player.color=paint;this.elapsed=0;this.countdown=3.5;this.lastCountdown=4;
    this.lapTimes=[];this.lapStartedAt=0;this.events=[];this.impactCooldown=0;this.random=seededRandom(82);this.phase='countdown';
  }
  pause(){if(this.phase==='racing'||this.phase==='countdown'){this.resumePhase=this.phase;this.phase='paused';}}
  resume(){if(this.phase==='paused')this.phase=this.resumePhase;}
  menu(){this.phase='menu';this.events=[];}
  useBoost(){
    const p=this.player;
    if(this.phase!=='racing'||p.energy<35||p.boost>.5)return false;
    p.energy-=35;p.boost=1.65;this.events.push({kind:'boost',text:'TURBO ENGAGED'});return true;
  }
  useItem(){
    const p=this.player;
    if(this.phase!=='racing'||!p.item)return false;
    if(p.item==='turbo'){p.boost=Math.max(p.boost,2.8);this.events.push({kind:'boost',text:'SUPER TURBO'});}
    if(p.item==='shield'){p.shield=6;this.events.push({kind:'shield',text:'SHIELD ONLINE'});}
    if(p.item==='pulse'){
      for(const r of this.racers.slice(1))if(Math.abs(r.distance-p.distance)<45)r.slowed=3.5;
      this.events.push({kind:'pulse',text:'PULSE RELEASED'});
    }
    p.item=null;return true;
  }
  private crossed(oldDistance:number,newDistance:number,position:number){return Math.floor((newDistance-position)/this.track.length)>Math.floor((oldDistance-position)/this.track.length);}
  step(dt:number,input:Input){
    if(!Number.isFinite(dt)||dt<=0)return;
    dt=Math.min(dt,.05);
    if(this.phase==='menu'||this.phase==='paused')return;
    if(this.phase==='countdown'){
      this.countdown-=dt;
      const n=Math.ceil(this.countdown);
      if(n!==this.lastCountdown&&n>0&&n<=3){this.events.push({kind:'countdown',value:n});this.lastCountdown=n;}
      if(this.countdown<=0){this.phase='racing';this.events.push({kind:'go',text:'LET’S FLY'});}
      return;
    }
    this.elapsed+=dt;this.impactCooldown=Math.max(0,this.impactCooldown-dt);
    for(const racer of this.racers){
      if(racer.finishTime!==null)continue;
      const old=racer.distance,curve=this.track.curvature(21+racer.distance);
      racer.boost=Math.max(0,racer.boost-dt);racer.shield=Math.max(0,racer.shield-dt);racer.slowed=Math.max(0,racer.slowed-dt);
      if(racer.id===0){
        const wasDrifting=racer.drifting;
        racer.drifting=input.drift&&Math.abs(input.steer)>.05&&racer.speed>10;
        if(racer.drifting)racer.driftCharge=clamp(racer.driftCharge+dt*.47,0,1);
        else if(wasDrifting){
          if(racer.driftCharge>.17){
            racer.boost=Math.max(racer.boost,.5+racer.driftCharge*1.9);racer.energy=clamp(racer.energy+racer.driftCharge*20,0,100);
            this.events.push({kind:'drift',text:racer.driftCharge>.75?'ULTRA MINI-TURBO':'MINI-TURBO',value:racer.driftCharge});
          }
          racer.driftCharge=0;
        }
        const speedLimit=this.maxSpeed*(racer.boost>0?1.43:1)*(racer.slowed>0?.58:1);
        const target=input.brake?0:input.throttle?speedLimit:0;
        racer.speed=damp(racer.speed,target,input.brake?2.7:input.throttle?1.03:.5,dt);
        const centrifugal=curve*racer.speed*racer.speed*(this.raceClass===50?.012:.035);
        const sideSpeed=input.steer*racer.speed*(racer.drifting?.30:.21)-centrifugal;
        racer.lateralVelocity=damp(racer.lateralVelocity,sideSpeed,racer.drifting?3:7,dt);
        racer.lateral+=racer.lateralVelocity*dt;
        const edge=this.track.width/2-1.18;
        if(Math.abs(racer.lateral)>edge){
          racer.lateral=clamp(racer.lateral,-edge,edge);racer.lateralVelocity*=-.22;
          if(this.impactCooldown===0&&racer.speed>9){
            if(!racer.shield)racer.speed*=.81;
            this.impactCooldown=.65;this.events.push({kind:'impact'});
          }
        }
        racer.yaw=damp(racer.yaw,input.steer*(racer.drifting?.48:.21),7,dt);
        racer.energy=clamp(racer.energy+dt*(racer.speed>10?3.2:1.4),0,100);
      }else{
        const bend=clamp(1-Math.abs(curve)*7.5,.76,1);
        const desired=this.maxSpeed*racer.personality*bend*(racer.boost>0?1.28:1)*(racer.slowed>0?.52:1);
        racer.speed=damp(racer.speed,desired,.75,dt);
        let lane=Math.sin(racer.distance*.015+racer.id*1.8)*3.6;
        for(const ahead of this.racers)if(ahead.id!==racer.id&&ahead.distance>racer.distance&&ahead.distance-racer.distance<9&&Math.abs(ahead.lateral-lane)<1.7)lane=clamp(ahead.lateral+(racer.id%2?2.8:-2.8),-5,5);
        const before=racer.lateral;racer.lateral=damp(racer.lateral,lane,1.2,dt);racer.yaw=damp(racer.yaw,(racer.lateral-before)/dt*.03,5,dt);
        if(Math.floor((this.elapsed+racer.id*3)/12)!==Math.floor((this.elapsed-dt+racer.id*3)/12))racer.boost=.9;
      }
      racer.distance+=racer.speed*dt;
      for(const pad of this.track.boostPads)if(this.crossed(old,racer.distance,pad-21)&&Math.abs(racer.lateral)<4.6){
        racer.boost=Math.max(racer.boost,1.1);if(racer.id===0)this.events.push({kind:'boost',text:'SLIPSTREAM BOOST'});
      }
      if(racer.id===0){
        for(const zone of this.track.pickupZones){
          if(this.crossed(old,racer.distance,zone-21)&&!racer.item){
            racer.item=(['turbo','shield','pulse'] as Item[])[Math.floor(this.random()*3)];
            this.events.push({kind:'pickup',text:racer.item==='turbo'?'SUPER TURBO':racer.item==='shield'?'ENERGY SHIELD':'SONIC PULSE'});
          }
          for(let j=0;j<4;j++)if(this.crossed(old,racer.distance,zone+14+j*4-21)&&Math.abs(racer.lateral)<2.5){
            racer.coins++;racer.energy=clamp(racer.energy+2,0,100);this.events.push({kind:'coin'});
          }
        }
        const previousLap=Math.max(0,Math.floor(old/this.track.length)),nextLap=Math.floor(racer.distance/this.track.length);
        if(nextLap>previousLap){
          const lapTime=this.elapsed-this.lapStartedAt;this.lapTimes.push(lapTime);this.lapStartedAt=this.elapsed;
          if(this.bestLap===null||lapTime<this.bestLap)this.bestLap=lapTime;
          if(nextLap<this.laps)this.events.push({kind:'lap',text:nextLap===this.laps-1?'FINAL LAP':'LAP 2',value:lapTime});
        }
      }
      if(racer.distance>=this.track.length*this.laps){
        racer.finishTime=this.elapsed-(racer.distance-this.track.length*this.laps)/(racer.speed||1);
        racer.distance=this.track.length*this.laps;
        if(racer.id===0){this.phase='finished';this.events.push({kind:'finish',text:'THAT’S A WRAP'});}
      }
    }
    const player=this.player;
    if(this.phase==='racing'&&!player.shield&&this.impactCooldown===0){
      for(const opponent of this.racers.slice(1)){
        const gap=opponent.distance-player.distance;
        if(Math.abs(gap)<2.6&&Math.abs(opponent.lateral-player.lateral)<1.65){
          player.lateral=clamp(player.lateral+(player.lateral>opponent.lateral?.3:-.3),-6.3,6.3);
          if(gap>0)player.speed*=.9;
          this.impactCooldown=.65;this.events.push({kind:'impact'});break;
        }
      }
    }
  }
  drainEvents(){const result=this.events;this.events=[];return result;}
  snapshot(){return {phase:this.phase,elapsed:this.elapsed,countdown:this.countdown,lap:this.lap,position:this.position,trackLength:this.track.length,lapTimes:[...this.lapTimes],player:{...this.player},racers:this.racers.map(r=>({id:r.id,distance:r.distance,lateral:r.lateral,speed:r.speed,finishTime:r.finishTime})),progress:mod(this.player.distance,this.track.length)/this.track.length};}
}
export function formatTime(seconds:number):string {
  if(!Number.isFinite(seconds)||seconds<0)return '—';
  const hundredths=Math.round(seconds*100),minutes=Math.floor(hundredths/6000),rest=(hundredths%6000)/100;
  return `${String(minutes).padStart(2,'0')}:${rest.toFixed(2).padStart(5,'0')}`;
}
