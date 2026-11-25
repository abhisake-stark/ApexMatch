import { useOrderBookSocket } from './hooks/useOrderBookSocket';
import { OrderBookTable } from './components/OrderBookTable';
import { RecentTradesTape } from './components/RecentTradesTape';
import { MarketDepthChart } from './components/MarketDepthChart';
import './index.css';

function App() {
  const wsUrl = import.meta.env.VITE_WS_URL || 'ws://127.0.0.1:9002';
  const { bids, asks, trades, status } = useOrderBookSocket(wsUrl);

  return (
    <div className="dashboard-container">
      <div className="header">
        <h2>ApexMatch Dashboard</h2>
        <div className="connection-indicator">
          <div className={`dot ${status}`}></div>
          <span style={{ textTransform: 'capitalize' }}>{status}</span>
        </div>
      </div>
      
      <div className="main-content">
        <div className="panel">
          <div className="panel-header">Market Depth</div>
          <div style={{ flex: 1, position: 'relative' }}>
            <MarketDepthChart bids={bids} asks={asks} />
          </div>
        </div>
        
        <div className="panel">
          <div className="panel-header">Order Book</div>
          <OrderBookTable bids={bids} asks={asks} />
        </div>
      </div>
      
      <div className="panel" style={{ gridRow: '2 / 3', gridColumn: '2 / 3' }}>
        <div className="panel-header">Recent Trades</div>
        <RecentTradesTape trades={trades} />
      </div>
    </div>
  );
}

export default App;
