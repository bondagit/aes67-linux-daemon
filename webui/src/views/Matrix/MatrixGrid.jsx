import React, { useCallback, useRef } from 'react';
import CrosspointNode from './CrosspointNode';

export default function MatrixGrid({ inputs, outputs, routes, onToggle }) {
  const tableRef = useRef(null);
  const highlightedCol = useRef(-1);

  // Flatten outputs into columns: [{groupId, groupName, ch, colIndex}]
  const outCols = [];
  outputs.forEach((out) => {
    (out.channels || []).forEach((ch) => {
      outCols.push({
        groupId: out.id,
        groupName: out.name,
        ch,
      });
    });
  });

  // Flatten inputs into rows grouped by source
  const inGroups = inputs.map((inp) => ({
    id: inp.id,
    name: inp.name,
    channels: inp.channels || [],
  }));

  const totalOutCols = outCols.length;

  const handleMouseOver = useCallback((e) => {
    const td = e.target.closest('td, th');
    if (!td) return;
    const colIdx = td.cellIndex;
    if (colIdx === highlightedCol.current) return;

    const table = tableRef.current;
    if (!table) return;

    // Remove old highlight
    if (highlightedCol.current >= 0) {
      const old = table.querySelectorAll('.col-hl');
      old.forEach((el) => el.classList.remove('col-hl'));
    }

    // Add new highlight (skip label column 0)
    if (colIdx > 0) {
      const rows = table.rows;
      for (let i = 0; i < rows.length; i++) {
        const cell = rows[i].cells[colIdx];
        if (cell) cell.classList.add('col-hl');
      }
    }
    highlightedCol.current = colIdx;
  }, []);

  const handleMouseLeave = useCallback(() => {
    const table = tableRef.current;
    if (!table) return;
    const old = table.querySelectorAll('.col-hl');
    old.forEach((el) => el.classList.remove('col-hl'));
    highlightedCol.current = -1;
  }, []);

  // Build output group headers with colspan
  const outGroupHeaders = [];
  let i = 0;
  while (i < outCols.length) {
    const gid = outCols[i].groupId;
    let span = 0;
    while (i + span < outCols.length && outCols[i + span].groupId === gid) span++;
    outGroupHeaders.push({ id: gid, name: outCols[i].groupName, span });
    i += span;
  }

  return (
    <table
      className="matrix-table"
      ref={tableRef}
      onMouseOver={handleMouseOver}
      onMouseLeave={handleMouseLeave}
    >
      <thead>
        {/* Row 1: corner + output group headers */}
        <tr>
          <th className="matrix-corner" rowSpan={2}>
            <div className="corner-labels">
              <span className="corner-out">OUTPUTS &rarr;</span>
              <span className="corner-in">&darr; INPUTS</span>
            </div>
          </th>
          {outGroupHeaders.map((g) => (
            <th key={g.id} className="out-group" colSpan={g.span}>
              {g.name}
            </th>
          ))}
        </tr>
        {/* Row 2: output channel headers */}
        <tr>
          {outCols.map((col, ci) => (
            <th key={`${col.groupId}-${col.ch.id}`} className="out-ch">
              {col.ch.label || col.ch.id}
            </th>
          ))}
        </tr>
      </thead>
      <tbody>
        {inGroups.map((group, gi) => {
          const altClass = gi % 2 === 1 ? ' grp-alt' : '';
          return (
            <React.Fragment key={group.id}>
              {/* Group header row */}
              <tr className={`in-group-row${altClass}`}>
                <td className="in-group-label" colSpan={totalOutCols + 1}>
                  {group.name}
                </td>
              </tr>
              {/* Channel rows */}
              {group.channels.map((ch) => (
                <tr key={`${group.id}-${ch.id}`} className={`in-ch-row${altClass}`}>
                  <td className="in-label">{ch.label || ch.id}</td>
                  {outCols.map((col) => {
                    const key = `${group.id}:${ch.id}-${col.groupId}:${col.ch.id}`;
                    const active = routes.has(key);
                    return (
                      <td key={key} className="xp-cell">
                        <CrosspointNode
                          active={active}
                          onToggle={() =>
                            onToggle(
                              { id: group.id, ch: ch.id },
                              { id: col.groupId, ch: col.ch.id }
                            )
                          }
                        />
                      </td>
                    );
                  })}
                </tr>
              ))}
            </React.Fragment>
          );
        })}
      </tbody>
    </table>
  );
}
