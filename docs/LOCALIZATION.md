# libwxui 多语言

libwxui 使用窗口独立的 UTF-8 语言表。语言包不依赖操作系统的 gettext 安装，
也不改变进程全局 locale。所有加载、切换、翻译和控件操作都在 UI 线程执行。

## 语言包与 XML

```xml
<!-- languages/en.xml -->
<Language>
  <String id="action.run">Run</String>
  <String id="request.hint">Enter request</String>
  <String id="action.tip">Submit the current request</String>
</Language>
```

```xml
<Button text="执行" textid="action.run"
        tooltip="提交当前请求" tooltipid="action.tip"/>
<Edit hint="请输入请求" hintid="request.hint"/>
```

`textid`、`hintid`、`tooltipid` 与相应原文属性绑定；属性先后顺序无关。
原文属性仍可单独使用，旧 XML 无需修改。
键可以是稳定标识，也可以是原文；DevTools 使用中文原文作为键，便于核对翻译。
XML 特殊字符使用 `&amp;`、`&lt;` 等实体，或在 String 内容中使用 CDATA。
空译文合法；重复键、嵌套标签、无效 XML 会抛出异常，原语言表保留。
重新加载同一语言会整体替换该语言表。

## 应用接入

```cpp
// resources 已传给 DesktopWindow，负责返回编译进 EXE 的资源字节。
window.LoadLanguageResource("zh-CN", "languages/zh-CN.xml");
window.LoadLanguageResource("en", "languages/en.xml");
window.SetFallbackLanguage("zh-CN");
window.SetLanguage("en");

// 动态标题、状态栏、对话框等，由应用获取译文后更新。
window.SetTitle(window.Translate("window.title", "开发工具"));
window.SetStatus(window.Translate("status.ready", "准备就绪"));

// 动态创建的按钮、标签、树节点同样可以绑定。
button->BindTranslation("text", "action.run", "执行");
```

已有 XML 控件和绑定的动态控件在 `SetLanguage()` 后自动更新，不重建窗口。
折叠树节点也会更新。语言切换保留事件绑定、输入内容以及未绑定属性。
编辑框通常只绑定 `hint`；显式绑定 `text` 表示允许切换时替换该控件文本。
如需转为应用手动管理，先调用 `UnbindTranslation("text")`。
通过 `Combo::AddItem()` 等接口添加的独立条目需由应用使用 `Translate()` 后更新。
原生文件选择器的系统按钮仍遵循操作系统语言。

回退顺序为：当前语言 → 父语言（例如 `en-GB` → `en`）→ 默认语言及其父语言
→ 指定原文 → 键名。语言标记忽略大小写，并将 `_` 视为 `-`。
每个窗口的语言和资源独立，互不影响；直接使用 UIManager 时也有同名加载和切换接口。

目前提供字符串翻译，不包含复数规则、日期/数字本地化或 RTL 布局镜像。

## DevTools

默认简体中文；顶部 `English` / `简体中文` 按钮即时切换。也可使用
`sovkit-devtools.exe --language en` 启动英文界面。
当前选择只作用于本次运行，不写入用户配置。

API 名称、请求模板、SDK 返回字段和接入文档保持原始契约。
切换时不会加载或重启 SDK，不会清空请求、响应和记录。
新增语言时添加 `res/languages/<语言>.xml`，在应用中加载并选择它。
`res` 下的语言文件由现有资源脚本自动编译进程序，Windows 发布仍只有 EXE 和 SDK DLL。

`libwxui.localization` 回归覆盖语言回退、损坏包回滚、嵌入资源、运行时更新、
折叠树节点、输入及事件保留、多窗口隔离。
