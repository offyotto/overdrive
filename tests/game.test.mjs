import test from 'node:test';
import assert from 'node:assert/strict';
import { identity, multiply, transform, transformPoint, dot, length, mod, clamp, seededRandom } from '../src/engine/math.ts';
import { Track } from '../src/game/track.ts';
import { Race, formatTime } from '../src/game/race.ts';

const idle={throttle:false,brake:false,steer:0,drift:false};
const throttle={...idle,throttle:true};
function run(race,seconds,input=throttle){for(let i=0;i<Math.ceil(seconds*60);i++)race.step(1/60,typeof input==='function'?input(race):input);}
function ready(cc=100){const race=new Race(new Track());race.start(cc,'#ff7545');run(race,3.6,idle);race.drainEvents();return race;}

test('matrix operations preserve transforms and camera-independent model coordinates',()=>{
  const m=transform([10,2,-3],Math.PI/2,2);
  const p=transformPoint(m,[0,0,1]);
  assert.ok(Math.abs(p[0]-12)<1e-6);assert.ok(Math.abs(p[1]-2)<1e-6);assert.ok(Math.abs(p[2]+3)<1e-6);
  assert.deepEqual(Array.from(multiply(identity(),m)),Array.from(m));
  assert.equal(mod(-1,10),9);assert.equal(clamp(30,0,20),20);
});
test('procedural generation is reproducible',()=>{
  const a=seededRandom(42),b=seededRandom(42);
  for(let i=0;i<200;i++){const value=a();assert.equal(value,b());assert.ok(value>=0&&value<1);}
});
test('circuit is closed, arc-length sampled, and has orthonormal frames',()=>{
  const track=new Track();assert.ok(track.length>700&&track.length<1400);
  for(let d=-track.length;d<track.length*2;d+=7.7){
    const s=track.at(d);assert.ok(Math.abs(length(s.tangent)-1)<1e-6);assert.ok(Math.abs(length(s.right)-1)<1e-6);assert.ok(Math.abs(dot(s.tangent,s.right))<.002);
    assert.ok(s.position.every(Number.isFinite));assert.ok(Number.isFinite(track.curvature(d)));
  }
  assert.deepEqual(track.at(0).position,track.at(track.length).position);
  const a=track.at(.001).position,b=track.at(track.length-.001).position;
  assert.ok(Math.hypot(...a.map((v,i)=>v-b[i]))<.005);
  assert.ok(track.mapPath(140,100).startsWith('M'));assert.ok(track.mapPath(140,100).endsWith('Z'));
});
test('the eight-kart starting grid has no overlapping vehicles',()=>{
  const race=new Race(new Track());race.start(100,'#ff7545');assert.equal(race.position,4);
  for(let i=0;i<race.racers.length;i++)for(let j=i+1;j<race.racers.length;j++){
    const a=race.racers[i],b=race.racers[j];
    assert.ok(Math.abs(a.distance-b.distance)>=4||Math.abs(a.lateral-b.lateral)>=2.6);
  }
});
test('countdown does not advance racers and can be paused safely',()=>{
  const race=new Race(new Track());race.start(100,'#fff');
  const position=race.player.distance;run(race,1,throttle);assert.equal(race.player.distance,position);assert.equal(race.phase,'countdown');
  race.pause();const before=race.countdown;run(race,4,throttle);assert.equal(race.countdown,before);assert.equal(race.phase,'paused');
  race.resume();run(race,3);assert.equal(race.phase,'racing');assert.ok(race.player.distance>position);
});
test('accelerator, brake, and engine classes change actual kart speed',()=>{
  const slow=ready(50),fast=ready(150);run(slow,2);run(fast,2);assert.ok(fast.player.speed>slow.player.speed+12);
  assert.ok(slow.player.distance>-14);const speed=fast.player.speed;run(fast,1,{...idle,brake:true});assert.ok(fast.player.speed<speed*.15);
});
test('turbo consumes energy, cannot spam, and is unavailable outside a race',()=>{
  const race=ready();race.player.energy=100;assert.equal(race.useBoost(),true);assert.equal(race.player.energy,65);assert.equal(race.useBoost(),false);
  run(race,.6);assert.ok(race.player.boost>0);race.pause();assert.equal(race.useBoost(),false);
  race.resume();race.player.boost=0;race.player.energy=10;assert.equal(race.useBoost(),false);
});
test('charging and releasing a drift earns a mini-turbo',()=>{
  const race=ready();run(race,1.2);run(race,.8,{...throttle,steer:.5,drift:true});
  assert.equal(race.player.drifting,true);assert.ok(race.player.driftCharge>.2);
  race.step(1/60,throttle);assert.equal(race.player.drifting,false);assert.ok(race.player.boost>0);assert.equal(race.player.driftCharge,0);
  assert.ok(race.drainEvents().some(e=>e.kind==='drift'));
});
test('guardrails prevent leaving the track without NaN or negative speeds',()=>{
  const race=ready(150);run(race,8,{...throttle,steer:1});
  assert.ok(Math.abs(race.player.lateral)<=race.track.width/2-1.17);assert.ok(race.player.speed>=0);
  for(const r of race.racers)assert.ok([r.distance,r.lateral,r.speed,r.yaw].every(Number.isFinite));
});
test('pickups are collected on crossing and all three items have gameplay effects',()=>{
  const race=ready();race.player.distance=race.track.pickupZones[0]-21-.01;race.player.speed=40;race.step(1/60,throttle);
  assert.ok(race.player.item);assert.ok(race.drainEvents().some(e=>e.kind==='pickup'));
  race.player.item='shield';assert.equal(race.useItem(),true);assert.equal(race.player.shield,6);assert.equal(race.player.item,null);
  race.player.item='turbo';race.useItem();assert.ok(race.player.boost>=2.8);
  race.racers[1].distance=race.player.distance+10;race.player.item='pulse';race.useItem();assert.equal(race.racers[1].slowed,3.5);
});
test('lap counting ignores the starting grid, records each lap, and finishes once',()=>{
  const race=ready();assert.equal(race.lap,1);assert.equal(race.lapTimes.length,0);
  for(let lap=1;lap<=3;lap++){
    race.player.distance=race.track.length*lap-.01;race.player.speed=40;race.step(1/60,throttle);
    assert.equal(race.lapTimes.length,lap);
  }
  assert.equal(race.phase,'finished');assert.ok(Number.isFinite(race.player.finishTime));assert.equal(race.lap,3);
  const finish=race.player.finishTime;run(race,2);assert.equal(race.player.finishTime,finish);assert.equal(race.lapTimes.length,3);
});
test('full three-lap race finishes with valid standings and autonomous AI rivals',()=>{
  const race=ready();
  for(let frame=0;frame<60*180&&race.phase!=='finished';frame++){
    const p=race.player,curve=race.track.curvature(21+p.distance);
    race.step(1/60,{...throttle,steer:clamp(-p.lateral*.2+curve*p.speed*.17,-1,1)});
    if(frame%240===0)race.useBoost();
  }
  assert.equal(race.phase,'finished');assert.equal(race.lapTimes.length,3);assert.ok(race.lapTimes.every(t=>t>0));
  assert.ok(race.position>=1&&race.position<=8);assert.equal(new Set(race.standings.map(r=>r.id)).size,8);
  run(race,100);assert.ok(race.racers.every(r=>r.finishTime!==null));
  const times=race.standings.map(r=>r.finishTime);assert.deepEqual(times,[...times].sort((a,b)=>a-b));
});
test('pause freezes the race and restart clears transient state',()=>{
  const race=ready();run(race,4);race.useBoost();race.pause();const snapshot=race.snapshot();run(race,2);
  assert.deepEqual(race.snapshot(),snapshot);race.start(50,'#47b9e5');assert.equal(race.phase,'countdown');assert.equal(race.elapsed,0);
  assert.equal(race.player.boost,0);assert.equal(race.player.item,null);assert.equal(race.player.energy,100);assert.equal(race.lapTimes.length,0);assert.equal(race.player.color,'#47b9e5');
});
test('invalid time steps cannot corrupt state and time formatting is stable',()=>{
  const race=ready();const before=race.snapshot();race.step(NaN,throttle);race.step(-1,throttle);race.step(Infinity,throttle);assert.deepEqual(race.snapshot(),before);
  assert.equal(formatTime(65.12),'01:05.12');assert.equal(formatTime(0),'00:00.00');assert.equal(formatTime(NaN),'—');
  assert.equal(formatTime(59.999),'01:00.00');assert.equal(formatTime(119.999),'02:00.00');
});
