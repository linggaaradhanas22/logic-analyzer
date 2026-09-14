import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { StatusBadge } from '@/components/docs/status-badge';
import { REGISTERS } from '@/lib/docs/registers';

export const metadata = {
  title: 'Register reference — RTOS Logic Analyzer',
};

const GROUPS: { name: string; names: string[]; note: string }[] = [
  { name: 'Identity and state', names: ['ID', 'STATUS'], note: 'Read-only information about the loaded design and the acquisition state.' },
  { name: 'Acquisition control', names: ['CTRL', 'SAMPLE_DIV', 'MODE', 'CHAN_EN', 'TIMEOUT', 'LIVE_FLUSH_CYCLES'], note: 'Mode, cadence and lifecycle control; ARM, ABORT and SOFT_RESET are self-clearing.' },
  { name: 'Trigger and capture window', names: ['TRIG_MASK', 'TRIG_VALUE', 'TRIG_RISE', 'TRIG_FALL', 'PRE_SAMPLES', 'POST_SAMPLES', 'TRIG_INDEX'], note: 'What qualifies a trigger and how much history is kept.' },
  { name: 'Data accounting', names: ['WORD_COUNT_LO', 'WORD_COUNT_HI'], note: '64-bit count of emitted stream words.' },
  { name: 'Interrupts', names: ['IRQ_EN', 'IRQ_STAT'], note: 'DONE, TRIGGERED and OVERFLOW events; IRQ_STAT is write-one-to-clear.' },
  { name: 'Pattern generator', names: ['PATGEN_CTRL'], note: 'Self-test pattern enable, mode and divider.' },
  { name: 'Transition encoding', names: ['TXN_MAX_RUN'], note: 'Maximum run length before a transition record is split.' },
];

export default function RegistersPage() {
  const byName = new Map(REGISTERS.map((register) => [register.name, register]));
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Reference' }, { label: 'Registers' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Reference · Normative map</p>
          <h1>Register reference</h1>
          <p className="doc-answer">
            The analyzer exposes {REGISTERS.length} registers in a 64 KiB AXI4-Lite slave at base address
            {' '}
            <code className="doc-inline-code">0x40000000</code>. The normative source is
            {' '}
            <code className="doc-inline-code">docs/regmap.yaml</code>; the C, SystemVerilog and Python
            views are generated from it and must never be hand-edited.
          </p>
          <div className="doc-status-row">
            <StatusBadge status="bench-verified" label="Read/write round trip proven on target" />
            <span className="doc-scope">
              The P4.4 self-test read ID 0x4C410002 and round-tripped SAMPLE_DIV through the real AXI
              fabric; register semantics also have a self-checking Questa test (tb_la_regs).
            </span>
          </div>
        </header>

        <section className="doc-section">
          <h2>Conventions</h2>
          <ul className="doc-prose">
            <li><strong>Offsets</strong> are relative to the la_top base <code className="doc-inline-code">0x40000000</code>.</li>
            <li><strong>Self-clearing</strong> bits (ARM, ABORT, SOFT_RESET) read back as zero after the command is accepted.</li>
            <li><strong>Sticky</strong> status bits (FIFO_OVF, ABORTED) stay set until the next ARM or a reset; STATUS.IDLE is computed, not latched.</li>
            <li><strong>W1C</strong> means write one to clear: IRQ_STAT bits are cleared only by writing a 1 to them.</li>
            <li><strong>Clamps</strong>: PRE_SAMPLES saturates at 32,768 and TXN_MAX_RUN at 0x00FFFFFF; masked fields ignore bits outside their mask.</li>
            <li>Out-of-range reads return zero and out-of-range writes are ignored, matching the RTL and the host driver test.</li>
          </ul>
        </section>

        {GROUPS.map((group) => (
          <section className="doc-section" key={group.name}>
            <h2>{group.name}</h2>
            <p className="doc-scope">{group.note}</p>
            <div className="doc-grid">
              {group.names.map((name) => {
                const register = byName.get(name);
                if (!register) return null;
                return (
                  <div className="doc-panel" key={name}>
                    <div className="doc-status-row" style={{ justifyContent: 'space-between' }}>
                      <h3>
                        <code className="doc-inline-code">{register.name}</code>
                      </h3>
                      <span className="doc-scope">
                        {register.offset} · {register.access} · reset {register.reset}
                      </span>
                    </div>
                    <p>{register.description}</p>
                    {register.fields.length ? (
                      <div className="doc-table-wrap" style={{ marginTop: 12 }}>
                        <table className="doc-table">
                          <caption>Fields</caption>
                          <thead>
                            <tr>
                              <th scope="col">Field</th>
                              <th scope="col">Bits</th>
                              <th scope="col">Values</th>
                            </tr>
                          </thead>
                          <tbody>
                            {register.fields.map((field) => (
                              <tr key={field.name}>
                                <td><code>{field.name}</code></td>
                                <td className="num">
                                  {field.width === 1 ? `[${field.lsb}]` : `[${field.lsb + field.width - 1}:${field.lsb}]`}
                                </td>
                                <td>
                                  {field.values
                                    ? Object.entries(field.values).map(([value, label]) => `${value}=${label}`).join(', ')
                                    : '—'}
                                </td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    ) : null}
                    {register.constraints.length ? (
                      <p className="doc-scope">Constraints: {register.constraints.join('; ')}</p>
                    ) : null}
                    <p className="doc-scope">Used by: {register.usedBy.join(', ')}</p>
                  </div>
                );
              })}
            </div>
          </section>
        ))}
      </div>
    </main>
  );
}
