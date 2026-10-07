const test = require('node:test');
const assert = require('node:assert/strict');
const { catalogDetails, visibleModel } = require('./kritaCatalogDetails');
test('metadata uses explicit fields and preserves author recommendations', () => {
  const result = catalogDetails({tags: ['fantasy','fantasy'], creator: {username:'artist'}, description:'Use strength 0.7',
    recommendedSettings: {steps:28, nested:{secret:true}}, images:[{url:'ignored',meta:{steps:28,prompt:'ignored'}}]}, {}, true);
  assert.deepEqual(result.tags,['fantasy']); assert.equal(result.creator,'artist');
  assert.equal(result.modelDescription,'Use strength 0.7'); assert.deepEqual(result.recommendedSettings,{steps:28});
  assert.deepEqual(result.images,[{meta:{steps:28}}]);
});
test('list response omits large descriptions', () => {
  assert.deepEqual(catalogDetails(null,{tagsJson:'["style"]',creator:'artist',description:'long notes'}),
    {tags:['style'],creator:'artist',versionName:'',modelUrl:''});
});
test('metadata only resolves exact visible paths and kinds', () => {
  const models=[{name:'a/Hero.safetensors',kind:'lora'},{name:'b/Hero.safetensors',kind:'checkpoint'}];
  assert.equal(visibleModel(models,'Hero.safetensors','lora'),null);
  assert.equal(visibleModel(models,'a\\Hero.safetensors','lora'),models[0]);
  assert.equal(visibleModel(models,'a/Hero.safetensors','checkpoint'),null);
  assert.equal(visibleModel([...models,models[0]],'a/Hero.safetensors','lora'),null);
});
