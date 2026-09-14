'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { Binary, Menu } from 'lucide-react';
import { Sheet, SheetContent, SheetHeader, SheetTitle, SheetTrigger } from '@/components/ui/sheet';

const LINKS = [
  { label: 'Overview', href: '/' },
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
      <Link className="brand" href="/" aria-label="RTOS logic analyzer documentation home">
        <span className="brand-mark">
          <Binary aria-hidden="true" />
        </span>
        <span>
          LA<span className="brand-dim">/RTOS</span>
        </span>
      </Link>
      <nav className="nav-links" aria-label="Documentation">
        {LINKS.map((link) => (
          <Link key={link.href} href={link.href} aria-current={isActive(pathname, link.href) ? 'page' : undefined}>
            {link.label}
          </Link>
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
                <Link key={link.href} href={link.href}>
                  {link.label}
                </Link>
              ))}
            </nav>
          </SheetContent>
        </Sheet>
      </div>
    </header>
  );
}
