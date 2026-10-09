// Mechanical integration into the existing embedded single-file dashboard.
const fs=require('fs');const file='64x64_matrix_01.ino';let s=fs.readFileSync(file,'utf8');
if(s.includes('id="calibrationPanel"'))throw Error('Already applied');
s=s.replace('<details class="fold"><summary>Utilities</summary>',`<details class="fold" id="calibrationPanel"><summary>Color calibration / comparison</summary><div class="foldBody settingsGrid">
<p class="help" id="calibrationStatus"></p>
<div class="setting"><label for="calMode">Display view</label><select id="calMode" onchange="command('setCalibrationMode',{value:Number(this.value)})"><option value="0">Normal playback</option><option value="1">Neutral gray + RGB chart</option><option value="2">Photo: RAW left / FIX right</option><option value="3" id="calDepthOption">Photo: simulated 3-bit / native 4-bit</option></select></div>
<div class="two"><div class="setting"><label for="calPhoto">Comparison photo</label><select id="calPhoto" onchange="command('setCalibrationPhoto',{value:Number(this.value)})"><option value="0">IMG_4786</option><option value="1">IMG_4704</option></select></div><div class="setting"><label for="calLevel">Test brightness / 255</label><input id="calLevel" type="number" min="32" max="128" onchange="command('setCalibrationLevel',{value:Number(this.value)})"></div></div>
<p class="help">Chart: gray ramp; white / red / green / blue blocks; RGB ramps. Judge the white block with your eyes, not auto-exposed camera images. Reduce excess channels until neutral.</p>
<div class="setting"><label for="calR">Panel red / %</label><input id="calR" type="range" min="0" max="100" onchange="applyColorCalibration()"></div>
<div class="setting"><label for="calG">Panel green / %</label><input id="calG" type="range" min="0" max="100" onchange="applyColorCalibration()"></div>
<div class="setting"><label for="calB">Panel blue / %</label><input id="calB" type="range" min="0" max="100" onchange="applyColorCalibration()"></div>
<div class="two"><div class="setting"><label for="calGamma">Photo gamma / 1.0–2.6</label><input id="calGamma" type="number" min="1" max="2.6" step="0.05" onchange="applyColorCalibration()"></div><div class="setting"><label for="calCorrect">Photo correction</label><input id="calCorrect" type="checkbox" onchange="applyColorCalibration()"></div></div>
<p class="help">RGB balance affects this device, all patterns. Gamma affects photos only; RAW/FIX always compares uncorrected with corrected. No dithering. Tests pause auto-cycle temporarily and expire after 10 minutes. Pattern defaults are untouched.</p>
<button class="btn primary wide" onclick="command('saveColorCalibration')">Save device calibration</button>
<button class="btn ghost wide" onclick="resetColorCalibration()">Neutral RGB / original photo output</button>
</div></details>
<details class="fold"><summary>Utilities</summary>`);
s=s.replace('function render(){','function render(){renderCalibration();');
s=s.replace('async function toggleDepthTest()',`function renderCalibration(){const c=state.calibration;if(!c)return;$('calibrationPanel').style.display=c.available?'':'none';$('calibrationStatus').textContent=c.bits+'-bit driver · RGB '+c.r+' / '+c.g+' / '+c.b+'% · '+(c.mode?'TEST ACTIVE':'playback');const fields={calMode:c.mode,calPhoto:c.photo,calLevel:c.level,calR:c.r,calG:c.g,calB:c.b,calGamma:c.gamma100/100};for(const [id,value] of Object.entries(fields)){const el=$(id);if(document.activeElement!==el)el.value=value;}$('calCorrect').checked=c.photoCorrection;$('calDepthOption').disabled=!state.depthComparisonAvailable;}
async function applyColorCalibration(){await command('setColorCalibration',{value:{r:Number($('calR').value),g:Number($('calG').value),b:Number($('calB').value),gamma100:Math.round(Number($('calGamma').value)*100),photoCorrection:$('calCorrect').checked}});}
async function resetColorCalibration(){await command('setColorCalibration',{value:{r:100,g:100,b:100,gamma100:180,photoCorrection:false}});await refreshState();}
async function toggleDepthTest()`);
// Selecting a pattern or Lua is an explicit exit from the calibration view.
s=s.replaceAll('      depthComparisonEnabled = false;','      depthComparisonEnabled = false;calibrationMode=0;');
fs.writeFileSync(file,s);
