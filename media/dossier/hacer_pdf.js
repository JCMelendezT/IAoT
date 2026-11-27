const { chromium } = require('playwright');
(async () => {
  const [,, entrada, salida, titulo] = process.argv;
  const b = await chromium.launch(); const p = await b.newPage();
  await p.goto('file://' + entrada, { waitUntil: 'networkidle' });
  await p.evaluate(() => document.fonts.ready);
  await p.pdf({ path: salida, format: 'A4', printBackground: true, preferCSSPageSize: true, displayHeaderFooter: true,
    headerTemplate: '<div></div>',
    footerTemplate: `<div style="width:100%;font-size:7.5pt;color:#7a8c9c;font-family:Montserrat,sans-serif;padding:0 16mm;display:flex;justify-content:space-between"><span>${titulo}</span><span><span class="pageNumber"></span> / <span class="totalPages"></span></span></div>` });
  await b.close();
})();
