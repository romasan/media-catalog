import { observer } from 'mobx-react-lite';
import React from 'react';
import { useApp } from '../store/AppStore';

export const ToastContainer = observer(function ToastContainer(): React.ReactElement {
  const { toasts, dismissToast } = useApp();

  return (
    <div className="toast-container">
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className={`toast toast-${toast.type}`}
          onClick={() => dismissToast(toast.id)}
        >
          {toast.message}
        </div>
      ))}
    </div>
  );
});
