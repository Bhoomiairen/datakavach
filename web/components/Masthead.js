"use client";
import Link from "next/link";
import { usePathname } from "next/navigation";

const TABS = [
  { href: "/", label: "Playground" },
  { href: "/scan", label: "Scan files" },
  { href: "/gateway", label: "AI gateway" },
  { href: "/benchmarks", label: "Benchmarks" },
];

export function Mark({ className = "brand-mark" }) {
  // A shield (kavach) cut from a document, with a folded corner and a hidden line of text.
  return (
    <svg className={className} viewBox="0 0 38 42" aria-hidden="true">
      <path d="M19 2.5 34.5 7.8c.4 10.9-2.2 23.4-15.5 31.7C5.7 31.2 3.1 18.7 3.5 7.8Z" fill="#1d3557" />
      <path d="M26.5 5.2 34.5 7.8c0 2.2-.1 4.3-.4 6.4l-7.6-2.2Z" fill="#f4efe4" opacity=".9" />
      <path d="M10 17.5h15.5" stroke="#f6c453" strokeWidth="4.2" strokeLinecap="round" />
      <path d="M10 25h10" stroke="#f4efe4" strokeWidth="2" strokeLinecap="round" opacity=".75" />
      <path d="M10 29.8h7" stroke="#f4efe4" strokeWidth="2" strokeLinecap="round" opacity=".5" />
    </svg>
  );
}

export default function Masthead() {
  const path = usePathname();
  return (
    <header className="masthead">
      <div className="masthead-inner">
        <Link href="/" className="brand">
          <Mark />
          <span>
            <span className="brand-name">
              Data<span>Kavach</span>
            </span>
            <span className="brand-tag">personal data · found &amp; hidden</span>
          </span>
        </Link>
        <nav className="tabs" aria-label="Main">
          {TABS.map((t) => (
            <Link key={t.href} href={t.href} className="tab" aria-current={path === t.href ? "page" : undefined}>
              {t.label}
            </Link>
          ))}
        </nav>
      </div>
    </header>
  );
}
