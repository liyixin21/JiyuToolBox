/**
 * 公告图标库（线性 SVG，非 emoji）。
 * 每项：viewBox + shapes（shape = { tag, attrs }），描边颜色/粗细由 NoticeIcon.vue 统一注入，
 * 所以这里不写 stroke 颜色，保证能跟随主题深浅色。
 */
export const noticeIcons = {
  board: {
    viewBox: '0 0 48 48',
    shapes: [
      { tag: 'rect', attrs: { x: 4, y: 15, width: 40, height: 26, rx: 2 } },
      { tag: 'path', attrs: { d: 'M24 7L16 15H32L24 7Z' } },
      { tag: 'path', attrs: { d: 'M12 24H30' } },
      { tag: 'path', attrs: { d: 'M12 32H20' } },
    ],
  },
  megaphone: {
    viewBox: '0 0 24 24',
    shapes: [
      { tag: 'path', attrs: { d: 'm3 11 16.5-4.6a.6.6 0 0 1 .8.6v9.9a.6.6 0 0 1-.8.6L3 13z' } },
      { tag: 'path', attrs: { d: 'M11.6 16.4a3 3 0 0 1-5.8-1.6' } },
    ],
  },
  bell: {
    viewBox: '0 0 24 24',
    shapes: [
      { tag: 'path', attrs: { d: 'M6.3 9a5.7 5.7 0 0 1 11.4 0c0 6.2 2.3 7.6 2.3 7.6H4S6.3 15.2 6.3 9z' } },
      { tag: 'path', attrs: { d: 'M10.3 20a2 2 0 0 0 3.4 0' } },
    ],
  },
  info: {
    viewBox: '0 0 24 24',
    shapes: [
      { tag: 'path', attrs: { d: 'M12 3.2a8.8 8.8 0 1 0 0 17.6 8.8 8.8 0 0 0 0-17.6z' } },
      { tag: 'path', attrs: { d: 'M12 11.4v4.8' } },
      { tag: 'path', attrs: { d: 'M12 8.1h.01' } },
    ],
  },
  spark: {
    viewBox: '0 0 24 24',
    shapes: [
      { tag: 'path', attrs: { d: 'M12 3.2v3.4' } },
      { tag: 'path', attrs: { d: 'M12 17.4v3.4' } },
      { tag: 'path', attrs: { d: 'M4.9 12H8.3' } },
      { tag: 'path', attrs: { d: 'M15.7 12h3.4' } },
      { tag: 'path', attrs: { d: 'M6.5 6.5l2.4 2.4' } },
      { tag: 'path', attrs: { d: 'M15.1 15.1l2.4 2.4' } },
      { tag: 'path', attrs: { d: 'M17.5 6.5l-2.4 2.4' } },
      { tag: 'path', attrs: { d: 'M8.9 15.1l-2.4 2.4' } },
    ],
  },
}

export const defaultNoticeIcon = 'board'

/** 取图标定义；名字非法时回退到默认图标 */
export const resolveNoticeIcon = (name) => noticeIcons[name] || noticeIcons[defaultNoticeIcon]
