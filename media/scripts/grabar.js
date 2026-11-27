const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const [,, escena, salida, desde, hasta, paso] = process.argv;
  const b = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const p = await b.newPage({ viewport: { width: 1080, height: 1920 } });
  p.on('console', m => { if (m.type() === 'error') console.log('ERR', m.text()); });
  p.on('pageerror', e => console.log('PAGEERR', e.message));
  await p.goto(`http://localhost:8765/escena.html?e=${escena}`);
  await p.waitForFunction(() => window.listo && window.render, null, { timeout: 60000 });
  fs.mkdirSync(salida, { recursive: true });
  let i = 0;
  for (let t = +desde; t < +hasta - 1e-9; t += +paso) {
    await p.evaluate(tt => window.render(tt), t);
    await p.screenshot({ path: `${salida}/f${String(i++).padStart(4, '0')}.jpg`, type: 'jpeg', quality: 92 });
  }
  await b.close();
})();
