import { TYPES, typeLabel } from "@/lib/types";

export default function TypeBadge({ type }) {
  return (
    <span className="badge" style={{ "--c": TYPES[type]?.color ?? "#ddd" }}>
      {typeLabel(type)}
    </span>
  );
}

export function Score({ value }) {
  return (
    <span className="score" title="confidence">
      <span className="score-bar">
        <i style={{ width: `${Math.round(value * 100)}%` }} />
      </span>
      {value.toFixed(2)}
    </span>
  );
}
