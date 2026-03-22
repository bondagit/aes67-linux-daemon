import React from 'react';

const defaultStyle = {
  background: 'var(--bg-surface)',
  border: '1px solid var(--border)',
};

const successStyle = {
  background: 'linear-gradient(135deg, #0c1a0c, #0f1f0f)',
  border: '1px solid #1a3a1a',
};

export default function StatusCard({ title, value, subtitle, details = [], variant = 'default', indicator, children }) {
  const isSuccess = variant === 'success';
  const cardStyle = isSuccess ? successStyle : defaultStyle;

  return (
    <div className="status-card" style={cardStyle}>
      <div className="status-card__header">
        <span className="status-card__title">{title}</span>
        {indicator && (
          <span
            className="status-card__indicator"
            style={{
              display: 'inline-block',
              width: indicator.size || 8,
              height: indicator.size || 8,
              borderRadius: '50%',
              background: indicator.color || 'var(--route-on)',
              boxShadow: indicator.glow ? `0 0 6px ${indicator.glow}` : 'none',
              animation: indicator.pulse ? 'pulse 2s ease-in-out infinite' : 'none',
            }}
          />
        )}
      </div>
      <div className="status-card__value">{value}</div>
      {subtitle && <div className="status-card__subtitle">{subtitle}</div>}
      {details.length > 0 && (
        <>
          <div className="status-card__divider" />
          <div className="status-card__details">
            {details.map((d, i) => (
              <div key={i} className="status-card__detail">
                <span className="status-card__detail-label">{d.label}</span>
                <span className="status-card__detail-value">{d.value}</span>
              </div>
            ))}
          </div>
        </>
      )}
      {children}
    </div>
  );
}
