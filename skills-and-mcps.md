# Skills y MCPs disponibles

_Inventario de la sesión OpenCode del 2026-10-18._

## MCPs (6)

| MCP | Tools | Para qué |
|---|---|---|
| **serial** | 27 | Conectar leer/escribir en puertos serie — canal con el ESP32. |
| **engram** | 18 | Memoria persistente: decisiones, bugs y contexto entre sesiones. |
| **opencode** | 5 | Gestionar sesiones, recursos MCP y configuración de OpenCode. |
| **context7** | 2 | Docs oficiales de librerías/frameworks al día bajo demanda. |
| **azure-mcp** | 69 | Todo Azure: recursos, IoT Hub, CLI, despliegues. |
| **canva** | 48 | Crear/editar diseños y brand templates en Canva. |

## Skills (~84)

### Desarrollo / código

- **esp32-firmware-engineer** — firmware ESP-IDF: escribir, revisar, debuggear, flash.
- **ponytail** — modo dev senior perezoso: el cambio más chico que resuelve.
- **ponytail-review / ponytail-audit / ponytail-debt / ponytail-gain / ponytail-help** — review de cambios, auditoría de repo, ledger de deuda, métricas de ahorro, ayuda.
- **tdd** — desarrollo test-first red-green-refactor.
- **go-testing** — tests en Go (teatest, golden files).
- **code-review** — review de branch/PR por estándares y spec en paralelo.
- **judgment-day** — review adversarial dual a ciegas.
- **rdd-defect-workflow** — receipt-driven development y defectos acotados.
- **diagnosing-bugs** — loop de diagnóstico para bugs difíciles o regresiones.
- **refactor-structure** — reordenar arquitectura de argumentos (papers) o estructura.
- **work-unit-commits** — planear commits como unidades revisables.
- **chained-pr** — partir cambios grandes en PRs encadenados.
- **resolving-merge-conflicts** — resolver merges en curso.
- **using-git-worktrees** — worktrees aislados para trabajo feature.
- **python-performance-optimization** — perfilar y optimizar Python (cProfile, heaps).
- **migrate-to-shoehorn** — migrar `as` assertions a shoehorn.
- **codebase-design** — vocabulario de módulos profundos y interfaces.
- **domain-modeling** — modelo de dominio, CONTEXT.md, ADRs.

### Docs y escritura

- **docs / docx / pdf / xlsx / pptx** — crear/editar cada formato (Word, PDF, Excel, slides).
- **google-workspace** — Google Docs/Sheets/Slides en tu Drive.
- **pr** — escribir cuerpos de PR.
- **comment-writer** — comentarios cálidos y directos en PRs/issues/Slack.
- **issue-creation** — crear y triagear issues de GitHub desde evidencia del repo.
- **report** — reportar bugs de OpenCode en GitHub.
- **internal-comms** — comunicaciones internas (status, updates, FAQs).
- **prose-guard** — quitar el tono "escrito por IA" antes de redactar.
- **cognitive-doc-design** — docs de baja carga cognitiva (RFCs, guías, README).
- **writing-for-agents** — escribir documentos para agentes (skills, AGENTS.md).

### Académico

- **academic-writing / scientific-paper / polish-prose / paper-polish / paper-audit / fit-page-limit / preflight-check / latex-formatting / polish-tables-figures** — escribir, pulir, auditar y maquetar papers académicos (LaTeX, tablas, límites de página, desk-reject).

### Diseño / frontend / datos

- **frontend-design** — dirección visual intencional para UI.
- **canvas-design** — arte/afiches en PNG/PDF.
- **algorithmic-art** — arte generativo con p5.js.
- **web-artifacts-builder** — artifacts HTML complejos (React/Tailwind).
- **theme-factory** — temas para slides/docs/web.
- **brand-guidelines** — colores/tipografía oficiales de Anthropic.
- **developing-with-streamlit** — todo Streamlit (apps, styling, deploy).
- **webapp-testing** — Playwright para probar webapps locales.
- **prototype** — prototipo descartable para validar una UI.
- **scaffold-exercises** — ejercicios de curso con problemas/soluciones.

### DevOps / cloud

- **setup-pre-commit** — hooks Husky + lint-staged + typecheck.
- **git-guardrails-claude-code** — bloquear comandos git peligrosos.
- **python-appservice-deploy** — desplegar Python en Azure App Service.
- **mcp-builder** — construir servidores MCP de alta calidad.

### IA / meta

- **opencode** — todo sobre OpenCode (config, plugins, SDK, migración).
- **claude-api** — API/SDK de Claude: modelos, pricing, params.
- **skill-creator / skill-improver / skill-registry / find-skills** — crear, mejorar, indexar y descubrir skills.
- **academy-guide** — recomendar cursos de Claude Academy.
- **gentle-ai-bench** — journeys/bench de gentle-ai.
- **grilling** — estresar un plan o idea con preguntas duras.
- **discernment-nudge** — follow-up questions para chequear mis respuestas.
- **import-memory** — importar memoria de otro asistente.
- **systemic-issue-triage** — atacar issues por clase raíz, no una por una.

### Utilidades

- **deep-research / research** — research multi-fuente (con subagentes / a archivo).
- **morning** — brief matutino HTML recurrente.
- **wizard** — bash interactivo para pasos que solo un humano hace.
- **slack-gif-creator** — GIFs animados optimizados para Slack.
- **doc-coauthoring** — co-authoring estructurado de documentos.
- **computer-use / built-in-browser / chrome-browser** — controlar tu PC / navegadores.

---

## Notas

- Skills tomadas del catálogo de la sesión (no se contaron una por una: ~84).
- MCPs con resources vacíos no listados.
- MCPs configurados pero deshabilitados en el proyecto no aparecen aquí.
- Para ESP32: falta habilitar `SERIAL_MCP_PLUGINS` y verificar que `idf.py` resuelva.
