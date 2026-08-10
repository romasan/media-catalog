import type { MediaFile, MetaTag, MetaTagKind } from './types';

export const META_TAG_PREFIX = 'meta:';

export const UNTAGGED_META_TAG_ID = `${META_TAG_PREFIX}untagged`;
export const UNTAGGED_META_TAG_GROUP = 'Прочее';
export const UNTAGGED_META_TAG: MetaTag = {
  id: UNTAGGED_META_TAG_ID,
  name: 'без тега',
  kind: 'untagged',
  group: UNTAGGED_META_TAG_GROUP,
};

export const META_TAG_GROUPS: Array<{ kind: MetaTagKind; group: string }> = [
  { kind: 'year', group: 'Годы' },
  { kind: 'season', group: 'Времена года' },
  { kind: 'type', group: 'Тип файла' },
  { kind: 'untagged', group: UNTAGGED_META_TAG_GROUP },
];

const SEASON_NAMES: Array<{ name: string; months: number[] }> = [
  { name: 'зима', months: [0, 1, 11] }, // декабрь, январь, февраль
  { name: 'весна', months: [2, 3, 4] }, // март, апрель, май
  { name: 'лето', months: [5, 6, 7] }, // июнь, июль, август
  { name: 'осень', months: [8, 9, 10] }, // сентябрь, октябрь, ноябрь
];

export function getSeason(month: number): string {
  const season = SEASON_NAMES.find((s) => s.months.includes(month));
  return season ? season.name : '';
}

export function getMetaTagById(metaTagId: string): MetaTag | null {
  if (metaTagId === UNTAGGED_META_TAG_ID) {
    return UNTAGGED_META_TAG;
  }
  if (!metaTagId.startsWith(META_TAG_PREFIX)) {
    return null;
  }
  const parts = metaTagId.split(':');
  if (parts.length !== 3) {
    return null;
  }
  const kind = parts[1] as MetaTagKind;
  const name = parts[2];
  if (!name) {
    return null;
  }
  if (kind === 'year') {
    return { id: metaTagId, name, kind, group: 'Годы' };
  }
  if (kind === 'season') {
    return { id: metaTagId, name, kind, group: 'Времена года' };
  }
  if (kind === 'type') {
    return { id: metaTagId, name, kind, group: 'Тип файла' };
  }
  return null;
}

export function isMetaTagId(tagId: string): boolean {
  return tagId.startsWith(META_TAG_PREFIX);
}

export function getMetaTagsForFile(file: Pick<MediaFile, 'createdAt' | 'type'>): MetaTag[] {
  const date = new Date(file.createdAt);
  const year = date.getFullYear();
  const season = getSeason(date.getMonth());
  const typeName = file.type === 'video' ? 'видео' : 'фото';

  return [
    { id: `${META_TAG_PREFIX}year:${year}`, name: String(year), kind: 'year', group: 'Годы' },
    { id: `${META_TAG_PREFIX}season:${season}`, name: season, kind: 'season', group: 'Времена года' },
    { id: `${META_TAG_PREFIX}type:${typeName}`, name: typeName, kind: 'type', group: 'Тип файла' },
  ];
}

export function getMediaMatchesMetaTag(file: Pick<MediaFile, 'createdAt' | 'type'>, metaTagId: string): boolean {
  return getMetaTagsForFile(file).some((metaTag) => metaTag.id === metaTagId);
}

export function getAllMetaTags(files: MediaFile[]): MetaTag[] {
  const yearNames = new Set<string>();
  const seasonNames = new Set<string>();
  const typeNames = new Set<string>();

  for (const file of files) {
    const metaTags = getMetaTagsForFile(file);
    for (const metaTag of metaTags) {
      if (metaTag.kind === 'year') {
        yearNames.add(metaTag.name);
      } else if (metaTag.kind === 'season') {
        seasonNames.add(metaTag.name);
      } else if (metaTag.kind === 'type') {
        typeNames.add(metaTag.name);
      }
    }
  }

  const metaTags: MetaTag[] = [];

  // Годы — по убыванию
  const years = Array.from(yearNames).sort((a, b) => Number(b) - Number(a));
  for (const year of years) {
    metaTags.push({ id: `${META_TAG_PREFIX}year:${year}`, name: year, kind: 'year', group: 'Годы' });
  }

  // Сезоны в календарном порядке: зима, весна, лето, осень
  const seasonOrder = ['зима', 'весна', 'лето', 'осень'];
  const seasons = seasonOrder.filter((s) => seasonNames.has(s));
  for (const season of seasons) {
    metaTags.push({ id: `${META_TAG_PREFIX}season:${season}`, name: season, kind: 'season', group: 'Времена года' });
  }

  // Типы: фото, видео
  const types = ['фото', 'видео'].filter((t) => typeNames.has(t));
  for (const type of types) {
    metaTags.push({ id: `${META_TAG_PREFIX}type:${type}`, name: type, kind: 'type', group: 'Тип файла' });
  }

  // "Без тега" - всегда доступен в группе "Прочее"
  metaTags.push(UNTAGGED_META_TAG);

  return metaTags;
}
