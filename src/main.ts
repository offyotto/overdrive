import './style.css';
import { createUI, icon } from './ui.ts';
import { Geometry, kartGeometry } from './engine/geometry.ts';
import { add, clamp, damp, hex, mix3, multiply, scale, transform } from './engine/math.ts';
import { Renderer, type Camera, type Quality, type RenderObject } from './engine/renderer.ts';
import { createWorld } from './engine/world.ts';
import { AudioSystem } from './game/audio.ts';
import { Race, PAINTS, formatTime, type Input, type RaceClass, type RaceEvent } from './game/race.ts';
import { Track } from './game/track.ts';

const track=new Track();
const app=document.querySelector<HTMLDivElement>('#app')!;
app.innerHTML=createUI(track);
const el=<T extends HTMLElement=HTMLElement>(id:string)=>document.getElementById(id) as T;
const dialog=(id:string)=>el<HTMLDialogElement>(id);
const storage={
  get(key:string){try{return localStorage.getItem(`overdrive.${key}`);}catch{return null;}},
  set(key:string,value:string){try{localStorage.setItem(`overdrive.${key}`,value);}catch{/* Storage is optional in private browsing. */}},
};
function showError(error:unknown){
  console.error(error);
  el('loading').hidden=true;el('game-error').hidden=false;
  el('error-message').textContent=error instanceof Error?error.message:'The graphics engine could not start. Try another browser or enable hardware acceleration.';
}
el('reload-button').addEventListener('click',()=>window.location.reload());
function boot(){
  const renderer=new Renderer(el<HTMLCanvasElement>('game-canvas'));
  const world=createWorld(track),worldMesh=renderer.upload(world.geometry);renderer.setReflectors(world.reflectors);
  const race=new Race(track),audio=new AudioSystem();
  const playerMeshes=PAINTS.map(p=>renderer.upload(kartGeometry(p.hex)));
  const rivalMeshes=race.racers.slice(1).map(r=>renderer.upload(kartGeometry(r.color)));
  const crystal=new Geometry();
  crystal.diamond([0,0,0],.69,hex('#8df8e6'),[.12,.62]);
  crystal.torus([0,0,0],[0,1,0],1.03,.045,hex('#ddffff'),24,5,[.3,1.1]);
  const crystalMesh=renderer.upload(crystal);
  const coin=new Geometry();
  coin.cylinder([0,0,-.07],[0,0,.07],.43,.43,hex('#ffd479'),18,[.27,.2]);
  coin.torus([0,0,.08],[0,0,1],.34,.04,hex('#fff0a4'),20,5,[.3,.3]);
  coin.box([0,0,.12],[.07,.35,.02],hex('#fff3b8'),[.25,.3]);
  const coinMesh=renderer.upload(coin);
  const flame=new Geometry();
  flame.ellipsoid([-.65,.76,-2.4],[.14,.13,.6],hex('#89f9ff'),12,7,[.15,3]);
  flame.ellipsoid([.65,.76,-2.4],[.14,.13,.6],hex('#89f9ff'),12,7,[.15,3]);
  const flameMesh=renderer.upload(flame);
  const shield=new Geometry();
  shield.torus([0,1.1,0],[0,1,0],2.25,.04,hex('#a2f6f0'),40,6,[.2,2]);
  shield.torus([0,1.1,0],[1,.4,0],2.1,.025,hex('#b9fbfa'),40,5,[.2,1.5]);
  const shieldMesh=renderer.upload(shield);
  const sparks=new Geometry();
  for(const side of [-1,1])for(let i=0;i<5;i++)sparks.diamond([side*(1.26+i*.12),.27+i*.1,-1.6-i*.35],.055+i*.012,hex(i%2?'#ffb06c':'#a4ffff'),[.25,2.5]);
  const sparkMesh=renderer.upload(sparks);
  const savedClass=Number(storage.get('class'));
  let raceClass:RaceClass=savedClass===50||savedClass===150?savedClass:100;
  let paintIndex=clamp(Math.floor(Number(storage.get('paint'))||0),0,PAINTS.length-1);
  let weather=storage.get('weather')==='night'?'night':'day';
  let quality:Quality=window.innerWidth<601?'balanced':'high';
  const savedQuality=storage.get('quality');
  if(savedQuality==='high'||savedQuality==='balanced'||savedQuality==='performance')quality=savedQuality;
  let autoAccelerate=storage.get('auto')==='true';
  let reducedMotion=storage.get('calm')===null?window.matchMedia('(prefers-reduced-motion: reduce)').matches:storage.get('calm')==='true';
  let time=0,night=weather==='night'?1:0,toastUntil=0,noticeUntil=0,goUntil=0,resultAt=Infinity;
  let previous=performance.now(),accumulator=0,lastUi=0,frameId=0,frameCount=0,fps=60,lastFpsTime=0;
  let camera:Camera={position:[0,0,0],target:[0,0,0],fov:Math.PI/3,shift:0},cameraReady=false;
  let resultRecorded=false,lastItem='';
  const keys=new Set<string>(),touches=new Set<string>();
  const bestRecord=()=>{const n=Number(storage.get(`best.${raceClass}`));return n>0&&Number.isFinite(n)?n:null;};
  const toast=(text:string)=>{el('toast').textContent=text;el('toast').hidden=false;toastUntil=time+3;};
  const startAudio=()=>{void audio.start().catch(()=>toast('Audio is unavailable. You can still enjoy the race.'));};
  const setNav=(id:string)=>document.querySelectorAll('.nav-button').forEach(button=>button.classList.toggle('active',button.id===id));
  const closeDialogs=()=>document.querySelectorAll<HTMLDialogElement>('dialog[open]').forEach(d=>d.close());
  const openDialog=(id:string)=>{closeDialogs();dialog(id).showModal();if(id==='garage-dialog')setNav('nav-garage');if(id==='how-dialog')setNav('nav-how');};
  const syncClass=()=>{
    document.querySelectorAll<HTMLButtonElement>('[data-class]').forEach(button=>{const selected=Number(button.dataset.class)===raceClass;button.classList.toggle('selected',selected);button.setAttribute('aria-pressed',String(selected));});
    el('class-note').textContent=raceClass===50?'Easy does it.':raceClass===100?'A little more thrill.':'Full-send territory.';
    storage.set('class',String(raceClass));
  };
  const syncPaint=()=>{
    document.querySelectorAll<HTMLButtonElement>('[data-paint]').forEach(button=>{const selected=Number(button.dataset.paint)===paintIndex;button.classList.toggle('selected',selected);button.setAttribute('aria-pressed',String(selected));});
    el('paint-name').textContent=PAINTS[paintIndex].name;storage.set('paint',String(paintIndex));
  };
  const syncWeather=()=>{
    document.querySelectorAll<HTMLButtonElement>('[data-weather]').forEach(button=>{const selected=button.dataset.weather===weather;button.classList.toggle('selected',selected);button.setAttribute('aria-pressed',String(selected));});
    storage.set('weather',weather);document.body.dataset.weather=weather;
  };
  const syncAudio=()=>{
    const button=el<HTMLButtonElement>('audio-toggle');button.innerHTML=icon(audio.enabled?'sound':'muted');button.setAttribute('aria-label',audio.enabled?'Mute audio':'Enable audio');button.title=audio.enabled?'Mute audio':'Enable audio';
    storage.set('audio',String(audio.enabled));
  };
  function applyQuality(value:Quality){quality=value;renderer.setQuality(value);el<HTMLSelectElement>('quality-select').value=value;el('graphics-badge').textContent=value==='high'?'HYBRID RAY TRACING':value==='balanced'?'REAL-TIME GRAPHICS':'PERFORMANCE MODE';storage.set('quality',value);}
  function startRace(){
    closeDialogs();keys.clear();touches.clear();race.start(raceClass,PAINTS[paintIndex].hex);race.bestLap=bestRecord();
    document.body.classList.add('racing');el('race-hud').hidden=false;cameraReady=false;resultRecorded=false;resultAt=Infinity;lastItem='';
    el('race-notice').hidden=true;noticeUntil=0;goUntil=0;accumulator=0;startAudio();updateHud();
  }
  function pauseRace(){
    if(race.phase!=='racing'&&race.phase!=='countdown')return;
    race.pause();keys.clear();touches.clear();document.querySelectorAll('.mobile-controls .pressed').forEach(b=>b.classList.remove('pressed'));openDialog('pause-dialog');
  }
  function resumeRace(){dialog('pause-dialog').close();race.resume();keys.clear();previous=performance.now();accumulator=0;}
  function backToMenu(){
    closeDialogs();race.menu();keys.clear();touches.clear();document.body.classList.remove('racing');el('race-hud').hidden=true;el('countdown').hidden=true;el('race-notice').hidden=true;el('boost-overlay').classList.remove('active');
    cameraReady=false;resultAt=Infinity;noticeUntil=0;goUntil=0;setNav('nav-race');
  }
  function eventFeedback(event:RaceEvent){
    audio.play(event);
    if(event.kind==='go')goUntil=time+.8;
    if(event.text&&!['countdown','go','coin','finish'].includes(event.kind)){
      el('race-notice').textContent=event.text;el('race-notice').hidden=false;noticeUntil=time+(event.kind==='lap'?2.6:1.3);
    }
    if(event.kind==='finish'&&!resultRecorded){
      resultRecorded=true;resultAt=time+.8;
      const best=Math.min(...race.lapTimes),record=bestRecord();
      if(Number.isFinite(best)&&(record===null||best<record))storage.set(`best.${raceClass}`,String(best));
      const count=Number(storage.get('races'))||0;storage.set('races',String(Math.min(999999,Math.max(0,Math.floor(count))+1)));
    }
  }
  const ordinal=(n:number)=>`${n}${n===1?'st':n===2?'nd':n===3?'rd':'th'}`;
  function updateResults(){
    el('results-title').textContent=race.position===1?'First class. First place.':race.position<=3?'Hello, podium.':'What a joyride.';
    el('results-class').textContent=`${raceClass}cc · ${ordinal(race.position)} place`;
    el('result-time').textContent=formatTime(race.player.finishTime??race.elapsed);
    el('result-best').textContent=formatTime(Math.min(...race.lapTimes));
    el('results-list').innerHTML=race.standings.map((r,i)=>`<li class="${r.id===0?'you':''}"><span class="result-rank">${String(i+1).padStart(2,'0')}</span><i class="result-dot" style="background:${r.color}"></i><strong>${r.name}${r.id===0?' / Comet':''}</strong><span>${r.finishTime===null?'Still racing':formatTime(r.finishTime)}</span></li>`).join('');
  }
  function updateHud(){
    const p=race.player,pos=race.position;
    el('hud-position').innerHTML=`${pos}<span>${ordinal(pos).replace(String(pos),'')}</span>`;
    el('hud-lap').innerHTML=`${race.lap} <span>/ ${race.laps}</span>`;
    document.querySelectorAll('.lap-dots>i').forEach((dot,i)=>dot.classList.toggle('active',i<race.lap));
    el('hud-time').textContent=formatTime(p.finishTime??race.elapsed);el('hud-speed').textContent=String(Math.round(p.speed*3.6));
    el('hud-coins').textContent=String(p.coins).padStart(2,'0');el('energy-fill').style.width=`${p.energy}%`;
    el('hud-best').textContent=race.bestLap?`BEST LAP ${formatTime(race.bestLap)}`:'MAKE IT A GOOD ONE.';
    el('drift-meter').classList.toggle('active',p.drifting);el('drift-fill').style.width=`${p.driftCharge*100}%`;
    el('boost-overlay').classList.toggle('active',p.boost>0&&!reducedMotion&&race.phase==='racing');
    const item=p.item||'empty';
    if(item!==lastItem){
      lastItem=item;el('item-slot').innerHTML=`${icon(item==='shield'?'shield':item==='pulse'?'pulse':'bolt')}<kbd>E</kbd>`;
      el('item-slot').classList.toggle('has-item',!!p.item);el('item-name').textContent=item==='turbo'?'Super turbo':item==='shield'?'Energy shield':item==='pulse'?'Sonic pulse':'Find an item';
    }
    el('map-racers').innerHTML=[...race.racers].reverse().map(r=>{const pos=track.at(21+r.distance).position,x=18+(pos[0]+120)/240*144,y=18+(pos[2]+120)/240*114;return `<circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${r.id===0?5:3.2}" fill="${r.color}" stroke="${r.id===0?'#fff':'#20384b'}" stroke-width="${r.id===0?1.8:1}"/>`;}).join('');
    if(race.phase==='countdown'){
      el('countdown').hidden=false;el('countdown').classList.remove('go');el('countdown-number').textContent=String(clamp(Math.ceil(race.countdown),1,3));
      el('countdown-hint').textContent=autoAccelerate?'Auto-accelerate is on. Find your line.':window.matchMedia('(pointer:coarse)').matches?'Hold the orange pedal to accelerate':'Hold W or ↑ to accelerate';
    }else if(time<goUntil){el('countdown').hidden=false;el('countdown').classList.add('go');el('countdown-number').textContent='GO!';el('countdown-hint').textContent='See you on the other side.';}else el('countdown').hidden=true;
    if(dialog('results-dialog').open)updateResults();
  }
  document.querySelectorAll<HTMLButtonElement>('[data-class]').forEach(button=>button.addEventListener('click',()=>{raceClass=Number(button.dataset.class) as RaceClass;syncClass();}));
  document.querySelectorAll<HTMLButtonElement>('[data-paint]').forEach(button=>button.addEventListener('click',()=>{paintIndex=Number(button.dataset.paint);syncPaint();}));
  document.querySelectorAll<HTMLButtonElement>('[data-weather]').forEach(button=>button.addEventListener('click',()=>{weather=button.dataset.weather!;syncWeather();}));
  el('start-race').addEventListener('click',startRace);el('race-again').addEventListener('click',startRace);el('restart-race').addEventListener('click',startRace);
  el('pause-button').addEventListener('click',pauseRace);el('resume-race').addEventListener('click',resumeRace);el('quit-race').addEventListener('click',backToMenu);el('results-home').addEventListener('click',backToMenu);
  el('nav-race').addEventListener('click',()=>{closeDialogs();setNav('nav-race');});
  el('nav-garage').addEventListener('click',()=>openDialog('garage-dialog'));el('nav-how').addEventListener('click',()=>openDialog('how-dialog'));
  el('garage-done').addEventListener('click',()=>{dialog('garage-dialog').close();toast(`${PAINTS[paintIndex].name}. Looking good.`);});
  el('settings-button').addEventListener('click',()=>openDialog('settings-dialog'));el('graphics-info').addEventListener('click',()=>openDialog('settings-dialog'));
  el('track-info-button').addEventListener('click',()=>openDialog('track-dialog'));
  el('profile-button').addEventListener('click',()=>{el('record-best').textContent=bestRecord()?formatTime(bestRecord()!):'Your first lap awaits';el('record-races').textContent=String(Math.max(0,Math.floor(Number(storage.get('races'))||0)));el('record-class').textContent=`${raceClass}cc`;openDialog('records-dialog');});
  document.querySelector('.brand')!.addEventListener('click',event=>{event.preventDefault();closeDialogs();setNav('nav-race');});
  document.querySelectorAll<HTMLButtonElement>('.dialog-close,.dialog-dismiss').forEach(button=>button.addEventListener('click',()=>button.closest('dialog')!.close()));
  document.querySelectorAll<HTMLDialogElement>('dialog').forEach(d=>{
    d.addEventListener('close',()=>{if(!document.querySelector('dialog[open]'))setNav('nav-race');});
    d.addEventListener('click',event=>{if(event.target===d&&d.id!=='pause-dialog'&&d.id!=='results-dialog'){const r=d.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)d.close();}});
  });
  dialog('pause-dialog').addEventListener('cancel',event=>{event.preventDefault();resumeRace();});
  dialog('results-dialog').addEventListener('cancel',event=>{event.preventDefault();backToMenu();});
  el('audio-toggle').addEventListener('click',()=>{audio.setEnabled(!audio.enabled);syncAudio();if(audio.enabled)startAudio();});
  el<HTMLSelectElement>('quality-select').addEventListener('change',event=>applyQuality((event.target as HTMLSelectElement).value as Quality));
  el<HTMLInputElement>('volume-range').addEventListener('input',event=>{const value=Number((event.target as HTMLInputElement).value)/100;audio.setVolume(value);storage.set('volume',String(value));});
  el<HTMLInputElement>('auto-accelerate').addEventListener('change',event=>{autoAccelerate=(event.target as HTMLInputElement).checked;storage.set('auto',String(autoAccelerate));});
  el<HTMLInputElement>('reduced-motion').addEventListener('change',event=>{reducedMotion=(event.target as HTMLInputElement).checked;storage.set('calm',String(reducedMotion));document.body.classList.toggle('calm-camera',reducedMotion);});
  el('fullscreen-button').addEventListener('click',()=>{
    if(!document.fullscreenEnabled){toast('Fullscreen is unavailable here. Try opening the game in its own browser tab.');return;}
    const request=document.fullscreenElement?document.exitFullscreen():document.documentElement.requestFullscreen();void request.catch(()=>toast('Your browser could not enter fullscreen.'));
  });
  window.addEventListener('keydown',event=>{
    const target=event.target as HTMLElement;
    if(['INPUT','SELECT','TEXTAREA'].includes(target.tagName)||document.querySelector('dialog[open]'))return;
    if(race.phase==='menu'&&event.code==='Enter'&&!target.closest('button,a')){event.preventDefault();startRace();return;}
    if(race.phase!=='racing'&&race.phase!=='countdown')return;
    if(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Space','ShiftLeft','ShiftRight','Escape','KeyW','KeyA','KeyS','KeyD','KeyE'].includes(event.code))event.preventDefault();
    keys.add(event.code);
    if(!event.repeat){if(event.code==='Escape')pauseRace();if(event.code==='Space')race.useBoost();if(event.code==='KeyE')race.useItem();}
  });
  window.addEventListener('keyup',event=>keys.delete(event.code));
  window.addEventListener('blur',()=>{keys.clear();touches.clear();pauseRace();});
  document.addEventListener('visibilitychange',()=>{if(document.hidden)pauseRace();previous=performance.now();accumulator=0;});
  document.querySelectorAll<HTMLButtonElement>('[data-control]').forEach(button=>{
    const control=button.dataset.control!;
    button.addEventListener('pointerdown',event=>{event.preventDefault();button.setPointerCapture(event.pointerId);touches.add(control);button.classList.add('pressed');});
    const release=()=>{touches.delete(control);button.classList.remove('pressed');};
    button.addEventListener('pointerup',release);button.addEventListener('pointercancel',release);button.addEventListener('lostpointercapture',release);
  });
  el('touch-boost').addEventListener('pointerdown',event=>{event.preventDefault();race.useBoost();});el('touch-item').addEventListener('pointerdown',event=>{event.preventDefault();race.useItem();});
  el('item-slot').addEventListener('click',()=>race.useItem());
  const input=():Input=>({throttle:autoAccelerate||keys.has('KeyW')||keys.has('ArrowUp')||touches.has('throttle'),brake:keys.has('KeyS')||keys.has('ArrowDown')||touches.has('brake'),steer:Number(keys.has('KeyD')||keys.has('ArrowRight')||touches.has('right'))-Number(keys.has('KeyA')||keys.has('ArrowLeft')||touches.has('left')),drift:keys.has('ShiftLeft')||keys.has('ShiftRight')||touches.has('drift')});
  applyQuality(quality);syncClass();syncPaint();syncWeather();
  audio.setEnabled(storage.get('audio')!=='false');const savedVolume=storage.get('volume');audio.setVolume(savedVolume===null?.45:clamp(Number(savedVolume)||0,0,1));syncAudio();
  el<HTMLInputElement>('volume-range').value=String(audio.volume*100);el<HTMLInputElement>('auto-accelerate').checked=autoAccelerate;el<HTMLInputElement>('reduced-motion').checked=reducedMotion;document.body.classList.toggle('calm-camera',reducedMotion);
  function scene(dt:number){
    const objects:RenderObject[]=[];
    night=damp(night,weather==='night'?1:0,2,dt);
    if(race.phase==='menu'){
      const hero=track.at(82),heroPosition=add(hero.position,[0,.15,0]),garage=dialog('garage-dialog').open;
      const mobile=window.innerWidth<601;
      const yaw=Math.atan2(hero.tangent[0],hero.tangent[2])+(garage?time*.23:-.36);
      objects.push({mesh:playerMeshes[paintIndex],matrix:transform(heroPosition,yaw,mobile?1.8:2.8)});
      for(let i=0;i<7;i++){
        const distance=190+i*119+time*(11+i),s=track.at(distance);
        if(Math.hypot(s.position[0]-hero.position[0],s.position[2]-hero.position[2])>18)objects.push({mesh:rivalMeshes[i],matrix:transform(track.position(distance,(i%2?1:-1)*2,.13),Math.atan2(s.tangent[0],s.tangent[2]))});
      }
      const orbit=reducedMotion?0:Math.sin(time*.055)*.8;
      const desired:Camera=mobile?{position:add(heroPosition,[17+orbit,10,34]),target:add(heroPosition,[0,-5,-1]),fov:59*Math.PI/180,shift:-.12}:{position:add(heroPosition,[6+orbit,garage?9:11,29]),target:add(heroPosition,[-2,3,-34]),fov:60*Math.PI/180,shift:garage?-.58:-.43};
      if(!cameraReady){camera=desired;cameraReady=true;}else{camera.position=mix3(camera.position,desired.position,1-Math.exp(-dt*3));camera.target=mix3(camera.target,desired.target,1-Math.exp(-dt*3));camera.fov=damp(camera.fov,desired.fov,3,dt);camera.shift=damp(camera.shift,desired.shift,3,dt);}
    }else{
      for(const racer of race.racers){
        const gap=racer.distance-race.player.distance;
        if(racer.id!==0&&gap<-2.4&&gap>-(window.innerWidth<601?18:15))continue;
        const sample=track.at(21+racer.distance),pos=track.position(21+racer.distance,racer.lateral,.14);
        const yaw=Math.atan2(sample.tangent[0],sample.tangent[2])+racer.yaw,pitch=-Math.asin(clamp(sample.tangent[1],-1,1));
        const matrix=transform(pos,yaw,1,-racer.yaw*.1,pitch);
        objects.push({mesh:racer.id===0?playerMeshes[paintIndex]:rivalMeshes[racer.id-1],matrix});
        if(racer.boost>0)objects.push({mesh:flameMesh,matrix:multiply(matrix,transform([0,0,0],0,1+Math.sin(time*34)*.06))});
        if(racer.shield>0)objects.push({mesh:shieldMesh,matrix:multiply(matrix,transform([0,0,0],time*.7))});
        if(racer.drifting)objects.push({mesh:sparkMesh,matrix:multiply(matrix,transform([0,Math.sin(time*38)*.08,0]))});
      }
      const player=race.player,sample=track.at(21+player.distance),pos=track.position(21+player.distance,player.lateral,.1),compact=window.innerWidth<601;
      const desiredPos=add(add(pos,scale(sample.tangent,compact?-14.5:-10.8)),[0,compact?7:5.6,0]),desiredTarget=add(add(pos,scale(sample.tangent,13)),[0,1.4,0]);
      const fov=((compact?69:62)+(player.boost>0&&!reducedMotion?5:0))*Math.PI/180;
      const motion=scale(sample.tangent,race.phase==='racing'?player.speed*dt:0);
      if(!cameraReady){camera={position:desiredPos,target:desiredTarget,fov,shift:0};cameraReady=true;}
      else{camera.position=mix3(add(camera.position,motion),desiredPos,1-Math.exp(-dt*(reducedMotion?8:6)));camera.target=mix3(add(camera.target,motion),desiredTarget,1-Math.exp(-dt*8));camera.fov=damp(camera.fov,fov,3,dt);camera.shift=damp(camera.shift,0,8,dt);}
    }
    for(const zone of track.pickupZones){
      for(const lane of [-4,0,4])objects.push({mesh:crystalMesh,matrix:transform(track.position(zone,lane,2.1+Math.sin(time*2+lane)*.18),time*.85,1,Math.sin(time+lane)*.07)});
      for(let j=0;j<4;j++)objects.push({mesh:coinMesh,matrix:transform(track.position(zone+14+j*4,0,1.9+Math.sin(time*2+j)*.12),time*1.7+j*.3)});
    }
    renderer.render(worldMesh,objects,camera,time,night,!reducedMotion&&race.phase==='racing'&&race.player.boost>0?1:0);
  }
  function frame(now:number){
    const dt=clamp((now-previous)/1000,0,.1);previous=now;time+=dt;
    if(!document.hidden){
      accumulator=Math.min(accumulator+dt,.15);
      const controls=input();
      while(accumulator>=1/60){race.step(1/60,controls);accumulator-=1/60;}
      for(const event of race.drainEvents())eventFeedback(event);
      scene(dt);
      audio.update(race.player.speed,race.player.drifting,race.player.boost>0,race.phase==='racing');
      if(time-lastUi>.05){if(race.phase!=='menu')updateHud();lastUi=time;}
      if(noticeUntil&&time>noticeUntil){el('race-notice').hidden=true;noticeUntil=0;}
      if(toastUntil&&time>toastUntil){el('toast').hidden=true;toastUntil=0;}
      if(race.phase==='finished'&&time>=resultAt&&!dialog('results-dialog').open){resultAt=Infinity;updateResults();openDialog('results-dialog');}
      frameCount++;
      if(time-lastFpsTime>=1){fps=Math.round(frameCount/(time-lastFpsTime));frameCount=0;lastFpsTime=time;}
    }
    frameId=requestAnimationFrame(safeFrame);
  }
  function safeFrame(now:number){try{frame(now);}catch(error){showError(error);audio.update(0,false,false,false);}}
  Object.defineProperty(window,'__OVERDRIVE__',{configurable:true,value:Object.freeze({snapshot:()=>({...race.snapshot(),graphics:renderer.diagnostics,fps,paint:PAINTS[paintIndex].name,weather,autoAccelerate,reducedMotion,camera})})});
  el<HTMLCanvasElement>('game-canvas').addEventListener('webglcontextlost',event=>{event.preventDefault();race.pause();showError(new Error('Your graphics connection was interrupted. Close other graphics-heavy tabs and reload to get back on track.'));});
  window.addEventListener('pagehide',()=>{cancelAnimationFrame(frameId);renderer.dispose();audio.dispose();},{once:true});
  scene(1/60);el('loading').classList.add('loaded');window.setTimeout(()=>{el('loading').hidden=true;},400);
  frameId=requestAnimationFrame(safeFrame);
}
try{boot();}catch(error){showError(error);}
