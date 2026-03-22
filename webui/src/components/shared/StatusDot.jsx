import React from 'react';

const statusStyles = {
  active: {
    background: 'var(--route-on)',
    boxShadow: '0 0 6px var(--route-glow)',
  },
  error: {
    background: 'var(--danger)',
    boxShadow: '0 0 6px var(--danger-glow)',
  },
  warning: {
    background: '#eab308',
    boxShadow: '0 0 6px rgba(234, 179, 8, 0.3)',
  },
  inactive: {
    background: 'var(--text-dim)',
    boxShadow: 'none',
  },
};

export default function StatusDot({ status = 'inactive', size = 8, pulse = false }) {
  const colors = statusStyles[status] || statusStyles.inactive;

  return (
    <span
      style={{
        display: 'inline-block',
        width: size,
        height: size,
        borderRadius: '50%',
        flexShrink: 0,
        ...colors,
        animation: pulse && status !== 'inactive' ? 'pulse 2s ease-in-out infinite' : 'none',
      }}
    />
  );
}
