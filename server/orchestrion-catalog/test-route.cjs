const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const {createRequire} = require('node:module');
const path = require('node:path');
const site = process.env.BARON_SITE_SOURCE;
const review = process.env.BARON_CATALOG_REVIEW;
if (!site || !review) throw new Error('Provide BARON_SITE_SOURCE and BARON_CATALOG_REVIEW');
const siteRequire = createRequire(path.join(site,'routes/kritaIntegration.js'));
const express = siteRequire('express');
const {PREFIX,scopeGuard} = siteRequire('../services/kritaDeviceService');
const output={exports:{}};
const source=fs.readFileSync(path.join(review,'routes/kritaIntegration.js'),'utf8');
vm.runInThisContext('(function(require,module,exports,__dirname){'+source+'\n})')(
  name=>name==='../services/kritaCatalogDetails' ? require('./kritaCatalogDetails') : siteRequire(name),output,output.exports,path.join(site,'routes'));
test('device-scoped catalogue route authenticates and only returns visible models',async()=>{
  const app=express();app.use(express.json());app.use(scopeGuard);
  const account={id:123};let seen;
  app.use(output.exports.createKritaIntegration({ApiKey:{},authenticate:async token=>token===PREFIX+'test' ? account : null,
    billing:{},getModels:async(user,options)=>{assert.equal(user,account);seen=options;return [{name:'characters/Hero #2.safetensors',kind:'lora',tags:['fantasy'],modelDescription:'Author recommendation'}];}}));
  const server=app.listen(0,'127.0.0.1');await new Promise(resolve=>server.once('listening',resolve));
  const base=`http://127.0.0.1:${server.address().port}`;
  const query='/api/krita/model-metadata?name='+encodeURIComponent('characters/Hero #2.safetensors')+'&kind=lora';
  try {
    assert.equal((await fetch(base+query)).status,401);
    const response=await fetch(base+query,{headers:{Authorization:'Bearer '+PREFIX+'test'}});
    assert.equal(response.status,200);const data=await response.json();assert.equal(data.data.modelDescription,'Author recommendation');
    assert.deepEqual(seen,{detailsFor:{name:'characters/Hero #2.safetensors',kind:'lora'}});
    assert.equal((await fetch(base+'/api/krita/model-metadata?name=Hero.safetensors&kind=lora',{headers:{Authorization:'Bearer '+PREFIX+'test'}})).status,404);
    assert.equal((await fetch(base+'/api/krita/model-metadata?name=Hero&kind=invalid',{headers:{Authorization:'Bearer '+PREFIX+'test'}})).status,400);
    assert.equal((await fetch(base+'/api/models/loras-v2',{headers:{Authorization:'Bearer '+PREFIX+'test'}})).status,403);
  } finally {server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
});
