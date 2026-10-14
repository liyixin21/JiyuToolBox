import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
const SITE_URL = 'https://jiyutool.liyixin.vip'
const SITE_NAME = '极域工具箱'

// relativePath: 'index.md' -> '/', 'how-to-use.md' -> '/how-to-use.html'
const toPageUrl = (relativePath) =>
  `${SITE_URL}/${relativePath.replace(/(^|\/)index\.md$/, '$1').replace(/\.md$/, '.html')}`

/**
 * 中文检索分词器。
 *
 * 默认分词按空白/标点切分，中文正文会整句变成一个 token，搜「断网」「截屏」等词永远为空；
 * 而 Intl.Segmenter 对中文是逐字切分（实测「解除断网」-> 解除 / 断 / 网），同样搜不到「断网」。
 * 因此改为：CJK 连续段切「单字 + 相邻二元组」，拉丁/数字段按词切分并转小写。
 * 这样任意两字词都能命中，较长查询靠多 token 的 OR 匹配 + 前缀匹配兜底。
 *
 * 注意：该函数会被 VitePress 序列化进 siteData、在客户端用 new Function 还原，
 * 因此必须自包含——不能引用本文件里的其它变量或 import。
 */
const searchTokenizer = (text) => {
  const cjk = '\\u3400-\\u4dbf\\u4e00-\\u9fff\\uf900-\\ufaff'
  const cjkOnly = new RegExp('^[' + cjk + ']+$')
  const segments = new RegExp('[' + cjk + ']+|[^' + cjk + ']+', 'g')
  const tokens = []
  for (const segment of String(text).match(segments) || []) {
    if (cjkOnly.test(segment)) {
      for (let i = 0; i < segment.length; i++) {
        tokens.push(segment[i])
        if (i + 1 < segment.length) tokens.push(segment.slice(i, i + 2))
      }
    } else {
      for (const word of segment.toLowerCase().split(/[^a-z0-9]+/)) {
        if (word) tokens.push(word)
      }
    }
  }
  return tokens
}

export default defineConfig({
  lang: 'zh-CN',
  title: "极域工具箱",
  description: "极域工具箱：解除极域电子教室断网、U盘限制与键盘锁定，支持窗口化屏幕广播、退出黑屏、挂起极域、置顶窗口与防截屏。",

  // 报告类文档不进站点，避免生成多余页面与污染搜索索引
  srcExclude: ['FIXES.md'],

  // 生成 sitemap.xml（docs:build 时产出）
  sitemap: { hostname: SITE_URL },

  // 每页注入 canonical 与 Open Graph / Twitter 卡片
  transformHead({ pageData, title, description }) {
    const url = toPageUrl(pageData.relativePath)
    const ogTitle = title.includes(SITE_NAME) ? title : `${title} | ${SITE_NAME}`
    const image = `${SITE_URL}/logo.png`
    return [
      ['link', { rel: 'canonical', href: url }],
      ['meta', { property: 'og:type', content: 'website' }],
      ['meta', { property: 'og:site_name', content: SITE_NAME }],
      ['meta', { property: 'og:title', content: ogTitle }],
      ['meta', { property: 'og:description', content: description }],
      ['meta', { property: 'og:url', content: url }],
      ['meta', { property: 'og:image', content: image }],
      ['meta', { name: 'twitter:card', content: 'summary' }],
      ['meta', { name: 'twitter:title', content: ogTitle }],
      ['meta', { name: 'twitter:description', content: description }],
      ['meta', { name: 'twitter:image', content: image }],
    ]
  },
  head: [
    ['link', { rel: 'icon', href: '/logo.png' }],
    // 百度站长平台 HTML 标签验证；每个页面都会注入，验证时保持不动
    ['meta', { name: 'baidu-site-verification', content: 'codeva-x7HpfbriBw' }],
    ['link', {
      rel: 'stylesheet',
      href: 'https://cdn.jsdelivr.net/npm/@waline/client@v3/dist/waline.css',
      // CDN 不可用时退回 unpkg，避免评论样式丢失
      onerror: "this.onerror=null;this.href='https://unpkg.com/@waline/client@v3/dist/waline.css'",
    }],
    ['script', { type: 'text/javascript' },
      `(function(c,l,a,r,i,t,y){
        c[a]=c[a]||function(){(c[a].q=c[a].q||[]).push(arguments)};
        t=l.createElement(r);t.async=1;t.src="https://www.clarity.ms/tag/"+i;
        y=l.getElementsByTagName(r)[0];y.parentNode.insertBefore(t,y);
      })(window, document, "clarity", "script", "o6nldo4au2");`
    ],
    ['script', { async: true, src: 'https://www.googletagmanager.com/gtag/js?id=G-919T2B5W81' }],
    ['script', {},
      `window.dataLayer = window.dataLayer || [];
      function gtag(){dataLayer.push(arguments);}
      gtag('js', new Date());
      gtag('config', 'G-919T2B5W81');`
    ],
  ],
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    logo: '/logo.png',
    lastUpdated: { text: '最后更新于' },
    // 导航栏
    nav: [
      { text: '主页', link: '/' },
      { text: '使用教程', link: '/how-to-use' },
      { text: '更新日志', link: '/changelog'},
    ],
    
    // 侧边栏
    sidebar: [
      {
        text: 'JiyuToolBox',
        items: [
          { text: '如何下载与使用', link: '/how-to-use' },
          { text: '问题反馈', link: '/feedback' },
          { text: '更新日志', link: '/changelog'},
        ]
      }
    ],

    // 社交链接
    socialLinks: [
      { icon: 'github', link: 'https://github.com/liyixin21/JiyuToolBox' }
    ],

    // 页脚
    footer: {
      message: '基于 GPL-3.0 许可发布',
      copyright: '版权所有 © 2024-2026 liyixin21'
    },

    // 搜索
    search: {
      provider: 'local',
      options: {
        miniSearch: {
          options: { tokenize: searchTokenizer },
        },
        locales: {
          root: {
            translations: {
              button: {
                buttonText: '搜索',
                buttonAriaLabel: '搜索'
              },
              modal: {
                noResultsText: '无法找到相关结果',
                resetButtonTitle: '清除查询条件',
                displayDetails: '显示更多',
                footer: {
                  selectText: '选择',
                  navigateText: '切换',
                  closeText: '关闭'
                }
              }
            }
          }
        }
      }
    },

    // 汉化
    //头上角要主题切换的文字 Appearance
    darkModeSwitchLabel: "切换主题",
    lightModeSwitchTitle: "切换到浅色模式",
    darkModeSwitchTitle: "切换到深色模式",
    // 文章翻页
    docFooter: {
      prev: "上一篇", //Next page
      next: "下一篇", //Previous page
    },
    //当前页面 On this page
    outlineTitle: "页面导航",

    // 返回顶部 Return to top
    returnToTopLabel: "返回顶部",

    // 菜单  Menu
    sidebarMenuLabel: "菜单",
    
    notFound: {
      title: "页面未找到",
      quote: "哎呀，您好像迷失在网络的小胡同里啦，别着急，赶紧回头是岸！",
      linkText: "返回首页",
      linkLabel: "返回首页",
    },

  }
})
