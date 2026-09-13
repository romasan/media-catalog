import React from 'react';
import styles from './DateRangeLabel.module.css';

interface DateRangeLabelProps {
  minTimestamp: number;
  maxTimestamp: number;
}

const MONTHS_GENITIVE = [
  'января',
  'февраля',
  'марта',
  'апреля',
  'мая',
  'июня',
  'июля',
  'августа',
  'сентября',
  'октября',
  'ноября',
  'декабря',
];

function isSameDay(a: Date, b: Date): boolean {
  return (
    a.getFullYear() === b.getFullYear() &&
    a.getMonth() === b.getMonth() &&
    a.getDate() === b.getDate()
  );
}

function formatDayMonth(date: Date): string {
  return `${date.getDate()} ${MONTHS_GENITIVE[date.getMonth()]}`;
}

function formatDayMonthYear(date: Date): string {
  return `${formatDayMonth(date)} ${date.getFullYear()}`;
}

/**
 * Форматирует диапазон дат карточек в видимой области.
 * Уровень детализации зависит от длины диапазона:
 * - тот же день:            «22 апреля 2025»;
 * - тот же месяц:           «1 - 9 мая 2025»;
 * - тот же год:             «1 июня - 30 июля 2025»;
 * - разные годы:            «20 декабря 2025 - 7 января 2026».
 */
export function formatDateRange(minTimestamp: number, maxTimestamp: number): string {
  if (!Number.isFinite(minTimestamp) || !Number.isFinite(maxTimestamp)) {
    return '';
  }

  const from = new Date(minTimestamp);
  const to = new Date(maxTimestamp);

  if (isSameDay(from, to)) {
    return formatDayMonthYear(from);
  }

  if (from.getFullYear() === to.getFullYear() && from.getMonth() === to.getMonth()) {
    return `${from.getDate()} - ${formatDayMonthYear(to)}`;
  }

  if (from.getFullYear() === to.getFullYear()) {
    return `${formatDayMonth(from)} - ${formatDayMonthYear(to)}`;
  }

  return `${formatDayMonthYear(from)} - ${formatDayMonthYear(to)}`;
}

export function DateRangeLabel({
  minTimestamp,
  maxTimestamp,
}: DateRangeLabelProps): React.ReactElement | null {
  const text = formatDateRange(minTimestamp, maxTimestamp);
  if (!text) {
    return null;
  }

  return <div className={styles['date-range-label']}>📅 {text}</div>;
}
