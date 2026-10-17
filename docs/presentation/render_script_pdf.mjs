// Print the speaking-script HTML to A4 PDF with page numbers.
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const out = path.join(root, 'build/presentation-script');
const moduleDir = process.env.PLAYWRIGHT_MODULE_DIR;
if (!moduleDir) throw new Error('Set PLAYWRIGHT_MODULE_DIR to the installed playwright directory.');
const { chromium } = await import(pathToFileURL(path.join(moduleDir, 'index.mjs')).href);
const browser = await chromium.launch({ headless: true });
try {
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  await page.goto(pathToFileURL(path.join(out, 'presentation-script.html')).href);
  await page.pdf({
    path: path.join(out, 'presentation-script.pdf'),
    format: 'A4',
    printBackground: true,
    displayHeaderFooter: true,
    headerTemplate: '<span></span>',
    footerTemplate: '<div style="width:100%;font-size:8pt;color:#607685;text-align:center;">Presentation Script — RTOS Logic Analyzer — page <span class="pageNumber"></span> of <span class="totalPages"></span></div>',
    margin: { top: '16mm', bottom: '18mm', left: '14mm', right: '14mm' },
  });
  if (errors.length) throw new Error('Page errors: ' + errors.join('; '));
  // Visual QA captures: the script start and the first slide section.
  await page.setViewportSize({ width: 900, height: 1200 });
  await page.screenshot({ path: path.join(out, 'preview-top.png'), clip: { x: 0, y: 0, width: 900, height: 1200 } });
  const slideHeading = page.locator('h2', { hasText: 'Slide 1 —' });
  await slideHeading.scrollIntoViewIfNeeded();
  await slideHeading.screenshot({ path: path.join(out, 'preview-slide1.png') });
  const quote = page.locator('blockquote').first();
  await quote.screenshot({ path: path.join(out, 'preview-quote.png') });
  console.log('PDF PASS: build/presentation-script/presentation-script.pdf');
} finally {
  await browser.close();
}
