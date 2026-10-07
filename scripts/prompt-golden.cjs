const fs = require('fs'), path = require('path'), vm = require('vm');
const root = path.resolve(__dirname,'..');
const scope = {}; vm.createContext(scope);
let code = ['promptSegments.js','promptOrder.js'].map(file => fs.readFileSync(path.join(root,'native/prompt',file),'utf8').replace(/^import .*;$/gm,'').replace(/export /g,'')).join('\n');
vm.runInContext(code,scope);
const prompts = ['masterpiece, (long_hair:1.2), <lora:Artists/demo:0.8>, BREAK blue eyes',
  'cat, dog\n((soft_light)), [a:b:0.5], {red|blue}, AND horse',
  'Русский текст, толстые_брови, 你好_世界, 👁️ focus, \\(brackets\\)',
  'A tall character walks. Soft light and warm colors!',
  'one, (two, three, four, bad)broken), <lora:folder/demo>, green, green',
  'background; 1boy; masterpiece; standing; forest; <lora:demo:-0.4>',
  fs.readFileSync(path.join(root,'tests/fixtures/prompt-self-copy.txt'),'utf8')];
const cases = [];
function add(name,args,options=false) {
  const actual = structuredClone(args);
  if (options) {const o=actual.at(-1); o.lookup = key=>o.dictionary?.[key] || null; }
  const expected = scope[name](...actual);
  cases.push({name,args,expected:expected===undefined?null:JSON.parse(JSON.stringify(expected))});
}
for (const text of prompts) {
  const segs = JSON.parse(JSON.stringify(scope.segmentPrompt(text)));
  add('segmentPrompt',[text]); add('lintPrompt',[text,segs]);
  for (const s of segs.slice(0,6)) {
    add('classifySegment',[s.text]); add('getSegmentWeight',[s.text]);
    add('adjustSegmentWeight',[s.text,.1]); add('setSegmentWeight',[s.text,.8]);
    add('toggleUnderscores',[s.text]); add('normalizeKey',[s.text]);
  }
  if (segs.length) {
    add('duplicateSegment',[text,segs,0]); add('deleteSegment',[text,segs,segs.length-1]);
    add('moveSegment',[text,segs,0,segs.length-1]);
    add('insertAfterSegment',[text,segs,0,'new tag']);
  }
  for (const family of ['illustrious','pony','anima','natural']) add('organizePrompt',[text,{family,dedupe:false,dictionary:{'green':{category:'artist'}}}],true);
}
fs.writeFileSync(path.join(root,'tests/fixtures/prompt-site-golden.json'),JSON.stringify(cases,null,2));
console.log(cases.length+' reference cases from the unmodified website sources');
