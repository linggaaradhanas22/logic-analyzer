import { Breadcrumbs } from '@/components/docs/breadcrumbs';
import { StatusBadge } from '@/components/docs/status-badge';
import { RTL_TESTS, RUNNER_TARGETS, TEST_LAYERS } from '@/lib/docs/verification';

export const metadata = {
  title: 'Verification — RTOS Logic Analyzer',
};

export default function VerificationPage() {
  return (
    <main className="doc-main">
      <div className="section-shell">
        <Breadcrumbs items={[{ label: 'System', href: '/' }, { label: 'Verification' }]} />
        <header className="doc-header">
          <p className="doc-eyebrow">Verification · What is proven, and how</p>
          <h1>Verification</h1>
          <p className="doc-answer">
            Each layer proves a different kind of claim: native tests prove logic, simulations prove RTL
            behavior, target Unity proves RTOS behavior, and bench runs prove the real wires. Mocks and
            compilation are never presented as target evidence.
          </p>
        </header>

        <section className="doc-section">
          <h2>The verification pyramid</h2>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>Layers, tools and what each cannot prove</caption>
              <thead>
                <tr>
                  <th scope="col">Layer</th>
                  <th scope="col">Tool</th>
                  <th scope="col">Target</th>
                  <th scope="col">Boundary</th>
                </tr>
              </thead>
              <tbody>
                {TEST_LAYERS.map((layer) => (
                  <tr key={layer.layer}>
                    <td><strong>{layer.layer}</strong></td>
                    <td>{layer.tool}</td>
                    <td><code>{layer.target}</code></td>
                    <td>{layer.boundary}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>

        <section className="doc-section">
          <h2>RTL block-to-testbench map</h2>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>One self-checking procedural Questa test per RTL block</caption>
              <thead>
                <tr>
                  <th scope="col">Block</th>
                  <th scope="col">Testbench</th>
                  <th scope="col">Covers</th>
                </tr>
              </thead>
              <tbody>
                {RTL_TESTS.map((test) => (
                  <tr key={test.block}>
                    <td><code>{test.block}</code></td>
                    <td><code>{test.testbench}</code></td>
                    <td>{test.covers}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>

        <section className="doc-section">
          <h2>Canonical runner targets</h2>
          <div className="doc-table-wrap">
            <table className="doc-table">
              <caption>python tools/run.py &lt;target&gt;</caption>
              <thead>
                <tr>
                  <th scope="col">Target</th>
                  <th scope="col">Validates</th>
                  <th scope="col">Requires</th>
                  <th scope="col">Report</th>
                  <th scope="col">Evidence</th>
                </tr>
              </thead>
              <tbody>
                {RUNNER_TARGETS.map((target) => (
                  <tr key={target.target}>
                    <td><code>{target.target}</code></td>
                    <td>{target.validates}</td>
                    <td>{target.requires}</td>
                    <td><code>{target.report}</code></td>
                    <td>
                      <StatusBadge status={target.status} label={target.status.replace(/-/g, ' ')} />
                      <p className="doc-scope" style={{ marginTop: 6 }}>{target.evidence}</p>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <p className="doc-scope">
            Reports are written under ignored <code className="doc-inline-code">build/reports</code>. Their
            absence on another checkout means not-run, not passed. Hardware and licensed-tool targets
            remain [HUMAN] gates.
          </p>
        </section>
      </div>
    </main>
  );
}
