"""Author native draw.io report figures using the hardware-architecture preset."""
from pathlib import Path
import json
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SKILL = Path.home() / '.config/opencode/skills/drawio-skill'
PRESET = json.loads((SKILL / 'styles/built-in/hardware-architecture.json').read_text())
DEST = ROOT / 'docs/hardwarex/diagrams/project-report_v2.drawio'
OUT = ROOT / 'build/reports/project-documentation/figures'
DRAWIO = Path.home() / 'AppData/Local/Programs/draw.io/draw.io.EXE'


class Page:
    def __init__(self, name, title, height=700):
        self.diagram = ET.Element('diagram', name=name, id=name)
        self.model = ET.SubElement(self.diagram, 'mxGraphModel', page='1',
                                   pageWidth='1260', pageHeight=str(height),
                                   grid='1', gridSize='10', background='#ffffff')
        self.root = ET.SubElement(self.model, 'root')
        ET.SubElement(self.root, 'mxCell', id='0')
        ET.SubElement(self.root, 'mxCell', id='1', parent='0')
        self.text('title', title, 30, 20, 1180, 40, size=24, bold=True)

    def text(self, id, text, x, y, w, h, size=17, bold=False):
        return self.node(id, text, x, y, w, h, style=(
            f'text;html=1;whiteSpace=wrap;fillColor=none;strokeColor=none;'
            f'fontFamily=Helvetica;fontSize={size};align=left;verticalAlign=middle;'
            f'fontStyle={1 if bold else 0};'))

    def node(self, id, text, x, y, w=220, h=100, role='primary', style=None, parent='1'):
        if style is None:
            pair = PRESET['palette'][role]
            style = (f'rounded=1;whiteSpace=wrap;html=1;'
                     f'fillColor={pair["fillColor"]};strokeColor={pair["strokeColor"]};'
                     'fontFamily=Helvetica;fontSize=19;align=center;verticalAlign=middle;spacing=8;')
        cell = ET.SubElement(self.root, 'mxCell', id=id, value=text,
                             style=style, vertex='1', parent=parent)
        ET.SubElement(cell, 'mxGeometry', x=str(x), y=str(y), width=str(w),
                      height=str(h), **{'as': 'geometry'})
        return cell

    def edge(self, id, source, target, label='', pins=(1, .5, 0, .5),
             points=(), dashed=False, two=False, offset=(0, -20)):
        a, b, c, d = pins
        style = (PRESET['edges']['style'] + ';endArrow=block;endSize=8;endFill=1;'
                 f'exitX={a};exitY={b};entryX={c};entryY={d};exitDx=0;exitDy=0;'
                 'entryDx=0;entryDy=0;fontFamily=Helvetica;fontSize=14;'
                 'labelBackgroundColor=#ffffff;strokeColor=#333333;')
        if dashed:
            style += 'dashed=1;strokeColor=#666666;'
        if two:
            style += 'startArrow=block;startSize=8;startFill=1;'
        cell = ET.SubElement(self.root, 'mxCell', id=id, source=source, target=target,
                             value=label, style=style, edge='1', parent='1')
        geo = ET.SubElement(cell, 'mxGeometry', relative='1', **{'as': 'geometry'})
        if points:
            array = ET.SubElement(geo, 'Array', **{'as': 'points'})
            for x, y in points:
                ET.SubElement(array, 'mxPoint', x=str(x), y=str(y))
        ET.SubElement(geo, 'mxPoint', x=str(offset[0]), y=str(offset[1]), **{'as': 'offset'})

    def legend(self, roles, y):
        self.text('legend-head', 'Block color = component role', 30, y, 590, 30, bold=True)
        self.text('line-legend', 'Lines: solid = data/control; dashed = configuration/reporting',
                  660, y, 570, 70, size=16)
        for i, (role, label) in enumerate(roles):
            self.node(f'legend-{role}', '', 30 + (i % 2) * 300, y + 40 + (i // 2) * 35,
                      20, 20, role=role)
            self.text(f'legend-label-{role}', label, 60 + (i % 2) * 300,
                      y + 35 + (i // 2) * 35, 240, 30, size=16)


def pages():
    p = Page('01-system', 'Figure 1. USB-first system architecture', 760)
    p.node('target', 'Target signals<br>8 channels, 3.3 V', 30, 140, 190, role='neutral')
    p.node('pl', 'FPGA capture<br>Sampling + triggering', 310, 140, 220)
    p.node('dma', 'AXI DMA + DDR<br>Ordinary SG buffers', 620, 140, 240, role='secondary')
    p.node('ps', 'Zynq CPU0<br>FreeRTOS + decoders', 960, 140, 260, role='success')
    p.node('usb', 'J10 / PS USB0<br>CDC-ACM device', 960, 390, 260, role='warning')
    p.node('host', 'PC host server<br>la-ui / la-cli', 620, 390, 240, role='success')
    p.node('browser', 'Browser + files<br>Waveforms / live events', 310, 390, 220, role='neutral')
    p.edge('inputs', 'target', 'pl', 'JC inputs')
    p.edge('stream', 'pl', 'dma', 'AXI-Stream')
    p.edge('memory', 'dma', 'ps', 'Memory / IRQ', two=True)
    p.edge('usb-frames', 'ps', 'usb', 'Frames', pins=(.5, 1, .5, 0), two=True, offset=(45, 0))
    p.edge('host-usb', 'usb', 'host', 'USB', pins=(0, .5, 1, .5), two=True)
    p.edge('browser-host', 'host', 'browser', 'HTTP / SSE', pins=(0, .5, 1, .5), two=True)
    p.edge('registers', 'ps', 'pl', 'AXI4-Lite configuration / status',
           pins=(.5, 0, .5, 0), points=((1090, 100), (420, 100)), dashed=True)
    p.legend([('primary', 'FPGA pipeline'), ('secondary', 'DMA / memory'),
              ('success', 'Processor software'), ('warning', 'USB interface'),
              ('neutral', 'External signal / host')], 550)

    f = Page('02-fpga', 'Figure 2. FPGA sampling and capture pipeline', 790)
    f.node('jc', 'JC inputs<br>CH0-CH7', 30, 140, 170, role='neutral')
    f.node('sync', 'Input synchronization<br>+ self-test selection', 270, 140, 260)
    f.node('sample', 'Sampler<br>100 MHz / (DIV + 1)', 610, 140, 260)
    f.node('trigger', 'Level / edge trigger<br>+ pre-trigger history', 960, 140, 270)
    f.node('pack', 'Raw / transition packing<br>4 samples / word<br>or run[23:0] + sample[7:0]', 960, 390, 270, 140)
    f.node('fifo', 'AXI-Stream FIFO<br>Backpressure + packet close', 610, 390, 260, 140)
    f.node('dma', 'AXI DMA S2MM<br>Ordinary SG descriptors', 270, 390, 260, 140, role='secondary')
    f.node('ddr', 'DDR capture<br>buffers', 30, 390, 170, 140, role='secondary')
    f.edge('sync-input', 'jc', 'sync', '8 bits')
    f.edge('sample-input', 'sync', 'sample', '8 bits')
    f.edge('trigger-input', 'sample', 'trigger', 'Samples')
    f.edge('window', 'trigger', 'pack', 'Capture window', pins=(.5, 1, .5, 0), offset=(65, 0))
    f.edge('packed', 'pack', 'fifo', '32-bit words', pins=(0, .5, 1, .5))
    f.edge('axis', 'fifo', 'dma', 'AXIS / TLAST', pins=(0, .5, 1, .5))
    f.edge('write', 'dma', 'ddr', 'AXI writes', pins=(0, .5, 1, .5))
    f.text('clock-note', 'Blue blocks share FCLK 100 MHz; DIV changes the sample enable, not the fabric clock.',
           30, 580, 1180, 40, size=18)
    f.legend([('primary', '100 MHz FPGA logic'), ('secondary', 'DMA / DDR interface'),
              ('neutral', 'External inputs')], 640)

    r = Page('03-rtos', 'Figure 3. FreeRTOS live-data and command pipeline', 970)
    r.node('irq', 'PL / DMA IRQ<br>Acknowledge + notify', 30, 140, 230, role='neutral')
    r.node('acq', 'AcqTask (priority 6)<br>Own SG descriptors', 350, 140, 250, role='success')
    r.node('decode', 'Decoder slots 0 / 1 / 2<br>Priority 5<br>UART, SPI or I2C', 700, 140, 270, 130, role='success')
    r.node('tx', 'TxTask (priority 5)<br>Pack events / capture data', 700, 440, 270, 120, role='success')
    r.node('usb', 'UsbTask (priority 5)<br>Stage RX / submit TX', 350, 440, 250, 120, role='success')
    r.node('pc', 'PC USB host<br>la-ui / la-cli', 30, 440, 230, 120, role='neutral')
    r.node('cmd', 'CmdTask (priority 5)<br>Parse + validate requests', 350, 720, 250, 100, role='success')
    r.node('status', 'StatusTask (priority 1)<br>5 s watermark loop', 700, 720, 270, 100, role='success')
    r.node('pool', 'DDR live pool<br>16 x 256 KiB<br>Shared segment handles', 1030, 140, 200, 130, role='secondary')
    r.edge('notify', 'irq', 'acq', 'Notification')
    r.edge('publish', 'acq', 'decode', 'seg_q (8 / slot)')
    r.edge('read', 'pool', 'decode', 'Read', pins=(0, .5, 1, .5))
    r.edge('batch', 'decode', 'tx', 'ev_q (8 handles)', pins=(.5, 1, .5, 0), offset=(75, 0))
    r.edge('release', 'decode', 'acq', 'Release reference + notify',
           pins=(.3, 0, .7, 0), points=((780, 100), (520, 100)), dashed=True)
    r.edge('usb-tx', 'tx', 'usb', '16-slot TX pool', pins=(0, .5, 1, .5))
    r.edge('link', 'usb', 'pc', 'USB frames', pins=(0, .5, 1, .5), two=True)
    r.edge('commands', 'usb', 'cmd', 'rx_sb (8 KiB)', pins=(.5, 1, .5, 0), offset=(70, 0))
    r.edge('control', 'cmd', 'acq', 'ctrl_q (4)', pins=(0, .5, 0, .8),
           points=((300, 770), (300, 220)), offset=(0, 130))
    r.edge('responses', 'cmd', 'tx', 'tx_q / ack_q', pins=(1, .5, .185185185185, 1),
           points=((640, 770), (640, 620), (750, 620)), offset=(65, 20))
    r.legend([('success', 'FreeRTOS task'), ('secondary', 'DMA payload memory'),
              ('neutral', 'IRQ / external host')], 860)

    o = Page('04-ownership', 'Figure 4. Segment ownership and safe DMA reuse', 650)
    o.node('free', 'FREE<br>No outstanding consumer', 30, 170, 300, 130, role='neutral')
    o.node('dma', 'DMA_OWNED<br>Hardware may write payload', 460, 170, 300, 130, role='secondary')
    o.node('cpu', 'CPU_OWNED<br>Enabled decoders hold references', 910, 170, 320, 130, role='success')
    o.edge('claim', 'free', 'dma', 'AcqTask posts BD')
    o.edge('complete', 'dma', 'cpu', 'Complete + publish')
    o.edge('recycle', 'cpu', 'free', 'Last consumer releases; AcqTask may repost',
           pins=(.5, 1, .5, 1), points=((1070, 390), (180, 390)), offset=(0, 30))
    o.text('ownership-note', 'Queue-full policy: release only that decoder\'s reference; count the drop and expose a sequence gap.',
           30, 460, 1190, 50, size=19)
    o.legend([('neutral', 'Available segment'), ('secondary', 'Hardware ownership'),
              ('success', 'CPU consumer ownership')], 540)
    return [p, f, r, o]


def main():
    assert DEST.parent.is_dir() and (ROOT / 'build/reports/project-documentation').is_dir()
    OUT.mkdir(exist_ok=True)
    xml = ET.Element('mxfile', host='drawio', version='31.4.5')
    for page in pages():
        xml.append(page.diagram)
    ET.indent(xml)
    ET.ElementTree(xml).write(DEST, encoding='utf-8', xml_declaration=True)
    subprocess.run([sys.executable, str(SKILL / 'scripts/validate.py'), str(DEST), '--score'], check=True)
    for index, page in enumerate(xml, 1):
        filename = page.get('name')
        for fmt in ['png', 'svg']:
            args = [str(DRAWIO), '-x', '-f', fmt, '-p', str(index), '-o', str(OUT / f'{filename}.{fmt}')]
            if fmt == 'png':
                args += ['--width', '2000']
            else:
                args += ['--embed-svg-images', '--embed-svg-fonts', 'false']
            subprocess.run(args + [str(DEST)], check=True, timeout=90)
    print(f'Published four native draw.io pages and draft figure exports: {OUT}')


if __name__ == '__main__':
    main()
