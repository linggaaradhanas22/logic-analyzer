// Print the same Markdown-derived content as the editable Word report.
import path from 'node:path';
import os from 'node:os';
import {fileURLToPath, pathToFileURL} from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const out = path.join(root, 'build/reports/project-documentation');
const modulePath = path.join(os.homedir(), '.config/opencode/skills/consulting-pptx-skill/node_modules/playwright-core/index.mjs');
const {chromium} = await import(pathToFileURL(modulePath).href);
const browser = await chromium.launch({headless:true, channel:'msedge'});
try {
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  await page.goto(pathToFileURL(path.join(out, 'project-report.html')).href);
  await page.evaluate(() => document.fonts.ready);
  const images = await page.locator('img').evaluateAll(elements => elements.map(e => ({complete:e.complete,width:e.naturalWidth})));
  if (images.length !== 4 || images.some(e => !e.complete || e.width === 0)) throw new Error('Missing figure images');
  await page.pdf({
    path:path.join(out,'Logic Analyzer with Live Protocol Decoding_Lingga Aradhana Sahadewa.pdf'),
    format:'A4', printBackground:true, displayHeaderFooter:true,
    headerTemplate:'<div style="font-size:8pt;color:#607685;margin-left:19mm">LOGIC ANALYZER | FPGA + EMBEDDED SYSTEM</div>',
    footerTemplate:'<div style="font-size:8pt;width:100%;color:#607685;text-align:right;padding-right:19mm">Lingga Aradhana Sahadewa • 12 October 2026 | <span class="pageNumber"></span> / <span class="totalPages"></span></div>',
    margin:{top:'18mm',bottom:'18mm',left:'19mm',right:'19mm'}
  });
  if (errors.length) throw new Error(errors.join(';'));
  console.log('Chromium report PDF PASS; four figures loaded; zero page errors');
} finally { await browser.close(); }
