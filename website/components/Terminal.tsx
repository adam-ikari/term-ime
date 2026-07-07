import React from 'react';

type TerminalProps = {
  title?: string;
  children: React.ReactNode;
  className?: string;
  showDots?: boolean;
};

/**
 * Reusable fake-terminal window chrome: a title bar with three traffic-light
 * dots and a centered title, wrapping a monospace body that preserves
 * whitespace. Styling lives in app/globals.css (.terminal*).
 */
export default function Terminal({
  title = 'term-ime',
  children,
  className = '',
  showDots = true,
}: TerminalProps) {
  return (
    <div className={`terminal ${className}`}>
      <div className="terminal-bar">
        {showDots && (
          <span className="dots" aria-hidden="true">
            <span className="dot dot-red" />
            <span className="dot dot-yellow" />
            <span className="dot dot-green" />
          </span>
        )}
        <span className="terminal-title">{title}</span>
      </div>
      <div className="terminal-body">{children}</div>
    </div>
  );
}
