'use client';

import { useState } from 'react';
import Link from 'next/link';
import { StatusBadge } from '@/components/docs/status-badge';
import type { EvidenceStatus, SubsystemSlug } from '@/lib/docs/types';

type SystemNode = {
  slug: SubsystemSlug;
  order: string;
  name: string;
  role: string;
  status: EvidenceStatus;
  statusNote: string;
};

const NODES: SystemNode[] = [
  { slug: 'physical-io', order: '01', name: 'Signals and physical I/O', role: 'Eight 3.3 V inputs, pattern outputs and board wiring.', status: 'bench-verified', statusNote: 'JE→JC loopback 18/18' },
  { slug: 'fpga-capture', order: '02', name: 'FPGA capture pipeline', role: 'Sample, trigger, keep history, encode and packetize at fabric speed.', status: 'bench-verified', statusNote: 'Exact captures at 1/10/100 MS/s' },
  { slug: 'dma-memory', order: '03', name: 'AXI DMA and DDR', role: 'Move complete packets into bounded DDR buffers with ordinary SG descriptors.', status: 'bench-verified', statusNote: 'Multi-BD and live segments' },
  { slug: 'zynq-rtos', order: '04', name: 'Zynq FreeRTOS control', role: 'Coordinate acquisition, commands, transmission, status and ownership.', status: 'bench-verified', statusNote: 'Burst and live paths' },
  { slug: 'protocol-link', order: '05', name: 'Framed UART + SPI link', role: 'CRC-protected frames for commands, events and capture chunks.', status: 'blocked', statusNote: 'SPI drain gate waits on the S3 port' },
  { slug: 'esp32-gateway', order: '06', name: 'ESP32 wireless gateway', role: 'Relay to one Wi-Fi TCP client and serve the live web view.', status: 'known-issue', statusNote: 'Wi-Fi drain measured at ~0.28 MB/s' },
  { slug: 'host-tools', order: '07', name: 'Host tools and capture files', role: 'Configure, validate, reconstruct and export captures; stream live events.', status: 'host-tested', statusNote: 'Burst and live used on bench' },
];

const FLOWS = [
  {
    id: 'capture',
    label: 'Capture data',
    order: ['physical-io', 'fpga-capture', 'dma-memory', 'zynq-rtos', 'protocol-link', 'esp32-gateway', 'host-tools'],
    steps: [
      'A signal arrives on a JC pin and is synchronized into the 100 MHz fabric clock domain.',
      'The FPGA samples, qualifies the trigger, keeps pre-trigger history and packs the samples.',
      'The AXI DMA writes complete packets into the reserved DDR capture region.',
      'AcqTask owns the finished capture, then hands it to TxTask as a job.',
      'TxTask frames the capture and sends chunks over the UART (or the SPI bulk link).',
      'The ESP32 relays frames to the single Wi-Fi TCP client.',
      'The host verifies the CRC32, reconstructs samples and writes .sr, VCD or PNG.',
    ],
    note: 'This is the burst path. Live decode shares the first four stages but decodes on the Zynq instead of transferring raw samples.',
  },
  {
    id: 'commands',
    label: 'Commands',
    order: ['host-tools', 'esp32-gateway', 'protocol-link', 'zynq-rtos', 'fpga-capture'],
    steps: [
      'The host builds a typed frame (CONFIG, ARM, ABORT, live setup) with a sequence number.',
      'The ESP32 forwards it verbatim to the UART; the host waits for the matching ACK.',
      'The Zynq parser validates the CRC and dispatches by message type.',
      'CmdTask latches the configuration and AcqTask programs the PL registers and DMA.',
      'The FPGA starts sampling on the next ARM and reports trigger/completion through the interrupt fan-in.',
    ],
    note: 'Commands always use the flow-controlled UART so a lost frame is retried rather than silently missed.',
  },
  {
    id: 'status',
    label: 'Status/errors',
    order: ['fpga-capture', 'dma-memory', 'zynq-rtos', 'protocol-link', 'esp32-gateway', 'host-tools'],
    steps: [
      'The PL raises DONE, TRIGGERED or OVERFLOW; the DMA raises completion and error interrupts.',
      'ISRs acknowledge the hardware and notify tasks; they never process payloads.',
      'Tasks turn the events into STATUS, EVT_TRIGGERED, LOG and overrun frames.',
      'The framed report crosses the link and the ESP32 relay.',
      'The host surfaces the state, and a stage-2 overrun tells it exactly where frames were lost.',
    ],
    note: 'Loss is visible by design: the system reports where it dropped data instead of presenting a corrupted stream.',
  },
];

export function SystemDiagram() {
  const [flowId, setFlowId] = useState('capture');
  const flow = FLOWS.find((item) => item.id === flowId) ?? FLOWS[0];
  const position = new Map(flow.order.map((slug, index) => [slug, index]));

  return (
    <div>
      <fieldset className="doc-flow-picker">
        <legend className="sr-only">Select a system flow</legend>
        {FLOWS.map((item) => (
          <button
            type="button"
            key={item.id}
            aria-pressed={item.id === flowId}
            onClick={() => setFlowId(item.id)}
          >
            {item.label}
          </button>
        ))}
      </fieldset>
      <ul className="doc-system-diagram" aria-label="Level 1 system architecture">
        {NODES.map((node) => {
          const index = position.get(node.slug);
          const active = index !== undefined;
          const from = index !== undefined && index > 0 ? NODES.find((item) => item.slug === flow.order[index - 1]) : undefined;
          const to = index !== undefined && index < flow.order.length - 1 ? NODES.find((item) => item.slug === flow.order[index + 1]) : undefined;
          return (
            <li key={node.slug} style={{ display: 'contents' }}>
              <Link
                className="doc-system-node"
                data-active={active ? 'true' : undefined}
                data-dim={active ? undefined : 'true'}
                href={`/subsystems/${node.slug}`}
              >
                <span className="order">{node.order}</span>
                {index !== undefined ? <span className="doc-system-step">Step {index + 1}</span> : null}
                <strong>{node.name}</strong>
                <p>{node.role}</p>
                <StatusBadge status={node.status} />
                <p className="doc-system-note">{node.statusNote}</p>
                {active ? (
                  <p className="doc-system-route">
                    {from ? `receives from ${from.name}` : 'starts the flow'}
                    <br />
                    {to ? `sends to ${to.name}` : 'ends the flow'}
                  </p>
                ) : (
                  <p className="doc-system-route">not part of the “{flow.label}” flow</p>
                )}
              </Link>
            </li>
          );
        })}
      </ul>
      <div className="doc-system-legend">
        <span>Step badge: position in the selected flow.</span>
        <span>Dimmed card: not on the selected path.</span>
        <span>Badge: evidence status with its scope below.</span>
        <span>Warning color: known issue or blocked.</span>
      </div>
      <div className="doc-system-explainer" aria-live="polite">
        <h3>{flow.label} — numbered path</h3>
        <ol>
          {flow.steps.map((step) => (
            <li key={step}>{step}</li>
          ))}
        </ol>
        <p className="doc-scope">{flow.note}</p>
      </div>
    </div>
  );
}
