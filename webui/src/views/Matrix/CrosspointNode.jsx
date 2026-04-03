import React from 'react';

const CrosspointNode = React.memo(function CrosspointNode({ active, onToggle }) {
  const handleKeyDown = (e) => {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      onToggle();
    }
  };

  return (
    <div
      className={`xp-node${active ? ' active' : ''}`}
      role="button"
      tabIndex={0}
      aria-pressed={active}
      onClick={onToggle}
      onKeyDown={handleKeyDown}
    />
  );
});

export default CrosspointNode;
