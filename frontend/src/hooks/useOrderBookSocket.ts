import { useState, useEffect, useRef } from 'react';

export interface PriceLevel {
  price: number;
  qty: number;
  orders: number;
}

export interface SnapshotMessage {
  type: 'snapshot';
  timestamp: number;
  bids: PriceLevel[];
  asks: PriceLevel[];
}

export interface TradeMessage {
  type: 'trade';
  timestamp: number;
  price: number;
  qty: number;
  aggressor_side: 'BUY' | 'SELL';
}

export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'reconnecting';

export function useOrderBookSocket(url: string) {
  const [bids, setBids] = useState<PriceLevel[]>([]);
  const [asks, setAsks] = useState<PriceLevel[]>([]);
  const [trades, setTrades] = useState<TradeMessage[]>([]);
  const [status, setStatus] = useState<ConnectionStatus>('disconnected');
  
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<number | null>(null);
  const backoffRef = useRef(1000); // start with 1s

  useEffect(() => {
    let isMounted = true;
    function connect() {
      if (!isMounted) return;
      
      setStatus(prev => prev === 'disconnected' ? 'connecting' : 'reconnecting');
      const ws = new WebSocket(url);
      wsRef.current = ws;

      ws.onopen = () => {
        if (!isMounted) return;
        setStatus('connected');
        backoffRef.current = 1000; // reset backoff
      };

      ws.onmessage = (event) => {
        if (!isMounted) return;
        try {
          const data = JSON.parse(event.data);
          if (data.type === 'snapshot') {
            const snap = data as SnapshotMessage;
            setBids(snap.bids);
            setAsks(snap.asks);
          } else if (data.type === 'trade') {
            const trade = data as TradeMessage;
            setTrades(prev => {
              const newTrades = [trade, ...prev];
              if (newTrades.length > 50) newTrades.length = 50;
              return newTrades;
            });
          }
        } catch (e) {
          console.error("Failed to parse WS message", e);
        }
      };

      ws.onclose = () => {
        if (!isMounted) return;
        setStatus('disconnected');
        // Exponential backoff reconnect
        reconnectTimeoutRef.current = window.setTimeout(() => {
          connect();
        }, backoffRef.current);
        
        backoffRef.current = Math.min(backoffRef.current * 1.5, 10000); // Cap at 10s for snappy feeling
      };

      ws.onerror = () => {
        // ws.onclose will handle the reconnect
        ws.close(); 
      };
    }

    connect();

    return () => {
      isMounted = false;
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
      if (wsRef.current) {
        wsRef.current.onclose = null; // prevent reconnect on unmount
        wsRef.current.close();
      }
    };
  }, [url]);

  return { bids, asks, trades, status };
}
