import React, { useCallback, useEffect, useRef, useState } from 'react';

interface DraggableResizableProps {
  title: string;
  onClose: () => void;
  children: React.ReactNode;
  defaultWidth: number;
  defaultHeight: number;
  minWidth: number;
  minHeight: number;
}

interface DragState {
  startX: number;
  startY: number;
  origLeft: number;
  origTop: number;
}

interface ResizeState {
  startX: number;
  startY: number;
  origWidth: number;
  origHeight: number;
}

export function DraggableResizable({
  title,
  onClose,
  children,
  defaultWidth,
  defaultHeight,
  minWidth,
  minHeight,
}: DraggableResizableProps): React.ReactElement {
  const [pos, setPos] = useState(() => ({
    x: Math.max(0, (window.innerWidth - defaultWidth) / 2),
    y: Math.max(0, (window.innerHeight - defaultHeight) / 2),
  }));
  const [size, setSize] = useState({ width: defaultWidth, height: defaultHeight });
  const dragState = useRef<DragState | null>(null);
  const resizeState = useRef<ResizeState | null>(null);

  const handleMouseDown = useCallback((e: React.MouseEvent<HTMLElement>) => {
    if (e.button !== 0) {
      return;
    }
    const header = e.currentTarget;
    const rect = header.parentElement?.getBoundingClientRect();
    if (!rect) {
      return;
    }

    dragState.current = {
      startX: e.clientX,
      startY: e.clientY,
      origLeft: rect.left,
      origTop: rect.top,
    };

    const handleMouseMove = (ev: MouseEvent) => {
      if (!dragState.current) {
        return;
      }
      const dx = ev.clientX - dragState.current.startX;
      const dy = ev.clientY - dragState.current.startY;
      setPos({
        x: dragState.current.origLeft + dx,
        y: dragState.current.origTop + dy,
      });
    };

    const handleMouseUp = () => {
      dragState.current = null;
      document.removeEventListener('mousemove', handleMouseMove);
      document.removeEventListener('mouseup', handleMouseUp);
    };

    document.addEventListener('mousemove', handleMouseMove);
    document.addEventListener('mouseup', handleMouseUp);
  }, []);

  const handleResizeMouseDown = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
    e.preventDefault();
    e.stopPropagation();

    resizeState.current = {
      startX: e.clientX,
      startY: e.clientY,
      origWidth: size.width,
      origHeight: size.height,
    };

    const handleMouseMove = (ev: MouseEvent) => {
      if (!resizeState.current) {
        return;
      }
      const dx = ev.clientX - resizeState.current.startX;
      const dy = ev.clientY - resizeState.current.startY;
      setSize({
        width: Math.max(minWidth, resizeState.current.origWidth + dx),
        height: Math.max(minHeight, resizeState.current.origHeight + dy),
      });
    };

    const handleMouseUp = () => {
      resizeState.current = null;
      document.removeEventListener('mousemove', handleMouseMove);
      document.removeEventListener('mouseup', handleMouseUp);
    };

    document.addEventListener('mousemove', handleMouseMove);
    document.addEventListener('mouseup', handleMouseUp);
  }, [size, minWidth, minHeight]);

  // Закрытие по Esc
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        onClose();
      }
    };
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [onClose]);

  return (
    <div
      className="draggable-resizable"
      style={{
        left: pos.x,
        top: pos.y,
        width: size.width,
        height: size.height,
      }}
    >
      <div className="popup-header" onMouseDown={handleMouseDown}>
        <span className="popup-title">{title}</span>
        <button className="popup-close" onClick={onClose} title="Закрыть">
          ✕
        </button>
      </div>
      <div className="popup-body">{children}</div>
      <div className="resize-handle" onMouseDown={handleResizeMouseDown} />
    </div>
  );
}