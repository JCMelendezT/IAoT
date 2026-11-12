// Entrenamiento del detector dentro del celular. Hace lo mismo que ml/entrenar.py:
// normaliza los rasgos, entrena una regresion logistica de 3 clases (fondo, bluetooth, dron)
// con pesos balanceados por clase y regularizacion L2 (C = 1, igual que scikit-learn) y la
// evalua separando bloques de 1 minuto. Funciona en el navegador y en Node (para probarlo).
"use strict";

const Entrenamiento = (() => {
  const CLASES = ["fondo", "bluetooth", "dron"];
  // Mismo orden y nombres que RD_NOMBRES (rasgos_detector.h) y ml/rasgos.py
  const NOMBRES = [
    "tramas", "aparatos", "mgmt", "datos", "tam_prom", "rssi_prom", "rssi_max",
    "tramas_debiles", "aparatos_fuertes",
    "banda_2400", "banda_2406", "banda_2412", "banda_2418", "banda_2424", "banda_2430",
    "banda_2436", "banda_2442", "banda_2448", "banda_2454", "banda_2460", "banda_2466",
    "banda_2472", "banda_2478", "banda_2484", "banda_2490", "banda_2496", "banda_2502",
    "banda_2508", "banda_2514", "banda_2520",
    "esp_media", "esp_banda_dron", "esp_max_banda_dron", "esp_canales_activos", "esp_variacion",
  ];
  const WIFI = NOMBRES.slice(0, 9);

  // ---------------------------------------------------------------- CSV
  function leerCSV(texto) {
    const lineas = texto.replace(/\r/g, "").split("\n").filter((l) => l.length);
    const cab = lineas[0].split(",");
    return lineas.slice(1).map((l) => {
      const v = l.split(",");
      const o = {};
      cab.forEach((c, i) => { o[c] = v[i]; });
      return o;
    });
  }

  // Filas de ventanas.csv (grabaciones del PC) -> ejemplos
  function ejemplosBase(filas) {
    return filas.filter((f) => CLASES.includes(f.clase)).map((f) => ({
      x: Object.fromEntries(NOMBRES.map((n) => [n, Number(f[n])])),
      y: CLASES.indexOf(f.clase),
      grupo: "pc:" + f.sesion + ":" + f.minuto,
      orden: f.sesion + ":" + String(Math.round(Number(f.t) * 1000)).padStart(12, "0"),
      origen: "base",
    }));
  }

  // Filas de /api/datos del devkit (solo las etiquetadas) -> ejemplos
  function ejemplosDispositivo(filas) {
    return filas.filter((f) => Number(f.etiqueta) >= 1 && Number(f.etiqueta) <= 3).map((f) => ({
      x: Object.fromEntries(NOMBRES.map((n) => [n, Number(f[n])])),
      y: Number(f.etiqueta) - 1,
      grupo: "dev:" + Math.floor(Number(f.epoch) / 60),
      orden: "dev:" + String(f.epoch).padStart(12, "0") + ":" + String(f.ventana).padStart(9, "0"),
      origen: "celular",
    }));
  }

  // Rasgos a usar: solo espectro (sin trafico WiFi) y que no sean constantes
  function elegirRasgos(ejemplos, usarWifi = false) {
    return NOMBRES.filter((n) => {
      if (!usarWifi && WIFI.includes(n)) return false;
      let min = Infinity, max = -Infinity;
      for (const e of ejemplos) { const v = e.x[n]; if (v < min) min = v; if (v > max) max = v; }
      return max > min;
    });
  }

  // ---------------------------------------------------------------- modelo
  function normalizador(X) {
    const d = X[0].length, n = X.length;
    const media = new Float64Array(d), escala = new Float64Array(d);
    for (const f of X) for (let j = 0; j < d; j++) media[j] += f[j];
    for (let j = 0; j < d; j++) media[j] /= n;
    for (const f of X) for (let j = 0; j < d; j++) escala[j] += (f[j] - media[j]) ** 2;
    for (let j = 0; j < d; j++) { escala[j] = Math.sqrt(escala[j] / n); if (escala[j] === 0) escala[j] = 1; }
    return { media, escala };
  }

  // Perdida y gradiente: sum_i w_i * entropia_cruzada + 0.5 * ||W||^2  (sesgos sin penalizar)
  function perdida(theta, Z, y, w, K, d) {
    const n = Z.length, g = new Float64Array(theta.length);
    let f = 0;
    for (let k = 0; k < K; k++) for (let j = 0; j < d; j++) {
      const v = theta[k * (d + 1) + j]; f += 0.5 * v * v; g[k * (d + 1) + j] = v;
    }
    const z = new Float64Array(K);
    for (let i = 0; i < n; i++) {
      const xi = Z[i];
      let mx = -Infinity;
      for (let k = 0; k < K; k++) {
        let s = theta[k * (d + 1) + d];
        const off = k * (d + 1);
        for (let j = 0; j < d; j++) s += theta[off + j] * xi[j];
        z[k] = s; if (s > mx) mx = s;
      }
      let suma = 0;
      for (let k = 0; k < K; k++) { z[k] = Math.exp(z[k] - mx); suma += z[k]; }
      f += w[i] * (Math.log(suma) - Math.log(z[y[i]]));
      for (let k = 0; k < K; k++) {
        const r = w[i] * (z[k] / suma - (k === y[i] ? 1 : 0));
        const off = k * (d + 1);
        for (let j = 0; j < d; j++) g[off + j] += r * xi[j];
        g[off + d] += r;
      }
    }
    return { f, g };
  }

  // L-BFGS con busqueda de linea por retroceso (Armijo)
  function lbfgs(fg, x0, maxIter = 1000, tol = 1e-6, m = 10) {
    let x = Float64Array.from(x0), { f, g } = fg(x);
    const S = [], Y = [], R = [];
    const punto = (a, b) => { let s = 0; for (let i = 0; i < a.length; i++) s += a[i] * b[i]; return s; };
    const norma0 = Math.sqrt(punto(g, g)) || 1;
    for (let it = 0; it < maxIter; it++) {
      // direccion = -H g (dos lazos)
      const q = Float64Array.from(g), alfa = [];
      for (let i = S.length - 1; i >= 0; i--) {
        const a = R[i] * punto(S[i], q); alfa[i] = a;
        for (let j = 0; j < q.length; j++) q[j] -= a * Y[i][j];
      }
      const gamma = S.length ? punto(S[S.length - 1], Y[Y.length - 1]) / punto(Y[Y.length - 1], Y[Y.length - 1]) : 1 / norma0;
      for (let j = 0; j < q.length; j++) q[j] *= gamma;
      for (let i = 0; i < S.length; i++) {
        const b = R[i] * punto(Y[i], q);
        for (let j = 0; j < q.length; j++) q[j] += S[i][j] * (alfa[i] - b);
      }
      for (let j = 0; j < q.length; j++) q[j] = -q[j];
      let gd = punto(g, q);
      if (gd >= 0) { for (let j = 0; j < q.length; j++) q[j] = -g[j]; gd = -punto(g, g); S.length = Y.length = R.length = 0; }
      let paso = 1, xn, nuevo;
      for (let k = 0; k < 40; k++) {
        xn = x.map((v, j) => v + paso * q[j]);
        nuevo = fg(xn);
        if (nuevo.f <= f + 1e-4 * paso * gd) break;
        paso *= 0.5;
      }
      const s = xn.map((v, j) => v - x[j]), yv = nuevo.g.map((v, j) => v - g[j]);
      const sy = punto(s, yv);
      if (sy > 1e-10) { S.push(s); Y.push(yv); R.push(1 / sy); if (S.length > m) { S.shift(); Y.shift(); R.shift(); } }
      const mejora = f - nuevo.f;
      x = xn; f = nuevo.f; g = nuevo.g;
      if (Math.sqrt(punto(g, g)) < tol * norma0 || Math.abs(mejora) < 1e-12 * Math.max(1, Math.abs(f))) break;
    }
    return x;
  }

  // ejemplos: [{x, y}], rasgos: nombres. Devuelve {rasgos, media, escala, pesos[3][d], sesgo[3]}
  function entrenar(ejemplos, rasgos) {
    const K = 3, d = rasgos.length;
    const X = ejemplos.map((e) => rasgos.map((n) => e.x[n]));
    const y = ejemplos.map((e) => e.y);
    const { media, escala } = normalizador(X);
    const Z = X.map((f) => f.map((v, j) => (v - media[j]) / escala[j]));
    const cuenta = [0, 0, 0];
    for (const c of y) cuenta[c]++;
    const presentes = cuenta.filter((c) => c > 0).length;
    const w = y.map((c) => y.length / (presentes * cuenta[c]));   // class_weight="balanced"
    const theta = lbfgs((t) => perdida(t, Z, y, w, K, d), new Float64Array(K * (d + 1)));
    const pesos = [], sesgo = [];
    for (let k = 0; k < K; k++) { pesos.push(Array.from(theta.slice(k * (d + 1), k * (d + 1) + d))); sesgo.push(theta[k * (d + 1) + d]); }
    return { rasgos, media: Array.from(media), escala: Array.from(escala), pesos, sesgo };
  }

  function probabilidades(modelo, x) {
    const z = modelo.pesos.map((p, k) => modelo.sesgo[k] + p.reduce((s, w, j) => s + w * (x[modelo.rasgos[j]] - modelo.media[j]) / modelo.escala[j], 0));
    const mx = Math.max(...z), e = z.map((v) => Math.exp(v - mx)), s = e.reduce((a, b) => a + b, 0);
    return e.map((v) => v / s);
  }

  function predecir(modelo, x) {
    const p = probabilidades(modelo, x);
    return p.indexOf(Math.max(...p));
  }

  // ---------------------------------------------------------------- evaluacion
  // Separa bloques de 1 minuto: nunca el mismo minuto para entrenar y para probar.
  function validar(ejemplos, rasgos, pliegues = 10) {
    const grupos = [...new Set(ejemplos.map((e) => e.grupo))].sort();
    // reparto fijo (siempre igual) de los grupos en pliegues
    let semilla = 12345;
    const azar = () => (semilla = (semilla * 1103515245 + 12345) % 2147483648) / 2147483648;
    for (let i = grupos.length - 1; i > 0; i--) { const j = Math.floor(azar() * (i + 1)); [grupos[i], grupos[j]] = [grupos[j], grupos[i]]; }
    const plieguePorGrupo = new Map(grupos.map((g, i) => [g, i % pliegues]));
    const pred = new Array(ejemplos.length);
    for (let p = 0; p < pliegues; p++) {
      const entreno = ejemplos.filter((e) => plieguePorGrupo.get(e.grupo) !== p);
      if (!entreno.length) continue;
      const m = entrenar(entreno, rasgos);
      ejemplos.forEach((e, i) => { if (plieguePorGrupo.get(e.grupo) === p) pred[i] = predecir(m, e.x); });
    }
    return resumen(ejemplos, pred);
  }

  function resumen(ejemplos, pred) {
    const matriz = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
    let bien = 0;
    ejemplos.forEach((e, i) => { matriz[e.y][pred[i]]++; if (e.y === pred[i]) bien++; });
    // falsas alarmas: sin dron y el modelo dijo dron; con la regla de 2 ventanas seguidas
    const orden = ejemplos.map((e, i) => i).sort((a, b) => (ejemplos[a].orden < ejemplos[b].orden ? -1 : 1));
    let sueltas = 0, seguidas = 0;
    for (let k = 0; k < orden.length; k++) {
      const i = orden[k];
      if (ejemplos[i].y !== 2 && pred[i] === 2) {
        sueltas++;
        const ant = orden[k - 1];
        if (k > 0 && pred[ant] === 2 && ejemplos[ant].y !== 2 &&
            ejemplos[ant].orden.split(":")[0] === ejemplos[i].orden.split(":")[0]) seguidas++;
      }
    }
    const sinDron = ejemplos.filter((e) => e.y !== 2).length;
    return { exactitud: bien / ejemplos.length, matriz, falsasSueltas: sueltas, falsasSeguidas: seguidas,
             horasSinDron: sinDron * 2 / 3600, n: ejemplos.length };
  }

  // ---------------------------------------------------------------- envio al S3
  function fnv1a(texto) {
    let h = 0x811c9dc5;
    for (let i = 0; i < texto.length; i++) { h ^= texto.charCodeAt(i) & 0xff; h = Math.imul(h, 0x01000193) >>> 0; }
    return h >>> 0;
  }

  // Linea "M,..." que entiende modelo_activo.h
  function lineaModelo(modelo, version, exactitud) {
    const num = (v) => Number(v).toPrecision(8).replace(/\.?0+e/, "e").replace(/(\.\d*?)0+$/, "$1").replace(/\.$/, "");
    const partes = ["M", String(version >>> 0), exactitud.toFixed(4), String(modelo.rasgos.length),
      ...modelo.rasgos, ...modelo.media.map(num), ...modelo.escala.map(num),
      ...modelo.pesos[0].map(num), ...modelo.pesos[1].map(num), ...modelo.pesos[2].map(num),
      ...modelo.sesgo.map(num)];
    const cuerpo = partes.join(",");
    return cuerpo + "," + fnv1a(cuerpo).toString(16).padStart(8, "0");
  }

  return { CLASES, NOMBRES, WIFI, leerCSV, ejemplosBase, ejemplosDispositivo, elegirRasgos,
           entrenar, probabilidades, predecir, validar, lineaModelo, fnv1a };
})();

if (typeof module !== "undefined") module.exports = Entrenamiento;
