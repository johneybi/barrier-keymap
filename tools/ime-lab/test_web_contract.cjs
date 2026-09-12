// Offline DOM contract checks. No browser, UI, real IME or key injection.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, 'main.swift'), 'utf8');
const match = source.match(/<script>([\s\S]*?)<\/script>/);
assert.ok(match, 'embedded page script exists');
assert.ok(source.includes("default-src 'none'"), 'page denies external resources');
assert.ok(source.includes('config.websiteDataStore = .nonPersistent()'));
const records = [];
const marker = 'PRIVATE_TEST_TEXT_한글';
const elements = ['search', 'area'].map(id => ({
  id,
  value: marker,
  handlers: {},
  addEventListener(name, callback) {
    assert.equal(this.handlers[name], undefined, 'no duplicate handlers');
    this.handlers[name] = callback;
  },
}));
const context = vm.createContext({
  document: {
    querySelectorAll(selector) {
      assert.equal(selector, 'input,textarea');
      return elements;
    },
  },
  window: { webkit: { messageHandlers: { ime: {
    postMessage(value) { records.push(value); },
  } } } },
});
new vm.Script(match[1], {filename: 'embedded-ime-lab.js'}).runInContext(context, {timeout: 1000});
assert.equal(records.length, 0, 'loading page sends no synthetic input or events');
const names = ['focus', 'blur', 'compositionstart', 'compositionupdate',
               'compositionend', 'beforeinput', 'input'];
for (const el of elements) {
  assert.deepEqual(Object.keys(el.handlers), names);
  for (const name of names) {
    const isComposing = name === 'compositionstart' || name === 'compositionupdate';
    const inputType = name === 'input' || name === 'beforeinput' ? 'insertText' : undefined;
    el.handlers[name]({data: marker, isComposing, inputType});
    assert.equal(records.at(-1),
      `web ${el.id} ${name} type=${inputType ?? '-'} composing=${isComposing} totalUTF16=${marker.length}`);
  }
}
assert.equal(records.length, 14);
assert.ok(records.every(record => !record.includes(marker) && record.length < 200));
console.log('PASS: 14 DOM event cases; no content in payloads; no startup events.');
console.log('DOM collaborators are mocked; this is not a WebKit/IME runtime test.');
