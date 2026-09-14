import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { StatusBadge } from '@/components/docs/status-badge';
import { FRAME_SUMMARY, PAYLOAD_LAYOUTS, PROTOCOL_ERRORS, PROTOCOL_MESSAGES } from '@/lib/docs/protocol';

export const metadata = {
  title: 'Protocol reference — RTOS Logic Analyzer',
};

const GROUP_LABELS: Record<string, string> = {
  discovery: 'Discovery and control',
  live: 'Live decode',
  state: 'State and events',
  burst: 'Burst transfer',
  reliability: 'Reliability',
};

export default function ProtocolPage() {
  const groups = ['discovery', 'live', 'state', 'burst', 'reliability'] as const;
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Reference' }, { label: 'Protocol' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Reference · Normative framing</p>
          <h1>Protocol reference</h1>
          <p className="doc-answer">
            Every message between the host, the ESP32 gateway and the Zynq uses one frame format with a
            CRC16 check. Capture data is additionally protected end to end by a CRC32 over the whole
            capture, so a missing or reordered chunk cannot pass silently.
          </p>
          <div className="doc-status-row">
            <StatusBadge status="bench-verified" label="Framing used on every bench session" />
            <span className="doc-scope">
              The same frame format carries commands, ACKs, status, burst data and live events over the
              UART and the P9.1 SPI bulk link; the normative source is docs/protocol.md.
            </span>
          </div>
        </header>

        <section className="doc-section">
          <h2>Frame anatomy</h2>
          <ul className="doc-prose">
            {FRAME_SUMMARY.map((line) => (
              <li key={line}>{line}</li>
            ))}
          </ul>
        </section>

        {groups.map((group) => (
          <section className="doc-section" key={group}>
            <h2>{GROUP_LABELS[group]}</h2>
            <div className="doc-table-wrap">
              <table className="doc-table">
                <caption>Messages in this group</caption>
                <thead>
                  <tr>
                    <th scope="col">Type</th>
                    <th scope="col">Name</th>
                    <th scope="col">Direction</th>
                    <th scope="col">Payload</th>
                    <th scope="col">Support</th>
                  </tr>
                </thead>
                <tbody>
                  {PROTOCOL_MESSAGES.filter((message) => message.group === group).map((message) => (
                    <tr key={message.type}>
                      <td className="num">{message.type}</td>
                      <td><code>{message.name}</code></td>
                      <td>{message.direction}</td>
                      <td>{message.payload}</td>
                      <td>
                        <StatusBadge status={message.supportStatus} label={message.supportStatus.replace(/-/g, ' ')} />
                        <p className="doc-scope" style={{ marginTop: 6 }}>{message.support}</p>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </section>
        ))}

        <section className="doc-section">
          <h2>Exact payload layouts</h2>
          <div className="doc-grid">
            {PAYLOAD_LAYOUTS.map((layout) => (
              <div className="doc-panel" key={layout.name}>
                <h3>{layout.name}</h3>
                <p className="doc-scope">Total: {layout.total}</p>
                <div className="doc-table-wrap" style={{ marginTop: 12 }}>
                  <table className="doc-table">
                    <caption>{layout.name}</caption>
                    <thead>
                      <tr>
                        <th scope="col">Field</th>
                        <th scope="col">Bytes</th>
                        <th scope="col">Meaning</th>
                      </tr>
                    </thead>
                    <tbody>
                      {layout.fields.map((field) => (
                        <tr key={field.field}>
                          <td><code>{field.field}</code></td>
                          <td className="num">{field.bytes}</td>
                          <td>{field.meaning}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
                <p className="doc-scope">{layout.note}</p>
              </div>
            ))}
          </div>
        </section>

        <section className="doc-section">
          <h2>Error model</h2>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>Protocol error codes and where they surface</caption>
              <thead>
                <tr>
                  <th scope="col">Code</th>
                  <th scope="col">Name</th>
                  <th scope="col">Meaning</th>
                  <th scope="col">Where</th>
                </tr>
              </thead>
              <tbody>
                {PROTOCOL_ERRORS.map((error) => (
                  <tr key={error.code}>
                    <td className="num">{error.code}</td>
                    <td><code>{error.name}</code></td>
                    <td>{error.meaning}</td>
                    <td>{error.where}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>
      </div>
    </main>
  );
}
