import { Binary } from 'lucide-react';
import { SNAPSHOT } from '@/lib/docs/snapshot';

export function SiteFooter() {
  return (
    <footer className="footer">
      <div className="section-shell footer-inner">
        <div>
          <span className="brand footer-brand">
            <span className="brand-mark">
              <Binary aria-hidden="true" />
            </span>
            LA<span className="brand-dim">/RTOS</span>
          </span>
          <p>Zybo Z7-10 · Zynq-7010 · ESP32 · FreeRTOS · native Windows</p>
        </div>
        <p className="footer-note">
          Evidence snapshot reviewed {SNAPSHOT.reviewedOn}; newest bench
          evidence {SNAPSHOT.benchOn}.
          <br />
          Sources pinned at{' '}
          <a
            className="inline-source"
            href={SNAPSHOT.sourceTree}
            target="_blank"
            rel="noreferrer"
          >
            {SNAPSHOT.sourceSha}
          </a>
          . The USB-first class project is complete. Targets are not presented
          as measurements.
        </p>
      </div>
    </footer>
  );
}
