// Playing view: brightness, cycling, the pattern's own controls and its colour.
const cycleChoices=[30,60,120,300,600,900,1800,3600];
let renderedPatternKey=null;

// A collection control without a label stays at its default.
function controlsFor(anim,variant){
 if(anim!==16)return defs[anim]?.params||[];
 return (collection[variant]?.[1]||[]).flatMap((label,i)=>label?[['c'+i,label,0,100,1]]:[]);
}
// WebKit has no native filled track; --fill paints it.
function paintRange(range){const min=Number(range.min),span=Number(range.max)-min;range.style.setProperty('--fill',(span?(Number(range.value)-min)/span*100:0)+'%')}
function makeSlider(parent,p,current,anim,format=String){
 const [key,label,min,max,step]=p,variant=anim===16?state.params.variant:0,id=anim+':'+(anim===16?variant:'native')+':'+key;
 const box=element('div','control');
 box.innerHTML=`<label for="r_${key}">${label}</label><output class="readout" for="r_${key}"></output><input id="r_${key}" type="range" min="${min}" max="${max}" step="${step}">`;
 parent.appendChild(box);
 const range=box.querySelector('input'),output=box.querySelector('output');
 const show=raw=>{
  let v=Math.max(min,Math.min(max,Number(raw)));v=Number((min+Math.round((v-min)/step)*step).toFixed(4));v=Math.max(min,Math.min(max,v));
  range.value=v;paintRange(range);output.textContent=format(v);return v;
 };
 range.oninput=()=>{
  const v=show(range.value);if(state.currentPattern===anim&&(anim!==16||state.params.variant===variant))state.params[key]=v;
  debounce(id,()=>sendParam(anim,variant,key,v));
 };
 show(current);
 return box;
}
// Edits name the pattern they were made for; the firmware rejects them once another plays.
function sendParam(anim,variant,name,value){return command('setParam',{paramName:name,value,expectedPattern:anim,expectedVariant:variant})}
// A palette shows as its four stops, block by block.
function blocks(stops){return stops?`linear-gradient(90deg,${stops.map((stop,i)=>`${stop} ${i*25}% ${i*25+25}%`).join(',')})`:'none'}
// The active palette shows this pattern's own stops; Original shows its authored colours.
function paletteStops(id){
 if(id===Number(state.params.palette))return [0,1,2,3].map(i=>state.params['color'+i]);
 return id===7?state.params.original||null:palettes.find(p=>p[0]===id)?.[2]||null;
}
function renderColorControls(){
 const anim=state.currentPattern,variant=state.params.variant||0,root=$('colorControls'),current=Number(state.params.palette);
 root.replaceChildren();
 const group=element('div','palettes');group.setAttribute('role','radiogroup');group.setAttribute('aria-label','Palette');
 for(const [id,name] of palettes){
  const option=element('label','palette');
  option.innerHTML=`<input type="radio" name="palette" value="${id}"><i></i><span>${name}</span>`;
  const input=option.querySelector('input');input.checked=id===current;
  option.querySelector('i').style.background=blocks(paletteStops(id));
  input.onchange=async()=>{await sendParam(anim,variant,'palette',id);await refreshState(true)};
  group.appendChild(option);
 }
 root.appendChild(group);
 makeSlider(root,['saturation','Saturation',0,200,1],state.params.saturation,anim,v=>v+' %');
 document.documentElement.style.setProperty('--glow',paletteStops(current)?.[2]||'transparent');
}
function renderParams(){
 const anim=state.currentPattern,variant=state.params.variant||0,params=$('paramControls');
 params.replaceChildren();controlsFor(anim,variant).forEach(p=>makeSlider(params,p,state.params[p[0]],anim));
 renderColorControls();
}
function setBrightness(value){state.brightness=Number(value);$('brightnessValue').textContent=value;paintRange($('brightness'));debounce('brightness',()=>command('setBrightness',{value:Number(value)}))}
function formatDuration(s){
 if(s<60)return s+' s';
 if(s<3600)return Math.floor(s/60)+' min'+(s%60?' '+s%60+' s':'');
 const minutes=Math.round(s/60);return Math.floor(minutes/60)+' h'+(minutes%60?' '+minutes%60+' min':'');
}
function renderPlayMode(){
 const cycling=Boolean(state.autoCycleEnabled),seconds=Number(state.cycleInterval)||300,select=$('cycleInterval');
 $('cycling').checked=cycling;select.hidden=!cycling;
 if(document.activeElement!==select){
  select.replaceChildren(...[...new Set([...cycleChoices,seconds])].sort((a,b)=>a-b).map(s=>{const option=element('option','','every '+formatDuration(s));option.value=s;return option}));
  select.value=String(seconds);
 }
}
function setCycling(on){return command('setAutoCycle',{value:on})}
function setCycleInterval(value){return command('setCycleInterval',{value:Number(value)})}
async function resetPattern(button){if(confirmOnButton(button,'Confirm reset',selectedGalleryKey()))await command('resetPattern')}
