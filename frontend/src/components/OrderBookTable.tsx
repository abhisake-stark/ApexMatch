import React from 'react';
import type { PriceLevel } from '../hooks/useOrderBookSocket';

interface OrderBookTableProps {
  bids: PriceLevel[];
  asks: PriceLevel[];
}

export const OrderBookTable: React.FC<OrderBookTableProps> = ({ bids, asks }) => {
  const bestBid = bids.length > 0 ? bids[0].price : null;
  const bestAsk = asks.length > 0 ? asks[0].price : null;
  const spread = bestAsk !== null && bestBid !== null ? bestAsk - bestBid : null;

  // Render up to 15 levels
  const maxLevels = 20;
  const rows = [];
  
  let maxQty = 0;
  for (let i = 0; i < maxLevels; i++) {
    const bidQty = bids[i]?.qty || 0;
    const askQty = asks[i]?.qty || 0;
    maxQty = Math.max(maxQty, bidQty, askQty);
    
    rows.push({
      bid: bids[i] || null,
      ask: asks[i] || null,
    });
  }
  
  // Format price helper (always 2 decimals)
  const formatPrice = (p: number) => p.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', fontFamily: 'var(--font-mono)' }}>
      <div style={{ textAlign: 'center', marginBottom: '8px', padding: '4px', color: 'var(--text-muted)', fontSize: '0.8rem', letterSpacing: '0.05em' }}>
        SPREAD: {spread !== null ? formatPrice(spread) : '--'}
      </div>
      
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Bids Column */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', position: 'relative' }}>
          <div style={{ display: 'flex', paddingBottom: '4px', marginBottom: '8px', color: 'var(--text-muted)', fontSize: '0.75rem', textTransform: 'uppercase' }}>
            <div style={{ flex: 1 }}>Orders</div>
            <div style={{ flex: 1, textAlign: 'right' }}>Size</div>
            <div style={{ flex: 1, textAlign: 'right' }}>Price</div>
          </div>
          <div className="scrollable-content" style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
            {rows.map((row, i) => {
              const depthPct = row.bid && maxQty > 0 ? (row.bid.qty / maxQty) * 100 : 0;
              return (
                <div key={`bid-${i}`} style={{ display: 'flex', padding: '2px 0', color: 'var(--bid-color)', position: 'relative', fontSize: '0.85rem' }}>
                  {row.bid && (
                    <div style={{ position: 'absolute', right: 0, top: 0, bottom: 0, width: `${depthPct}%`, backgroundColor: 'var(--bid-bg-hover)', zIndex: 0 }} />
                  )}
                  <div style={{ flex: 1, zIndex: 1, opacity: 0.8 }}>{row.bid ? row.bid.orders : ''}</div>
                  <div style={{ flex: 1, textAlign: 'right', zIndex: 1 }}>{row.bid ? row.bid.qty.toLocaleString() : ''}</div>
                  <div style={{ flex: 1, textAlign: 'right', zIndex: 1 }}>{row.bid ? formatPrice(row.bid.price) : ''}</div>
                </div>
              );
            })}
          </div>
        </div>

        {/* Space Divider */}
        <div style={{ width: '24px' }} />

        {/* Asks Column */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', position: 'relative' }}>
          <div style={{ display: 'flex', paddingBottom: '4px', marginBottom: '8px', color: 'var(--text-muted)', fontSize: '0.75rem', textTransform: 'uppercase' }}>
            <div style={{ flex: 1 }}>Price</div>
            <div style={{ flex: 1, textAlign: 'right' }}>Size</div>
            <div style={{ flex: 1, textAlign: 'right' }}>Orders</div>
          </div>
          <div className="scrollable-content" style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
            {rows.map((row, i) => {
              const depthPct = row.ask && maxQty > 0 ? (row.ask.qty / maxQty) * 100 : 0;
              return (
                <div key={`ask-${i}`} style={{ display: 'flex', padding: '2px 0', color: 'var(--ask-color)', position: 'relative', fontSize: '0.85rem' }}>
                  {row.ask && (
                    <div style={{ position: 'absolute', left: 0, top: 0, bottom: 0, width: `${depthPct}%`, backgroundColor: 'var(--ask-bg-hover)', zIndex: 0 }} />
                  )}
                  <div style={{ flex: 1, zIndex: 1 }}>{row.ask ? formatPrice(row.ask.price) : ''}</div>
                  <div style={{ flex: 1, textAlign: 'right', zIndex: 1 }}>{row.ask ? row.ask.qty.toLocaleString() : ''}</div>
                  <div style={{ flex: 1, textAlign: 'right', zIndex: 1, opacity: 0.8 }}>{row.ask ? row.ask.orders : ''}</div>
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </div>
  );
};
