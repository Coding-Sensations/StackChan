// DOM-only simulation. VM has no require, fetch, browser, device, or process API.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
class Element {
 constructor(tag){this.tag=tag;this.children=[];this.style={};this.textContent='';this.value='';}
 append(...nodes){this.children.push(...nodes);}
 replaceChildren(...nodes){this.children=[...nodes];}
 querySelectorAll(tag){return this.children.flatMap(node=>[...(node.tag===tag?[node]:[]),...node.querySelectorAll(tag)]);}
 querySelector(tag){return this.querySelectorAll(tag)[0];}
}
const elements={interactive:new Element('div'),all:new Element('div')};
const document={createElement:tag=>new Element(tag),getElementById:id=>{assert(id in elements);return elements[id];}};
const context=vm.createContext({document});
const source=fs.readFileSync(path.resolve(__dirname,'../../tools-preview.html'),'utf8');
const scripts=[...source.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(match=>match[1]);
assert.equal(scripts.length,2);
for(const script of scripts)vm.runInContext(script,context,{timeout:1000});
const evaluate=expression=>vm.runInContext(expression,context,{timeout:1000});
assert.equal(elements.interactive.children.length,1);
assert(elements.all.children.length>=12);
evaluate("page=9;show('Edit URL')");
assert.equal(evaluate('page'),0);
assert.equal(elements.interactive.querySelector('input').maxLength,96);
evaluate("page=8;show('Demo line')");
const line=elements.interactive.querySelector('input');
assert.equal(line.maxLength,64);
const space=elements.interactive.querySelectorAll('button').find(button=>button.textContent===' ');
assert(space);space.onclick();assert.equal(line.value,'ping ');
assert.equal(evaluate('new Set(uartKeys.join("")).size'),95);
evaluate("savedCount=0;show('Saved remotes')");
assert(elements.interactive.querySelectorAll('button').filter(button=>button.textContent==='Empty slot').every(button=>button.disabled));
for(let i=0;i<10;++i)evaluate("show('Save captured')");
assert.equal(evaluate('savedCount'),8);
evaluate("show('UART demo')");
assert(elements.interactive.querySelectorAll('button').some(button=>button.textContent==='Start demo'));
elements.interactive.querySelectorAll('button').find(button=>button.textContent==='Start demo').onclick();
assert.equal(evaluate('demoRunning'),true);
assert.match(evaluate('echo'),/no GPIO IO/);
elements.interactive.querySelectorAll('button').find(button=>button.textContent==='‹ Back').onclick();
assert.equal(evaluate('demoRunning'),false);
console.log('PASS preview initial scripts/NFC page reset/UART space/editor bounds/RAM capacity/demo Back');
