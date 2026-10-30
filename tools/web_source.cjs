// App sources in page order, for behavioural tests without a browser.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.join(__dirname,'..'),web=path.join(root,'web');
const read=name=>fs.readFileSync(path.join(web,name),'utf8');
const readProject=name=>fs.readFileSync(path.join(root,name),'utf8');
const files=[...read('index.html').matchAll(/<script src="([\w.-]+)"><\/script>/g)].map(m=>m[1]);
const script=(names=files)=>names.map(read).join('\n');
// Top-level declarations by name, for tests that stub everything around them.
function pick(file,...names){
 const chunks=[];
 for(const line of read(file).split('\n')){
  if(/^(async function|function|const|let) /.test(line))chunks.push(line);
  else if(chunks.length)chunks[chunks.length-1]+='\n'+line;
 }
 const declares=(chunk,name)=>new RegExp(`^(async )?function ${name}\\(|^(const|let) ${name}\\b`).test(chunk);
 for(const name of names)if(!chunks.some(chunk=>declares(chunk,name)))throw new Error(`${file} declares no ${name}`);
 return chunks.filter(chunk=>names.some(name=>declares(chunk,name))).join('\n');
}
// Element stand-ins that remember what the app sets and hand out child parts by selector.
function node(){return {value:'',checked:false,disabled:false,hidden:false,textContent:'',innerHTML:'',dataset:{},children:[],valid:true,
 style:{setProperty(name,value){this[name]=value}},
 checkValidity(){return this.valid},reportValidity(){this.reported=true;return this.valid},closest(){return null},querySelectorAll(){return []},
 querySelector(selector){return this['query '+selector]||=node()},appendChild(child){this.children.push(child);return child},append(...children){this.children.push(...children)},
 replaceChildren(...children){this.children=children},setAttribute(name,value){this['attr '+name]=String(value)},getAttribute(name){return this['attr '+name]},
 addEventListener(){},contains(){return false},get childElementCount(){return this.children.length},getContext(){return {fillRect(){},drawImage(){}}}}}
// Every app script in one context on a manual clock; tests replace fetch and other globals on ctx.
function loadApp(){
 let clock=0,timerId=0;
 const elements={},scheduled=new Map(),el=id=>elements[id]||=node();
 const ctx=vm.createContext({console,AbortController,Image:class{},requestAnimationFrame(){},
  Date:{now:()=>clock},
  setTimeout(callback,delay=0){scheduled.set(++timerId,{callback,at:clock+delay});return timerId},
  clearTimeout(id){scheduled.delete(id)},
  location:{hash:''},window:{addEventListener(){},matchMedia:()=>({matches:false}),scrollTo(){}},
  document:{addEventListener(){},getElementById:el,createElement:node,querySelector:()=>null,querySelectorAll:()=>[],
   body:{dataset:{}},documentElement:{style:{setProperty(){}}},activeElement:null},
  fetch:async()=>{throw new Error('No network in tests')}});
 vm.runInContext(script(),ctx);
 // Runs every timer due by ms from now, in order, including ones they schedule.
 function advance(ms){
  clock+=ms;
  for(let due;(due=[...scheduled].filter(([,t])=>t.at<=clock).sort((a,b)=>a[1].at-b[1].at)[0]);){scheduled.delete(due[0]);due[1].callback()}
 }
 return {ctx,el,advance,run:code=>vm.runInContext(code,ctx),plain:value=>JSON.parse(JSON.stringify(value))};
}
const tick=()=>new Promise(resolve=>setImmediate(resolve));
async function settle(){for(let i=0;i<6;i++)await tick()}
module.exports={files,read,readProject,script,pick,node,loadApp,tick,settle};
