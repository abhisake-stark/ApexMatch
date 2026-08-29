import React, { useEffect, useRef } from 'react';
import { createChart, AreaSeries } from 'lightweight-charts';
import type { IChartApi, ISeriesApi } from 'lightweight-charts';
import type { PriceLevel } from '../hooks/useOrderBookSocket';

interface MarketDepthChartProps {
  bids: PriceLevel[];
  asks: PriceLevel[];
}

export const MarketDepthChart: React.FC<MarketDepthChartProps> = ({ bids, asks }) => {
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const bidSeriesRef = useRef<ISeriesApi<"Area"> | null>(null);
  const askSeriesRef = useRef<ISeriesApi<"Area"> | null>(null);

  useEffect(() => {
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      layout: {
        background: { color: 'transparent' },
        textColor: '#94a3b8',
      },
      grid: {
        vertLines: { visible: false },
        horzLines: { visible: false },
      },
      timeScale: {
        visible: true,
        timeVisible: true,
        secondsVisible: false,
        tickMarkFormatter: (time: number) => {
          // X-axis is actually the Price
          return time.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
        },
        borderColor: '#1e293b',
      },
      rightPriceScale: {
        visible: true,
        borderColor: '#1e293b',
      },
      crosshair: {
        mode: 1, // Normal crosshair
        vertLine: {
          labelBackgroundColor: '#1e293b',
          color: '#1e293b',
        },
        horzLine: {
          labelBackgroundColor: '#1e293b',
          color: '#1e293b',
        }
      }
    });

    // Time scale is normally used for dates, but for a depth chart we use prices as 'time'
    // lightweight-charts allows numeric timestamps, so we can use price directly if we cast appropriately,
    // though the typing prefers string or standard timestamp. We'll use Price as the "time" value (Unix-like).

    const bidSeries = chart.addSeries(AreaSeries, {
      lineColor: '#089981',
      topColor: 'rgba(8, 153, 129, 0.4)',
      bottomColor: 'rgba(8, 153, 129, 0.0)',
      lineWidth: 2,
      priceFormat: {
        type: 'volume',
      },
      lineType: 1, // StepLine
    });

    const askSeries = chart.addSeries(AreaSeries, {
      lineColor: '#f23645',
      topColor: 'rgba(242, 54, 69, 0.4)',
      bottomColor: 'rgba(242, 54, 69, 0.0)',
      lineWidth: 2,
      priceFormat: {
        type: 'volume',
      },
      lineType: 1, // StepLine
    });

    chartRef.current = chart;
    bidSeriesRef.current = bidSeries;
    askSeriesRef.current = askSeries;

    const handleResize = () => {
      if (chartContainerRef.current) {
        chart.applyOptions({
          width: chartContainerRef.current.clientWidth,
          height: chartContainerRef.current.clientHeight,
        });
      }
    };

    window.addEventListener('resize', handleResize);
    
    // Initial size
    handleResize();

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
    };
  }, []);

  useEffect(() => {
    if (!bidSeriesRef.current || !askSeriesRef.current || !chartRef.current) return;

    // Build cumulative bids
    
    // Calculate correct cumulative from best bid outwards
    const bidAccumulated = bids.map(b => b); // Copy
    let acc = 0;
    const finalBidData = bidAccumulated.map(b => {
      acc += b.qty;
      return { time: b.price as any, value: acc };
    }).reverse(); // Reverse to have time (price) ascending for lightweight-charts

    // Build cumulative asks
    let cumAskQty = 0;
    // Asks are sorted ascending (lowest first, e.g. 10001, 10002).
    const finalAskData = asks.map(a => {
      cumAskQty += a.qty;
      return { time: a.price as any, value: cumAskQty };
    });

    bidSeriesRef.current.setData(finalBidData);
    askSeriesRef.current.setData(finalAskData);
    
    // Fit content
    if (finalBidData.length > 0 || finalAskData.length > 0) {
        chartRef.current.timeScale().fitContent();
    }
  }, [bids, asks]);

  return <div ref={chartContainerRef} style={{ width: '100%', height: '100%' }} />;
};
