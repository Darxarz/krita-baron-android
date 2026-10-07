// Reads the production sources; writes only this repository.
const fs = require('fs'), path = require('path'), crypto = require('crypto'), vm = require('vm');
const root = path.resolve(__dirname, '..');
const babel = require(path.join(root, 'build/prompt-tools/node_modules/@babel/core'));
const preset = require(path.join(root, 'build/prompt-tools/node_modules/@babel/preset-env'));
const source = process.argv[2] || 'Z:/orchestrator/user-portal/src';
const dest = path.join(root, 'native/prompt'); fs.mkdirSync(dest, {recursive: true});
const files = ['components/PromptEditor/promptSegments.js', 'components/PromptEditor/promptOrder.js', 'utils/promptLint.js'];
let code = '', names = [], provenance = [];
for (const file of files) {
  const original = fs.readFileSync(path.join(source, file), 'utf8');
  fs.writeFileSync(path.join(dest, path.basename(file)), original);
  provenance.push({file, sha256: crypto.createHash('sha256').update(original).digest('hex')});
  names.push(...Array.from(original.matchAll(/export (?:function|const) (\w+)/g), m => m[1]));
  code += original.replace(/^import .*;$/gm, '').replace(/export /g, '') + '\n';
}
const polyfills = `if (!Array.prototype.flatMap) Array.prototype.flatMap = function(f) { return [].concat.apply([],this.map(f)); };`;
code += '\nreturn {' + names.join(',') + '};';
const result = babel.transformSync(polyfills + '\nvar PromptLogic = (function(){\n' + code + '\n})();',
  {presets:[[preset,{targets:{ie:'11'},modules:false}]],comments:false,compact:false,configFile:false,babelrc:false});
fs.writeFileSync(path.join(dest,'logic.js'), result.code);
const messages = fs.readFileSync(path.join(source,'i18n/promptEditorMessages.js'),'utf8');
const context = {}; vm.createContext(context);
vm.runInContext(messages.replace('export const promptEditorMessages', 'var promptEditorMessages'),context);
fs.writeFileSync(path.join(dest,'messages.json'), JSON.stringify(context.promptEditorMessages));
fs.writeFileSync(path.join(dest,'provenance.json'), JSON.stringify({source:'Orchestrion prompt editor', importedAt:new Date().toISOString(), files:provenance},null,2));
fs.writeFileSync(path.join(root,'native/prompt.qrc'), '<RCC><qresource prefix="/baron/prompt"><file alias="logic.js">prompt/logic.js</file><file alias="messages.json">prompt/messages.json</file></qresource></RCC>');
console.log('Imported prompt logic and translations into native/prompt; production unchanged.');
