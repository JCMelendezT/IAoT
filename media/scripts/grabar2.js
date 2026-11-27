const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const [,, url, salida, desde, hasta, paso, w, h, transp] = process.argv;
  const b = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const p = await b.newPage({ viewport: { width: +w, height: +h } });
  p.on('pageerror', e => console.log('PAGEERR', e.message));
  p.on('console', m => { if (m.type() === 'error') console.log('ERR', m.text()); });
  await p.goto(url);
  await p.waitForFunction(() => window.listo && window.render, null, { timeout: 60000 });
  fs.mkdirSync(salida, { recursive: true });
  let i = 0;
  for (let t = +desde; t < +hasta - 1e-6; t += +paso) {
    await p.evaluate(tt => window.render(tt), t);
    if (transp) await p.screenshot({ path: `${salida}/f${String(i++).padStart(4, '0')}.png`, omitBackground: true });
    else await p.screenshot({ path: `${salida}/f${String(i++).padStart(4, '0')}.jpg`, type: 'jpeg', quality: 92 });
  }
  await b.close();
})();
