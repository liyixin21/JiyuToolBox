/**
 * 公告状态（Notice.vue 内部使用的单例）。
 *
 * 两种关闭语义，刻意区分：
 *   「关闭」    —— 纯临时，只改内存状态、不写任何存储；刷新页面后会重新弹出
 *   「不再显示」—— 持久，写入 localStorage；之后刷新/新开都只保留右下角图标
 *
 * 手动点右下角图标重新打开公告 = 撤回"不再显示"（清除标记），所以之后
 * 再点「关闭」刷新仍会弹出，不会出现"点了不再显示就再也回不来"的死锁。
 *
 * 模块级 ref 在 SSR 期间初值恒为"已关闭"，只有客户端 onMounted 才会改写，不会跨请求泄漏。
 */
import { ref } from 'vue'
import notice from '../notice.mjs'

const LOCAL_PREFIX = 'vitepress:notice-dismissed:'

export const isNoticeEnabled = notice.enabled !== false && notice.mode !== 'none'
export const localKey = `${LOCAL_PREFIX}${notice.id}`
export const noticeConfig = notice

export const isDismissed = ref(true)
export const isCollapsed = ref(!!notice.floating?.defaultCollapsed)

// 隐私模式下存储可能不可用，读写都要容错
const safeGet = (storage, key) => {
  try {
    return storage.getItem(key)
  } catch {
    return null
  }
}

const safeRemove = (storage, key) => {
  try {
    storage.removeItem(key)
  } catch {
    /* 忽略 */
  }
}

const safeSet = (storage, key) => {
  try {
    storage.setItem(key, '1')
  } catch {
    /* 忽略 */
  }
}

/** ?notice=preview 用于本地预览：忽略记忆且不写入 */
export const isPreview = () => {
  if (typeof window === 'undefined') return false
  return new URLSearchParams(window.location.search).get('notice') === 'preview'
}

/** 读取历史记忆，判断这次访问是否还需要自动弹出 */
export const readMemory = () => {
  if (typeof window === 'undefined') return false
  if (isPreview() || notice.rememberDismiss === false) return false
  return safeGet(window.localStorage, localKey) === '1'
}

/** 关闭：纯临时，不写任何存储，刷新后会重新弹出 */
export const closeNotice = () => {
  isDismissed.value = true
}

/** 不再显示：写入持久记忆，之后不再自动出现 */
export const neverShowNotice = () => {
  isDismissed.value = true
  if (typeof window === 'undefined' || isPreview() || notice.rememberDismiss === false) return
  safeSet(window.localStorage, localKey)
}

/** 重新打开：手动唤起时一并撤回"不再显示"，避免之后点关闭又"刷不出来" */
export const openNotice = () => {
  isDismissed.value = false
  isCollapsed.value = false
  if (typeof window === 'undefined') return
  safeRemove(window.localStorage, localKey)
}

/** 把公告收起成圆形图标（仅悬浮窗模式） */
export const collapseNotice = () => {
  isCollapsed.value = true
}
