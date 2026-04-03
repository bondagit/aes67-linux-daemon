import React, { useState, useEffect, useCallback } from 'react';
import { NavLink } from 'react-router-dom';
import { api } from '../../services/api';
import { usePolling } from '../../hooks/usePolling';
import StatusDot from '../shared/StatusDot';
import './Sidebar.css';

const navItems = [
  { to: '/',         icon: '\u2B21', label: 'Dashboard' },
  { to: '/monitoring', icon: '\u2261', label: 'Monitoring' },
  { to: '/matrix',   icon: '\u229E', label: 'Matrix' },
  { to: '/sources',  icon: '\u25B6', label: 'Sources' },
  { to: '/sinks',    icon: '\u25C0', label: 'Sinks' },
  { to: '/browser',  icon: '\u25CE', label: 'Browser' },
  { to: '/settings', icon: '\u2699', label: 'Settings' },
];

function ptpStatusToLed(status) {
  if (!status) return { dotStatus: 'inactive', label: '---' };
  const gmid = status.gmid || status.status;
  if (typeof gmid === 'string') {
    const s = gmid.toLowerCase();
    if (s === 'locked' || s === 'tracking') return { dotStatus: 'active', label: 'Locked' };
    if (s === 'locking' || s === 'acquiring') return { dotStatus: 'warning', label: 'Locking' };
  }
  // If the status field is a number or other format
  if (status.jitter !== undefined && status.jitter !== null) {
    return { dotStatus: 'active', label: 'Locked' };
  }
  return { dotStatus: 'error', label: 'Unlocked' };
}

function getInitialTheme() {
  const saved = localStorage.getItem('aes67-theme');
  return saved || 'light';
}

export default function Sidebar({ collapsed, onToggle }) {
  const [ptpStatus, setPtpStatus] = useState(null);
  const [version, setVersion] = useState('');
  const [theme, setTheme] = useState(getInitialTheme);

  // Apply theme to document
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('aes67-theme', theme);
  }, [theme]);

  const toggleTheme = () => setTheme(t => t === 'dark' ? 'light' : 'dark');

  const fetchPtp = useCallback(async () => {
    try {
      const data = await api.getPTPStatus();
      setPtpStatus(data);
    } catch {
      setPtpStatus(null);
    }
  }, []);

  useEffect(() => {
    fetchPtp();
    api.getVersion().then((v) => {
      setVersion(typeof v === 'string' ? v : v?.version || '');
    }).catch(() => {});
  }, [fetchPtp]);

  usePolling(fetchPtp, 1000);

  const { dotStatus, label } = ptpStatusToLed(ptpStatus);

  return (
    <aside className={`sidebar ${collapsed ? 'sidebar--collapsed' : ''}`}>
      {/* Brand */}
      <div className="sidebar__brand" onClick={onToggle} title="Toggle sidebar">
        <div className="sidebar__logo">A67</div>
        {!collapsed && <span className="sidebar__brand-text">AES67</span>}
      </div>

      {/* Navigation */}
      <nav className="sidebar__nav">
        {navItems.map(({ to, icon, label: navLabel }) => (
          <NavLink
            key={to}
            to={to}
            end={to === '/'}
            className={({ isActive }) =>
              `sidebar__link ${isActive ? 'sidebar__link--active' : ''}`
            }
            title={collapsed ? navLabel : undefined}
          >
            <span className="sidebar__icon">{icon}</span>
            {!collapsed && <span className="sidebar__label">{navLabel}</span>}
          </NavLink>
        ))}
      </nav>

      {/* Bottom section */}
      <div className="sidebar__footer">
        <button
          className="sidebar__theme-toggle"
          onClick={toggleTheme}
          title={theme === 'dark' ? 'Switch to light mode' : 'Switch to dark mode'}
        >
          <span className="sidebar__theme-icon">{theme === 'dark' ? '☀' : '☾'}</span>
          {!collapsed && <span className="sidebar__theme-label">{theme === 'dark' ? 'Light' : 'Dark'}</span>}
        </button>
        <div className="sidebar__ptp" title={`PTP: ${label}`}>
          <StatusDot status={dotStatus} size={8} pulse={dotStatus === 'warning'} />
          {!collapsed && <span className="sidebar__ptp-label">PTP</span>}
        </div>
        {!collapsed && version && (
          <span className="sidebar__version">v{version}</span>
        )}
      </div>
    </aside>
  );
}
