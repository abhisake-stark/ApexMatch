import React from 'react';
import type { TradeMessage } from '../hooks/useOrderBookSocket';

interface RecentTradesTapeProps {
  trades: TradeMessage[];
}

export const RecentTradesTape: React.FC<RecentTradesTapeProps> = ({ trades }) => {
  // Format price helper (always 2 decimals)
  const formatPrice = (p: number) => p.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', fontFamily: 'var(--font-mono)' }}>
      <div style={{ display: 'flex', paddingBottom: '4px', marginBottom: '8px', color: 'var(--text-muted)', fontSize: '0.75rem', textTransform: 'uppercase' }}>
        <div style={{ flex: 1 }}>Time</div>
        <div style={{ flex: 1, textAlign: 'right' }}>Price</div>
        <div style={{ flex: 1, textAlign: 'right' }}>Size</div>
      </div>
      
      <div className="scrollable-content" style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
        {trades.map((trade, idx) => {
          const date = new Date(trade.timestamp);
          const timeStr = `${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}:${date.getSeconds().toString().padStart(2, '0')}`;
          const color = trade.aggressor_side === 'BUY' ? 'var(--bid-color)' : 'var(--ask-color)';
          
          return (
            <div key={`${trade.timestamp}-${idx}`} style={{ display: 'flex', padding: '2px 0', color, fontSize: '0.85rem' }}>
              <div style={{ flex: 1, color: 'var(--text-muted)' }}>{timeStr}</div>
              <div style={{ flex: 1, textAlign: 'right' }}>{formatPrice(trade.price)}</div>
              <div style={{ flex: 1, textAlign: 'right', opacity: 0.8 }}>{trade.qty.toLocaleString()}</div>
            </div>
          );
        })}
        {trades.length === 0 && (
          <div style={{ textAlign: 'center', padding: '20px', color: 'var(--text-muted)', fontSize: '0.85rem' }}>
            Awaiting trades...
          </div>
        )}
      </div>
    </div>
  );
};
