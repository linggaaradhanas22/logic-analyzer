'use client';
/* oxlint-disable next/no-html-link-for-pages -- Vinext client Link navigation fails in production. */

import { usePathname } from 'next/navigation';
import { Binary, Menu } from 'lucide-react';
import { Sheet, SheetContent, SheetHeader, SheetTitle, SheetTrigger } from '@/components/ui/sheet';

const LINKS = [
  { label: 'Overview', href: '/' },
  { label: 'Using', href: '/using' },
  { label: 'Subsystems', href: '/#subsystems' },
  { label: 'Reference', href: '/reference/registers' },
  { label: 'Verification', href: '/verification' },
  { label: 'Roadmap', href: '/roadmap' },
  { label: 'Tooling', href: '/tooling' },
  { label: 'Glossary', href: '/glossary' },
];

function isActive(pathname: string, href: string): boolean {
  if (href === '/') return pathname === '/';
  const base = href.split('#')[0];
  return base !== '/' && pathname.startsWith(base);
}

export function SiteHeader() {
  const pathname = usePathname();
  return (
    <header className="topbar doc-topbar">
      <a className="brand" href="/" aria-label="RTOS logic analyzer documentation home">
        <span className="brand-mark">
          <Binary aria-hidden="true" />
        </span>
        <span>
          LA<span className="brand-dim">/RTOS</span>
        </span>
      </a>
      <nav className="nav-links" aria-label="Documentation">
        {LINKS.map((link) => (
          <a key={link.href} href={link.href} aria-current={isActive(pathname, link.href) ? 'page' : undefined}>
            {link.label}
          </a>
        ))}
      </nav>
      <div className="doc-header-actions">
        <span className="build-tag">
          <span /> Native Windows build
        </span>
        <Sheet>
          <SheetTrigger className="doc-menu-button" aria-label="Open documentation navigation">
            <Menu aria-hidden="true" />
          </SheetTrigger>
          <SheetContent side="right">
            <SheetHeader>
              <SheetTitle>Documentation</SheetTitle>
            </SheetHeader>
            <nav className="doc-menu-list" aria-label="Documentation">
              {LINKS.map((link) => (
                <a key={link.href} href={link.href}>
                  {link.label}
                </a>
              ))}
            </nav>
          </SheetContent>
        </Sheet>
      </div>
    </header>
  );
}
