import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
  vite: {
    assetsInclude: ['**/*.blob'],
  },
  lang: 'zh-CN',
  title: "极域工具箱",
  description: "一款能够解除极域电子教室断网、解除U盘使用限制等功能的软件",
  head: [
    ['link', { rel: 'icon', href: '/logo.png' }],
    ['script', { type: 'text/javascript' },
      `(function(c,l,a,r,i,t,y){
        c[a]=c[a]||function(){(c[a].q=c[a].q||[]).push(arguments)};
        t=l.createElement(r);t.async=1;t.src="https://www.clarity.ms/tag/"+i;
        y=l.getElementsByTagName(r)[0];y.parentNode.insertBefore(t,y);
      })(window, document, "clarity", "script", "o6nldo4au2");`
    ],
    ['script', { async: true, src: 'https://www.googletagmanager.com/gtag/js?id=G-F0FMWV284X' }],
    ['script', {},
      `window.dataLayer = window.dataLayer || [];
      function gtag(){dataLayer.push(arguments);}
      gtag('js', new Date());
      gtag('config', 'G-F0FMWV284X');`
    ],
  ],
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    logo: '/logo.png',
    lastUpdated: true,
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
      message: '基于 MIT 许可发布',
      copyright: '版权所有 © 2024-2026 liyixin21'
    },

    // 搜索
    search: {
      provider: 'local',
      options: {
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
    },

  }
})
