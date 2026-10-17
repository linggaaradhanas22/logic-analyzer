import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { writeFileSync } from 'node:fs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const {chromium}=await import(pathToFileURL(path.join(process.env.PLAYWRIGHT_MODULE_DIR,'index.mjs')).href);
const browser=await chromium.launch({headless:true});
try {
  const page=await browser.newPage({viewport:{width:1150,height:950},deviceScaleFactor:2});
  const errors=[];page.on('pageerror',e=>errors.push(String(e)));
  await page.goto(process.argv[2]);
  await page.locator('#open-file').setInputFiles(path.join(root,'build/presentation/illustrative-uart.sr'));
  await page.waitForFunction(()=>window.LAApp?.state.capture?.total_samples>0);
  await page.locator('#btn-decode').click();
  await page.waitForFunction(()=>document.getElementById('decode-status').textContent.includes('decoded events loaded'));
  await page.locator('#btn-zoom-in').click();
  await page.locator('#btn-zoom-in').click();
  await page.locator('#btn-zoom-in').click();
  // Use the application's normal visibility controls to focus the illustration.
  const toggles=page.locator('#chan-list input[type="checkbox"]');
  for(let i=1;i<8;i++) await toggles.nth(i).uncheck();
  await page.locator('#btn-cursor-a').click();
  await page.waitForTimeout(400);
  await page.screenshot({path:path.join(root,'build/presentation/ui-offline-full.png')});
  await page.locator('.canvas-wrap').first().screenshot({path:path.join(root,'build/presentation/ui-offline-waveform.png')});
  const state=await page.evaluate(()=>({title:document.getElementById('capture-title').textContent,meta:document.getElementById('capture-sub').textContent,decode:document.getElementById('decode-status').textContent,measurement:document.getElementById('measure-text').textContent,cursorA:window.LAApp.state.waveform.cursors.a,offline:!window.LAApp.state.connected,visibleChannels:window.LAApp.state.channels.filter(c=>c.visible).map(c=>c.index)}));
  if(state.cursorA===null || state.visibleChannels.join(',')!=='0') throw new Error('Expected focused CH0/cursor illustration');
  writeFileSync(path.join(root,'build/presentation/ui-screenshot-evidence.json'),JSON.stringify({source:'build/presentation/illustrative-uart.sr',synthetic:true,configuration:'CH0 115200-baud UART 8N1, LA-TEST newline, 10 MS/s',state,errors,newHardwareRun:false},null,2));
  console.log(JSON.stringify({state,errors},null,2));
  if(errors.length) process.exitCode=1;
} finally {await browser.close();}
