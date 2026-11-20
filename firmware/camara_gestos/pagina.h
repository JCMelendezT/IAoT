// Pagina de la camara (HTML, CSS y JS en un solo texto). Se edita aqui.
#pragma once
#include <pgmspace.h>

static const char PAGINA[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Camara de gestos</title>
<style>
:root{--f:#f4f5f7;--t:#1c2330;--m:#5b6678;--c:#fff;--l:#d9dde4;--a:#2358d6;--ok:#14803c;--mal:#b3261e}
@media(prefers-color-scheme:dark){:root{--f:#12161d;--t:#e8ebf0;--m:#98a2b3;--c:#1b212b;--l:#2c3442;--a:#6d9bff;--ok:#4ade80;--mal:#ff8a80}}
*{box-sizing:border-box}body{margin:0;background:var(--f);color:var(--t);font:15px/1.45 system-ui,sans-serif}
main{max-width:760px;margin:0 auto;padding:16px}h1{font-size:20px;margin:0 0 4px}h2{font-size:16px;margin:0 0 10px}
section{background:var(--c);border:1px solid var(--l);border-radius:10px;padding:14px;margin:12px 0}
p.s{color:var(--m);margin:2px 0 10px}.fila{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:6px 0}
input,select,button{font:inherit;padding:7px 10px;border-radius:7px;border:1px solid var(--l);background:var(--c);color:var(--t)}
button{background:var(--a);border-color:var(--a);color:#fff;cursor:pointer}button.g{background:var(--c);color:var(--t);border-color:var(--l)}
button:disabled{opacity:.5}input[type=file]{max-width:100%}
canvas{width:320px;max-width:100%;image-rendering:pixelated;border:1px solid var(--l);border-radius:6px;background:#000}
.b{height:14px;background:var(--l);border-radius:7px;overflow:hidden;flex:1;min-width:120px}.b i{display:block;height:100%;background:var(--a);width:0}
.e{display:grid;grid-template-columns:90px 1fr 48px;gap:8px;align-items:center;margin:4px 0}
.ok{color:var(--ok)}.mal{color:var(--mal)}.m{color:var(--m)}.alerta{background:#fff3cd;color:#5c4400;border-radius:8px;padding:10px;margin:12px 0}
code{background:var(--f);padding:1px 5px;border-radius:4px}
</style></head><body><main>
<h1>Camara de gestos</h1>
<p class="s">Mano abierta = probar sonido &nbsp;·&nbsp; Puno = armar o desarmar &nbsp;·&nbsp; Nada = no hace nada</p>
<div id="seguro" class="alerta" hidden><b>Modo seguro.</b> La camara fallo varias veces al arrancar, asi que solo estan activos el WiFi y la actualizacion. Sube un programa nuevo abajo o pulsa <i>Reiniciar</i>.</div>

<section><h2>Lo que ve ahora</h2>
<div class="fila"><canvas id="v" width="80" height="60"></canvas>
<div style="flex:1;min-width:200px"><div id="gesto" style="font-size:20px;font-weight:600">-</div>
<div class="m" id="vel"></div><div class="m" id="acc" style="margin-top:6px"></div></div></div>
<div class="e"><span>Nada</span><div class="b"><i id="p0"></i></div><span id="n0">0</span></div>
<div class="e"><span>Mano abierta</span><div class="b"><i id="p1"></i></div><span id="n1">0</span></div>
<div class="e"><span>Puno</span><div class="b"><i id="p2"></i></div><span id="n2">0</span></div>
<p class="m" id="est"></p></section>

<section><h2>1. Sacar fotos para entrenar</h2>
<p class="s">Cada tanda se baja como un .zip. Haz varias tandas por gesto, cambiando fondo, luz y distancia (la sesion sube sola).</p>
<div class="fila"><select id="cl"><option value="abierta">Mano abierta</option><option value="puno">Puno</option><option value="nada">Nada (sin gesto)</option></select>
<label>Sesion <input id="se" type="number" min="1" max="99" value="1" style="width:70px"></label>
<label>Fotos <input id="nf" type="number" min="10" max="400" value="100" style="width:80px"></label>
<button id="go">Empezar</button></div>
<div class="fila"><div class="b"><i id="pb"></i></div><span id="pt" class="m"></span></div>
<p class="m">Cuenta atras de 3 s antes de empezar. Mueve la mano un poco durante la tanda. En "Nada" deja la camara con lo que ve normalmente (personas, escritorio, manos quietas en otra pose).</p></section>

<section><h2>2. Subir el modelo entrenado</h2>
<p class="s">Archivo <code>modelo_gestos.bin</code> que crea <code>entrenar_gestos.py</code>. No hay que recompilar.</p>
<div class="fila"><input type="file" id="fm" accept=".bin"><button id="sm">Subir modelo</button></div><p id="rm"></p></section>

<section><h2>WiFi y devkit</h2>
<p class="m" id="redes"></p>
<div class="fila"><input id="wn" placeholder="Nombre del WiFi"><input id="wc" placeholder="Clave" type="password"><button id="sw">Agregar WiFi</button></div>
<div class="fila"><input id="dh" placeholder="detector.local o IP del devkit"><button id="sd">Guardar devkit</button><button class="g" id="pd">Probar conexion</button></div><p id="rw"></p></section>

<section><h2>Cambiar el programa por WiFi</h2>
<p class="s">Sin cable. Arduino IDE: <i>Programa, Exportar Binario compilado</i> y sube el <code>.ino.bin</code> (el que NO dice bootloader, partitions ni merged).</p>
<div class="fila"><input type="file" id="fo" accept=".bin"><input id="oc" type="password" placeholder="Clave de actualizacion"><button id="so">Actualizar</button></div>
<div class="fila"><div class="b"><i id="ob"></i></div></div><p id="ro"></p>
<div class="fila"><button class="g" id="rs">Reiniciar</button></div></section>
</main>
<script>
const $=i=>document.getElementById(i),N=['Nada','Mano abierta','Puno'];let capturando=false,ult=-1;
const f=(u,o)=>fetch(u,o).then(async r=>{const t=await r.text();if(!r.ok)throw new Error(t||r.status);return t});
function msg(id,t,ok){const e=$(id);e.textContent=t;e.className=ok?'ok':'mal'}
async function estado(){try{const e=JSON.parse(await f('/api/estado'));
$('seguro').hidden=e.modo!=='seguro';
for(let i=0;i<3;i++){$('p'+i).style.width=(e.prob[i]*100)+'%';$('n'+i).textContent=Math.round(e.prob[i]*100)+'%'}
$('gesto').textContent=!e.camara?'La camara no inicio':!e.modelo?'Sin modelo (sube uno)':(e.gesto?'Gesto: '+N[e.gesto]:'Nada');
$('vel').textContent=e.modelo?e.fps+' fotos/s · red: '+e.ms+' ms por foto':'';
$('acc').textContent=e.accion!=='ninguna'?'Ultima accion hace '+e.haceSeg+' s: '+e.accion+' → '+e.resultado:'Aun no ha hecho ninguna accion';
$('est').textContent='IP '+e.ip+' · '+e.wifi+(e.rssi?' ('+e.rssi+' dBm)':'')+' · devkit '+e.devkit+(e.devkitIp?' ('+e.devkitIp+')':'')+(e.modelo?' · modelo '+e.modeloSuma:' · '+(e.modeloError||''))+' · memoria libre '+Math.round(e.memoria/1024)+' KB, PSRAM '+Math.round(e.psram/1024)+' KB';
$('redes').textContent='WiFi conocidos: '+(e.redes.length?e.redes.join(', '):'ninguno')+(e.modo==='ap'?'  (ahora estoy en mi red propia)':'');
if(!$('dh').value)$('dh').value=e.devkit}catch(x){$('est').textContent='Sin conexion con la camara...'}}
async function foto(){const r=await fetch('/api/entrada.pgm',{cache:'no-store'});if(!r.ok)throw 0;
const b=new Uint8Array(await r.arrayBuffer()),seq=+r.headers.get('X-Seq'),d=b.subarray(b.length-4800);return{seq,d}}
function dibujar(d){const c=$('v').getContext('2d'),im=c.createImageData(80,60);for(let i=0;i<4800;i++){im.data[4*i]=im.data[4*i+1]=im.data[4*i+2]=d[i];im.data[4*i+3]=255}c.putImageData(im,0,0)}
async function vista(){if(!capturando){try{const x=await foto();dibujar(x.d)}catch(e){}}setTimeout(vista,300)}
// ---- zip sin compresion ----
const T=(()=>{const t=[];for(let n=0;n<256;n++){let c=n;for(let k=0;k<8;k++)c=c&1?0xEDB88320^(c>>>1):c>>>1;t[n]=c>>>0}return t})();
const crc=d=>{let c=~0;for(let i=0;i<d.length;i++)c=T[(c^d[i])&255]^(c>>>8);return ~c>>>0};
function zip(arch){const e=new TextEncoder(),p=[],cen=[];let off=0;
for(const a of arch){const n=e.encode(a.n),c=crc(a.d),h=new DataView(new ArrayBuffer(30));
h.setUint32(0,0x04034b50,true);h.setUint16(4,20,true);h.setUint32(14,c,true);h.setUint32(18,a.d.length,true);h.setUint32(22,a.d.length,true);h.setUint16(26,n.length,true);
p.push(h.buffer,n,a.d);const k=new DataView(new ArrayBuffer(46));
k.setUint32(0,0x02014b50,true);k.setUint16(4,20,true);k.setUint16(6,20,true);k.setUint32(16,c,true);k.setUint32(20,a.d.length,true);k.setUint32(24,a.d.length,true);k.setUint16(28,n.length,true);k.setUint32(42,off,true);
cen.push(k.buffer,n);off+=30+n.length+a.d.length}
let cs=0;for(const c of cen)cs+=c.byteLength||c.length;const z=new DataView(new ArrayBuffer(22));
z.setUint32(0,0x06054b50,true);z.setUint16(8,arch.length,true);z.setUint16(10,arch.length,true);z.setUint32(12,cs,true);z.setUint32(16,off,true);
return new Blob([...p,...cen,z.buffer],{type:'application/zip'})}
$('se').value=localStorage.getItem('sesion')||1;
$('go').onclick=async()=>{if(capturando)return;capturando=true;$('go').disabled=true;
const cl=$('cl').value,se=+$('se').value,n=+$('nf').value,pre=cl+'_s'+String(se).padStart(2,'0'),arch=[];
try{for(let s=3;s>0;s--){$('pt').textContent='Empieza en '+s+'...';await new Promise(r=>setTimeout(r,1000))}
let last=-1;while(arch.length<n){const x=await foto();if(x.seq!==last){last=x.seq;dibujar(x.d);arch.push({n:pre+'_'+String(arch.length+1).padStart(4,'0')+'.pgm',d:new Uint8Array([...new TextEncoder().encode('P5\n80 60\n255\n'),...x.d])});
$('pb').style.width=(100*arch.length/n)+'%';$('pt').textContent=arch.length+' / '+n}await new Promise(r=>setTimeout(r,60))}
const a=document.createElement('a');a.href=URL.createObjectURL(zip(arch));a.download=pre+'.zip';a.click();
$('se').value=se+1;localStorage.setItem('sesion',se+1);$('pt').textContent='Listo: se bajo '+pre+'.zip. Guardalo en detector/gestos/fotos/'}
catch(e){$('pt').textContent='Se corto la tanda ('+(e&&e.message||'sin foto')+')'}
capturando=false;$('go').disabled=false};
// ---- subidas ----
function subir(url,file,barra,res,extra){return new Promise(ok=>{if(!file){msg(res,'Elige un archivo',false);return ok()}
const x=new XMLHttpRequest(),fd=new FormData();fd.append('archivo',file,file.name);x.open('POST',url);
if(barra)x.upload.onprogress=e=>{$(barra).style.width=(100*e.loaded/e.total)+'%'};
x.onload=()=>{msg(res,x.responseText,x.status===200);ok(x.status===200)};x.onerror=()=>{msg(res,'Sin respuesta (si era una actualizacion, puede que ya se este reiniciando)',false);ok()};x.send(fd)})}
$('sm').onclick=async()=>{await subir('/api/modelo',$('fm').files[0],null,'rm');estado()};
$('so').onclick=()=>subir('/actualizar?clave='+encodeURIComponent($('oc').value),$('fo').files[0],'ob','ro');
const post=(u,b)=>f(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});
$('sw').onclick=()=>post('/api/wifi','nombre='+encodeURIComponent($('wn').value)+'&clave='+encodeURIComponent($('wc').value)).then(t=>{msg('rw',t,true);estado()}).catch(e=>msg('rw',e.message,false));
$('sd').onclick=()=>post('/api/devkit','host='+encodeURIComponent($('dh').value)).then(t=>msg('rw',t,true)).catch(e=>msg('rw',e.message,false));
$('pd').onclick=()=>post('/api/probar_devkit','').then(t=>msg('rw',t,true)).catch(e=>msg('rw',e.message,false));
$('rs').onclick=()=>post('/api/reiniciar','').then(()=>msg('ro','Reiniciando... recarga la pagina en unos segundos',true)).catch(()=>{});
estado();setInterval(estado,1000);vista();
</script></body></html>)HTML";
