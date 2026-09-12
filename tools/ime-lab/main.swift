import AppKit
import WebKit

// Isolated manual test surface. No event taps, key injection, TIS writes,
// external navigation, persistence, or inspection of another application's text.
final class NativeInput: NSTextView {
    var record: ((String) -> Void)?
    override func setMarkedText(_ text: Any, selectedRange: NSRange, replacementRange: NSRange) {
        super.setMarkedText(text, selectedRange: selectedRange, replacementRange: replacementRange)
        record?("native marked: active=\(hasMarkedText()) length=\(markedRange().length)")
    }
    override func insertText(_ text: Any, replacementRange: NSRange) {
        super.insertText(text, replacementRange: replacementRange)
        record?("native insert: totalUTF16=\(string.utf16.count) marked=\(hasMarkedText())")
    }
}

final class Lab: NSObject, NSApplicationDelegate, WKScriptMessageHandler, WKNavigationDelegate {
    var window: NSWindow!
    let logView = NSTextView()
    var lines: [String] = []
    let started = Date()

    func record(_ value: String) {
        lines.append(String(format: "%.3f %@", Date().timeIntervalSince(started), value))
        if lines.count > 200 { lines.removeFirst(lines.count - 200) }
        logView.string = lines.joined(separator: "\n")
        logView.scrollToEndOfDocument(nil)
    }

    func userContentController(_ controller: WKUserContentController, didReceive message: WKScriptMessage) {
        guard message.name == "ime", message.frameInfo.isMainFrame,
              let value = message.body as? String, value.count < 200 else { return }
        record(value)
    }

    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction,
                 decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        decisionHandler(navigationAction.request.url?.absoluteString == "about:blank" ? .allow : .cancel)
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 860, height: 680),
                          styleMask: [.titled, .closable, .miniaturizable, .resizable],
                          backing: .buffered, defer: false)
        window.title = "KeyStitch IME Lab — Manual test only"
        window.isReleasedWhenClosed = false
        let root = NSStackView()
        root.orientation = .vertical
        root.alignment = .leading
        root.spacing = 12
        root.edgeInsets = NSEdgeInsets(top: 16, left: 16, bottom: 16, right: 16)
        window.contentView = root
        let help = NSTextField(wrappingLabelWithString:
            "전용 테스트 창입니다. 전환키를 자동으로 보내지 않습니다.\n" +
            "각 입력창에서 한글 조합 → 영문 전환 → abc 입력을 비교하세요. " +
            "기록은 이 창의 이벤트·길이만 표시하며 저장하지 않습니다.\n" +
            "내장 키보드 전환과 Windows 키보드 전환을 구분하여 시험하세요.")
        root.addArrangedSubview(help)
        let nativeLabel = NSTextField(labelWithString: "1. macOS 기본 입력창 (NSTextView)")
        root.addArrangedSubview(nativeLabel)
        let nativeScroll = NSScrollView()
        nativeScroll.borderType = .bezelBorder
        nativeScroll.hasVerticalScroller = true
        let native = NativeInput(frame: NSRect(x: 0, y: 0, width: 800, height: 90))
        native.isRichText = false
        native.font = .systemFont(ofSize: 20)
        native.autoresizingMask = [.width]
        native.isVerticallyResizable = true
        native.isHorizontallyResizable = false
        native.textContainer?.widthTracksTextView = true
        native.record = { [weak self] in self?.record($0) }
        nativeScroll.documentView = native
        root.addArrangedSubview(nativeScroll)
        let config = WKWebViewConfiguration()
        config.websiteDataStore = .nonPersistent()
        config.userContentController.add(self, name: "ime")
        let web = WKWebView(frame: .zero, configuration: config)
        web.navigationDelegate = self
        root.addArrangedSubview(web)
        web.loadHTMLString("""
        <!doctype html><meta charset="utf-8">
        <meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'">
        <style>body{font:14px system-ui;margin:8px}input,textarea{box-sizing:border-box;width:100%;font:20px system-ui;margin:6px 0}textarea{height:60px}</style>
        <label>2. WebKit 검색 입력창<input id="search" type="search" autocomplete="off" spellcheck="false"></label>
        <label>3. WebKit 일반 입력창<textarea id="area" autocomplete="off" spellcheck="false"></textarea></label>
        <script>
        for (const el of document.querySelectorAll('input,textarea')) {
          for (const name of ['focus','blur','compositionstart','compositionupdate','compositionend','beforeinput','input']) {
            el.addEventListener(name, e => {
              const kind = typeof e.inputType === 'string' ? e.inputType : '-';
              window.webkit.messageHandlers.ime.postMessage(
                `web ${el.id} ${name} type=${kind} composing=${!!e.isComposing} totalUTF16=${el.value.length}`);
            });
          }
        }
        </script>
        """, baseURL: nil)
        let logLabel = NSTextField(labelWithString: "최근 이벤트 — 글자 내용은 기록하지 않음 / Safari 자체 재현을 보장하지 않음")
        root.addArrangedSubview(logLabel)
        let logScroll = NSScrollView()
        logScroll.hasVerticalScroller = true
        logView.isEditable = false
        logView.isSelectable = true
        logView.font = .monospacedSystemFont(ofSize: 11, weight: .regular)
        logView.autoresizingMask = [.width]
        logScroll.documentView = logView
        root.addArrangedSubview(logScroll)
        for view in root.arrangedSubviews {
            view.translatesAutoresizingMaskIntoConstraints = false
            view.widthAnchor.constraint(equalTo: root.widthAnchor, constant: -32).isActive = true
        }
        nativeScroll.heightAnchor.constraint(equalToConstant: 90).isActive = true
        web.heightAnchor.constraint(equalToConstant: 190).isActive = true
        logScroll.heightAnchor.constraint(greaterThanOrEqualToConstant: 120).isActive = true
        record("Lab ready. No automatic toggle. No file logging.")
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

let application = NSApplication.shared
let delegate = Lab()
application.delegate = delegate
application.setActivationPolicy(.regular)
let menu = NSMenu()
let appItem = NSMenuItem()
menu.addItem(appItem)
let appMenu = NSMenu()
appMenu.addItem(withTitle: "Quit IME Lab", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
appItem.submenu = appMenu
application.mainMenu = menu
application.run()
