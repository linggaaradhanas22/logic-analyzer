// Render the authored HTML and extract its existing layout for PowerPoint.
// No second layout engine: browser coordinates are the source of truth.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const out = path.join(root, 'build/presentation');
const moduleDir = process.env.PLAYWRIGHT_MODULE_DIR;
if (!moduleDir) throw new Error('Set PLAYWRIGHT_MODULE_DIR to the installed playwright directory.');
const { chromium } = await import(pathToFileURL(path.join(moduleDir, 'index.mjs')).href);
const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 1400, height: 900 }, deviceScaleFactor: 2 });
const errors = [];
page.on('pageerror', e => errors.push(String(e)));
await page.emulateMedia({ media: 'print' });
await page.goto(pathToFileURL(path.join(out, 'logic-analyzer-design.html')).href);
await page.evaluate(() => document.fonts.ready);
mkdirSync(path.join(out, 'previews'), { recursive: true });
const slides = page.locator('section.s');
const n = await slides.count();
const layouts = [];
const overflow = [];
for (let i = 0; i < n; i++) {
  const slide = slides.nth(i);
  await slide.screenshot({ path: path.join(out, `previews/slide-${String(i+1).padStart(2,'0')}.png`) });
  const graphics = slide.locator('svg,img');
  for (let j = 0; j < await graphics.count(); j++) {
    await graphics.nth(j).screenshot({ path: path.join(out, `previews/graphic-${i+1}-${j}.png`) });
  }
  layouts.push(await slide.evaluate((s, index) => {
    const sr = s.getBoundingClientRect();
    const box = r => ({ x: r.x-sr.x, y: r.y-sr.y, w: r.width, h: r.height });
    const rectangles = [];
    const texts = [];
    const images = [...s.querySelectorAll('svg,img')].map((el,j) => ({...box(el.getBoundingClientRect()), path:`previews/graphic-${index+1}-${j}.png` }));
    const isGraphic = el => el.closest('svg') || el.tagName === 'IMG';
    for (const el of [s,...s.querySelectorAll('*')]) {
      if (isGraphic(el)) continue;
      const st = getComputedStyle(el);
      const r = el.getBoundingClientRect();
      if (!r.width || !r.height || st.display==='none') continue;
      if (st.backgroundColor !== 'rgba(0, 0, 0, 0)') rectangles.push({...box(r), fill:st.backgroundColor});
      for (const side of ['Top','Right','Bottom','Left']) {
        const w = parseFloat(st[`border${side}Width`]);
        if (!w || st[`border${side}Style`] === 'none') continue;
        const b = box(r);
        if (side==='Top' || side==='Bottom') { b.h=w; if (side==='Bottom') b.y += r.height-w; }
        else { b.w=w; if(side==='Right') b.x += r.width-w; }
        rectangles.push({...b,fill:st[`border${side}Color`]});
      }
      // Group text-node characters by the browser's actual line positions.
      for (const node of el.childNodes) {
        if (node.nodeType!==Node.TEXT_NODE || !node.textContent.trim()) continue;
        const ranges = [];
        for (let k=0;k<node.length;k++) {
          const range = document.createRange(); range.setStart(node,k); range.setEnd(node,k+1);
          const q = range.getBoundingClientRect();
          if (q.width===0 || q.height===0) continue;
          const previous = ranges.at(-1);
          if (previous && Math.abs(previous.y-q.y)<1) { previous.text += node.textContent[k]; previous.right = q.right; }
          else ranges.push({x:q.x,y:q.y,h:q.height,right:q.right,text:node.textContent[k]});
        }
        for (const q of ranges) {
          if (!q.text.trim()) continue;
          texts.push({x:q.x-sr.x,y:q.y-sr.y,w:q.right-q.x,h:q.h,text:q.text,font:st.fontFamily,size:parseFloat(st.fontSize),weight:st.fontWeight,color:st.color,italic:st.fontStyle==='italic', title:!!el.closest('h1'),href:el.closest('a[href]')?.getAttribute('href')});
        }
      }
      if (el.tagName==='LI') {
        texts.push({x:r.x-sr.x-18,y:r.y-sr.y,w:14,h:25,text:'',bullet:true,size:parseFloat(st.fontSize),color:st.color,weight:400});
      }
    }
    const issues = [];
    const foot = s.querySelector('.src')?.getBoundingClientRect();
    for(const t of texts) {
      if (t.x<0 || t.y<0 || t.x+t.w>sr.width+1 || t.y+t.h>sr.height+1) issues.push(`text overflow: ${t.text}`);
      if (foot && t.y+sr.y<foot.top && t.y+t.h+sr.y>foot.top+1) issues.push(`source collision: ${t.text}`);
    }
    // SVG text geometry must also fit each diagram's viewBox.
    for(const graphic of s.querySelectorAll('svg')) {
      for(const t of graphic.querySelectorAll('text')) {
        const b=t.getBBox(), vb=graphic.viewBox.baseVal;
        if(b.x<0 || b.y<0 || b.x+b.width>vb.width+1 || b.y+b.height>vb.height+1) issues.push(`SVG overflow: ${t.textContent}`);
      }
    }
    return {width:sr.width,height:sr.height,rectangles,images,texts,issues};
  }, i));
  overflow.push(...layouts.at(-1).issues.map(x=>`Slide ${i+1}: ${x}`));
}
await page.pdf({ path: path.join(out,'logic-analyzer-design.pdf'), printBackground: true, preferCSSPageSize: true, displayHeaderFooter: false });
writeFileSync(path.join(out,'layout.json'),JSON.stringify(layouts,null,2));
const report = {slides:n,pageErrors:errors,geometryIssues:overflow};
writeFileSync(path.join(out,'render-report.json'),JSON.stringify(report,null,2));
await browser.close();
console.log(JSON.stringify(report,null,2));
if(errors.length || overflow.length) process.exitCode=1;
