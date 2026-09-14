import type { ComponentDoc, SubsystemSlug } from './types';
import { physicalIoComponents } from './components/physical-io';
import { fpgaCaptureComponents } from './components/fpga-capture';
import { dmaMemoryComponents } from './components/dma-memory';
import { zynqRtosComponents } from './components/zynq-rtos';
import { protocolLinkComponents } from './components/protocol-link';
import { esp32GatewayComponents } from './components/esp32-gateway';
import { hostToolsComponents } from './components/host-tools';

export const COMPONENTS: ComponentDoc[] = [
  ...physicalIoComponents,
  ...fpgaCaptureComponents,
  ...dmaMemoryComponents,
  ...zynqRtosComponents,
  ...protocolLinkComponents,
  ...esp32GatewayComponents,
  ...hostToolsComponents,
];

export function componentBySlug(slug: string): ComponentDoc | undefined {
  return COMPONENTS.find((component) => component.slug === slug);
}

export function componentsForSubsystem(slug: SubsystemSlug): ComponentDoc[] {
  return COMPONENTS.filter((component) => component.subsystem === slug);
}
