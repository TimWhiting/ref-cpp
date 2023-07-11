class LinearMemory {
    constructor({initial = 1000, maximum = 1000}) {
        this.memory = new WebAssembly.Memory({ initial, maximum });
    }
    read_string(offset) {
        let view = new Uint8Array(this.memory.buffer);
        let bytes = []
        for (let byte = view[offset]; byte; byte = view[++offset])
            bytes.push(byte);
        return String.fromCharCode(...bytes);
    }
    log(str)      { console.log(`wasm log: ${str}`) }
    log_i(str, i) { console.log(`wasm log: ${str}: ${i}`) }
    env() {
        return {
            memory: this.memory,
            wasm_log: (off) => this.log(this.read_string(off)),
            wasm_log_i: (off, i) => this.log_i(this.read_string(off), i)
        }
    }
}

let finalizers = new FinalizationRegistry(f => { f(); });

function invoke(callback) {
    return callback()
}
function out_of_memory() {
    console.log('error: out of linear memory');
}

let nalloc = 0;
let nfinalized = 0;
let nmax = 0;
class WasmObject {
    constructor(wasm) {        
        this.wasm = wasm
        let obj = wasm.exports.make_obj();
        this.obj = obj;
        nalloc++;
        nmax = Math.max(nalloc - nfinalized, nmax);
        let free_obj = this.wasm.exports.free_obj;
        finalizers.register(this, () => { nfinalized++; free_obj(obj); }, this);
    }
    attachCallback(f) {
        this.wasm.exports.attach_callback(this.obj, f);
    }
    invokeCallback(a, a2) {
        this.wasm.exports.invoke_callback(this.obj, a, a2);
    }
}


const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

async function allowFinalizersToRun() {
    if (typeof gc !== 'undefined')
        gc();
    await delay(1);
}
async function runTestLoop(n, f, ...args) {
    for (let i = 0; i < n; i++) {
        await f(...args);
        await allowFinalizersToRun();
    }
}

function doSomething(n){
  console.log(`Callback after ${nalloc} allocated. ${n}`)
}
async function test(n, instance) {
    for (let i = 0; i < n; i++) {
        let obj = new WasmObject(instance);
        obj.attachCallback(doSomething);
        if (i == 0) obj.invokeCallback(i*2, 100);
    }
    await delay(1);
    console.log(`${nalloc} total allocated, ${nalloc - nfinalized} still live.`);
}
async function main() {
    let resp = await fetch("test.wasm");
    let wasm = await resp.arrayBuffer()
    let mod = new WebAssembly.Module(wasm);
    let memory = new LinearMemory({ initial: 2, maximum: 100 });
    let rt = { invoke, out_of_memory };
    let imports = { env: memory.env(), rt }
    let instance = new WebAssembly.Instance(mod, imports);
    await runTestLoop(10, test, 10, instance);
    console.log(`Success; max ${nmax} objects live.`);
}
main()
