// Exercise the actual embedded control handler without a browser or hardware.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '..', '64x64_matrix_01.ino'), 'utf8');
const handler = source.split('\n').find(line => line.startsWith('function makeSlider('));
const calls = [];
const listeners={};
let elements;
const document = {
  addEventListener(name,callback){listeners[name]=callback;},
  querySelectorAll(){return parent.children;},
  createElement() {
    elements = {};
    const dial = {style: {setProperty() {}}, setAttribute(name, value) {this[name] = value;},
      focus() {}, setPointerCapture() {}, hasPointerCapture() {return false;}};
    return {dataset:{},isConnected:true,classList:{toggle(){}},contains(el){return Object.values(elements).includes(el);},querySelector(selector) {
      if (selector === '.encoder') return dial;
      return elements[selector] ||= {value: '', textContent: ''};
    }};
  }
};
const context = vm.createContext({document, Number, Math, String,state:{currentPattern:0,params:{}},
  command(command, extra) {calls.push({command, ...extra});},
  debounce(key, callback) {callback();}});
const selection=source.split('\n').filter(line=>line.startsWith('let selectedControlId=')||line.startsWith('document.addEventListener("keydown"')).join('\n');
vm.runInContext(selection+'\n'+handler, context);
const parent = {id: 'paramControls', children: [], appendChild(box) {this.children.push(box);}};
context.makeSlider(parent, ['b','Brightness',0,15,1,'Brightness'], 5, 0);
const dial = parent.children[0].querySelector('.encoder');
const event = {button: 0, clientY: 100, pointerId: 1, preventDefault() {}};
dial.onpointerdown(event);
for (let y = 99; y > 30; y--) dial.onpointermove({...event, clientY: y, shiftKey: false});
assert.equal(calls.length, 0, 'Dragging must not send any parameter commands');
dial.onpointerup(event);
assert.equal(calls.length, 1);
assert.equal(calls[0].paramName, 'b');
assert.equal(calls[0].animIndex, 0);
assert.equal(calls[0].value, 11);
dial.onpointerdown(event);
dial.onpointermove({...event, clientY: 0});
dial.onpointercancel();
assert.equal(calls.length, 1, 'Cancelled drag must not commit');
assert.equal(Number(elements['#r_b'].value), 11);
dial.onkeydown({key: 'ArrowDown', preventDefault() {}});
assert.equal(calls.length, 1, 'Keyboard adjustment commits on key release');
dial.onkeyup();
assert.equal(calls.at(-1).value, 10);
elements['#r_b'].value = 2;
elements['#r_b'].oninput();
assert.equal(calls.length, 2, 'Native range input must not send while moving');
elements['#r_b'].onchange();
assert.equal(calls.at(-1).value, 2);
assert.equal(dial['aria-valuenow'], 2);
listeners.keydown({key:'ArrowRight',target:{tagName:'BODY'},preventDefault(){}});
assert.equal(calls.at(-1).value,3,'Last edited control responds after focus moves away');
listeners.keydown({key:'ArrowUp',shiftKey:true,target:{tagName:'BODY'},preventDefault(){}});
assert.equal(calls.at(-1).value,13,'Shift arrows make ten-step changes');
const count=calls.length;
listeners.keydown({key:'ArrowLeft',target:{tagName:'INPUT',type:'search'},preventDefault(){throw Error('Must not capture search caret');}});
assert.equal(calls.length,count);
context.makeSlider(parent,['b','Brightness',0,15,1,'Brightness'],13,0);
listeners.keydown({key:'ArrowDown',target:{tagName:'BODY'},preventDefault(){}});
assert.equal(calls.at(-1).value,12,'Selection survives a dashboard state rerender');
console.log('PASS: drag/release/cancel, keyboard release, persistent selection, Shift steps, search caret, rerender, and API fields.');
