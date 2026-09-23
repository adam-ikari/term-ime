import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'term-ime',
  tagline: '在终端里直接输入中文——不管有没有桌面环境',
  favicon: 'img/favicon.svg',

  url: 'https://adam-ikari.github.io',
  baseUrl: '/term-ime/',

  organizationName: 'adam-ikari',
  projectName: 'term-ime',

  onBrokenLinks: 'warn',
  onBrokenMarkdownLinks: 'warn',

  // SEO: shared <head> metadata for the whole site.
  headTags: [
    {
      tagName: 'meta',
      attributes: {name: 'description', content: 'term-ime：TTY/TUI 环境的中文输入法。输入法引擎库 + TUI 输入法组件，封装 librime 拼音，无桌面依赖，单文件静态二进制。'},
    },
    {
      tagName: 'meta',
      attributes: {name: 'keywords', content: 'TTY 输入法,终端中文输入,TUI 输入法组件,输入法引擎库,librime,拼音输入法,Linux 终端输入法,IME,terminal input method'},
    },
    {
      tagName: 'meta',
      attributes: {property: 'og:title', content: 'term-ime — 终端里的中文输入法'},
    },
    {
      tagName: 'meta',
      attributes: {property: 'og:description', content: '在 TTY/TUI 环境直接输入中文：输入法引擎库 + 候选栏/状态栏 TUI 组件，封装 librime，无桌面依赖。'},
    },
    {
      tagName: 'meta',
      attributes: {property: 'og:type', content: 'website'},
    },
    {
      tagName: 'meta',
      attributes: {property: 'og:image', content: '/term-ime/img/og-image.png'},
    },
    {
      tagName: 'meta',
      attributes: {property: 'og:locale', content: 'zh_CN'},
    },
    {
      tagName: 'meta',
      attributes: {name: 'twitter:card', content: 'summary_large_image'},
    },
    {
      tagName: 'meta',
      attributes: {name: 'twitter:title', content: 'term-ime — 终端里的中文输入法'},
    },
    {
      tagName: 'meta',
      attributes: {name: 'twitter:description', content: 'TTY/TUI 中文输入法：输入法引擎库 + TUI 输入法组件，封装 librime。'},
    },
    {
      tagName: 'meta',
      attributes: {name: 'twitter:image', content: '/term-ime/img/og-image.png'},
    },
  ],

  i18n: {
    defaultLocale: 'zh-CN',
    locales: ['zh-CN'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.ts',
          editUrl: 'https://github.com/adam-ikari/term-ime/tree/master/website/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
        sitemap: {
          lastmod: 'date',
          changefreq: 'weekly',
          priority: 0.5,
          ignorePatterns: ['/404.html'],
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    image: 'img/og-image.png',
    colorMode: {
      defaultMode: 'dark',
      disableSwitch: true,
      respectPrefersColorScheme: false,
    },
    navbar: {
      title: 'term-ime',
      logo: {
        alt: 'term-ime',
        src: 'img/favicon.svg',
      },
      items: [
        {type: 'docSidebar', sidebarId: 'docs', position: 'left', label: '文档'},
        {href: 'https://github.com/adam-ikari/term-ime', label: 'GitHub', position: 'right'},
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: '文档',
          items: [
            {label: '快速开始', to: '/docs/quickstart'},
            {label: '快捷键', to: '/docs/shortcuts'},
            {label: '配置', to: '/docs/config'},
          ],
        },
        {
          title: '项目',
          items: [
            {label: 'GitHub', href: 'https://github.com/adam-ikari/term-ime'},
            {label: 'Release', href: 'https://github.com/adam-ikari/term-ime/releases'},
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} term-ime. MIT License.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;