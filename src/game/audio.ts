import { clamp } from '../engine/math.ts';
import type { RaceEvent } from './race.ts';

export class AudioSystem {
  enabled=true;
  volume=.45;
  private context:AudioContext|null=null;
  private master:GainNode|null=null;
  private engineGain:GainNode|null=null;
  private engine:OscillatorNode|null=null;
  private sub:OscillatorNode|null=null;
  private filter:BiquadFilterNode|null=null;
  private driftGain:GainNode|null=null;
  async start(){
    if(!this.context){
      this.context=new AudioContext();
      const ctx=this.context;
      this.master=ctx.createGain();this.master.gain.value=this.enabled?this.volume:0;this.master.connect(ctx.destination);
      this.engine=ctx.createOscillator();this.engine.type='sawtooth';this.engine.frequency.value=45;
      this.sub=ctx.createOscillator();this.sub.type='triangle';this.sub.frequency.value=44;
      this.engineGain=ctx.createGain();this.engineGain.gain.value=0;
      this.filter=ctx.createBiquadFilter();this.filter.type='lowpass';this.filter.frequency.value=460;
      this.engine.connect(this.filter);this.sub.connect(this.filter);this.filter.connect(this.engineGain);this.engineGain.connect(this.master);
      this.engine.start();this.sub.start();
      const buffer=ctx.createBuffer(1,ctx.sampleRate*2,ctx.sampleRate),data=buffer.getChannelData(0);
      for(let i=0;i<data.length;i++)data[i]=Math.random()*2-1;
      const source=ctx.createBufferSource();source.buffer=buffer;source.loop=true;
      const filter=ctx.createBiquadFilter();filter.type='bandpass';filter.frequency.value=1700;filter.Q.value=.4;
      this.driftGain=ctx.createGain();this.driftGain.gain.value=0;
      source.connect(filter);filter.connect(this.driftGain);this.driftGain.connect(this.master);source.start();
    }
    if(this.context.state==='suspended')await this.context.resume();
  }
  setEnabled(value:boolean){this.enabled=value;this.syncVolume();}
  setVolume(value:number){this.volume=clamp(value,0,1);this.syncVolume();}
  private syncVolume(){if(this.context&&this.master)this.master.gain.setTargetAtTime(this.enabled?this.volume:0,this.context.currentTime,.05);}
  update(speed:number,drifting:boolean,boosting:boolean,running:boolean){
    if(!this.context||!this.engine||!this.sub||!this.engineGain||!this.filter||!this.driftGain)return;
    const now=this.context.currentTime,frequency=42+speed*2.7+(boosting?30:0);
    this.engine.frequency.setTargetAtTime(frequency,now,.07);this.sub.frequency.setTargetAtTime(frequency*.503,now,.07);
    this.filter.frequency.setTargetAtTime(300+speed*14,now,.08);
    this.engineGain.gain.setTargetAtTime(running?.026+speed*.00035:0,now,.12);
    this.driftGain.gain.setTargetAtTime(running?(drifting?.07:boosting?.035:0):0,now,.09);
  }
  private note(frequency:number,duration:number,type:OscillatorType='sine',delay=0,volume=.1){
    if(!this.context||!this.master||!this.enabled)return;
    const ctx=this.context,o=ctx.createOscillator(),gain=ctx.createGain(),t=ctx.currentTime+delay;
    o.type=type;o.frequency.setValueAtTime(frequency,t);gain.gain.setValueAtTime(.001,t);gain.gain.exponentialRampToValueAtTime(volume,t+.01);gain.gain.exponentialRampToValueAtTime(.001,t+duration);
    o.connect(gain);gain.connect(this.master);o.start(t);o.stop(t+duration+.02);o.onended=()=>{o.disconnect();gain.disconnect();};
  }
  play(event:RaceEvent){
    if(event.kind==='countdown')this.note(440,.16,'triangle');
    if(event.kind==='go'){this.note(880,.5,'triangle');this.note(1320,.35,'sine',.1,.055);}
    if(event.kind==='coin'){this.note(1400,.09,'sine',0,.06);this.note(1800,.13,'sine',.075,.04);}
    if(event.kind==='pickup'){[660,830,990].forEach((f,i)=>this.note(f,.18,'triangle',i*.08,.085));}
    if(event.kind==='boost'||event.kind==='drift'){[180,260,390].forEach((f,i)=>this.note(f,.24,'sawtooth',i*.08,.032));}
    if(event.kind==='impact')this.note(70,.13,'triangle',0,.2);
    if(event.kind==='shield'||event.kind==='pulse'){[440,660,880,1100].forEach((f,i)=>this.note(f,.3,'sine',i*.07,.06));}
    if(event.kind==='lap'||event.kind==='finish'){[523,659,784,1046].forEach((f,i)=>this.note(f,.33,'triangle',i*.13,.08));}
  }
  dispose(){void this.context?.close();this.context=null;}
}
