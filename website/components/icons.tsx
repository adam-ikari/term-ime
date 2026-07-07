import React from 'react';

type IconProps = { className?: string; size?: number };

/**
 * Minimal inline SVG icon set. All use `currentColor` so they inherit the
 * accent color from their parent. No external assets.
 */

function svg(path: React.ReactNode, { size = 20, className = '' }: IconProps) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      className={className}
      aria-hidden="true"
    >
      {path}
    </svg>
  );
}

export const IconTerminal = (p: IconProps) =>
  svg(
    <>
      <rect x="3" y="4" width="18" height="16" rx="2" />
      <path d="M7 9l3 3-3 3M13 15h4" />
    </>,
    p
  );

export const IconGlobe = (p: IconProps) =>
  svg(
    <>
      <circle cx="12" cy="12" r="9" />
      <path d="M3 12h18M12 3a14 14 0 010 18M12 3a14 14 0 000 18" />
    </>,
    p
  );

export const IconPuzzle = (p: IconProps) =>
  svg(
    <path d="M10 3h4v3h3v4h-3v2h-4v-2H7V6h3zM5 14h14v6a1 1 0 01-1 1H6a1 1 0 01-1-1z" />,
    p
  );

export const IconBolt = (p: IconProps) =>
  svg(<path d="M13 2L4 14h7l-1 8 9-12h-7z" />, p);

export const IconBrain = (p: IconProps) =>
  svg(
    <>
      <path d="M9 4a3 3 0 00-3 3 3 3 0 00-2 5 3 3 0 001 5 3 3 0 005 1V4z" />
      <path d="M15 4a3 3 0 013 3 3 3 0 012 5 3 3 0 01-1 5 3 3 0 01-5 1V4z" />
    </>,
    p
  );

export const IconChar = (p: IconProps) =>
  svg(
    <>
      <path d="M4 7V5h16v2M9 5v14M7 19h4" />
      <path d="M14 19l3-10 3 10M15 16h4" opacity="0.6" />
    </>,
    p
  );

export const IconUi = (p: IconProps) =>
  svg(
    <>
      <rect x="3" y="3" width="18" height="18" rx="2" />
      <path d="M3 9h18M9 9v12" />
    </>,
    p
  );

export const IconGitHub = (p: IconProps) =>
  svg(
    <path
      fill="currentColor"
      stroke="none"
      d="M12 2C6.5 2 2 6.6 2 12.3c0 4.5 2.9 8.3 6.8 9.7.5.1.7-.2.7-.5v-1.7c-2.8.6-3.4-1.4-3.4-1.4-.5-1.2-1.1-1.5-1.1-1.5-.9-.6.1-.6.1-.6 1 .1 1.5 1 1.5 1 .9 1.6 2.4 1.1 3 .1 0-.7.4-1.1.7-1.4-2.2-.3-4.6-1.1-4.6-5 0-1.1.4-2 1-2.7-.1-.3-.5-1.3.1-2.7 0 0 .8-.3 2.7 1a9.4 9.4 0 015 0c1.9-1.3 2.7-1 2.7-1 .6 1.4.2 2.4.1 2.7.6.7 1 1.6 1 2.7 0 3.9-2.4 4.7-4.6 5 .4.3.7.9.7 1.9v2.8c0 .3.2.6.7.5A10.3 10.3 0 0022 12.3C22 6.6 17.5 2 12 2z"
    />,
    p
  );
