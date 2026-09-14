import type { InterfaceItem } from '@/lib/docs/types';

const DIRECTION_LABELS: Record<InterfaceItem['direction'], string> = {
  input: 'input',
  output: 'output',
  bidirectional: 'bidirectional',
  internal: 'internal',
};

export function InterfaceTable({ caption, items }: { caption: string; items: InterfaceItem[] }) {
  return (
    <div className="doc-table-wrap">
      <table className="doc-table">
        <caption>{caption}</caption>
        <thead>
          <tr>
            <th scope="col">Name</th>
            <th scope="col">Direction</th>
            <th scope="col">Form</th>
            <th scope="col">Description</th>
          </tr>
        </thead>
        <tbody>
          {items.map((item) => (
            <tr key={item.name}>
              <td>
                <code>{item.name}</code>
              </td>
              <td className="num">{DIRECTION_LABELS[item.direction]}</td>
              <td>{item.form}</td>
              <td>{item.description}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
