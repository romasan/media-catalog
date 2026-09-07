import { observer } from 'mobx-react-lite';
import React from 'react';
import { useApp } from '../../store/AppStore';
import styles from './ToastContainer.module.css';

export const ToastContainer = observer(function ToastContainer(): React.ReactElement {
  const { toasts, dismissToast } = useApp();

  return (
    <div className={styles['toast-container']}>
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className={`${styles['toast']} ${styles['toast-' + toast.type]}`}
          onClick={() => dismissToast(toast.id)}
        >
          {toast.message}
        </div>
      ))}
    </div>
  );
});
