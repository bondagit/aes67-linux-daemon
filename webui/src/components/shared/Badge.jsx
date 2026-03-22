import React from 'react';

const variants = {
  success: { color: 'var(--route-on)', background: 'rgba(0, 212, 170, 0.1)' },
  info:    { color: 'var(--accent)', background: 'var(--accent-glow)' },
  warning: { color: '#eab308', background: 'rgba(234, 179, 8, 0.1)' },
  error:   { color: 'var(--danger)', background: 'rgba(232, 85, 85, 0.1)' },
  neutral: { color: 'var(--text-secondary)', background: 'var(--bg-hover)' },
};

export default function Badge({ children, variant = 'neutral' }) {
  const colors = variants[variant] || variants.neutral;

  return (
    <span
      style={{
        display: 'inline-block',
        padding: '2px 8px',
        borderRadius: '4px',
        fontSize: '10px',
        fontWeight: 600,
        letterSpacing: '0.03em',
        textTransform: 'uppercase',
        lineHeight: '16px',
        whiteSpace: 'nowrap',
        ...colors,
      }}
    >
      {children}
    </span>
  );
}
