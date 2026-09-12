#!/usr/bin/env python3
"""Run actual Gureum 1.13.2 method bodies against inert Swift collaborators.

No IMK server, event injection, application activation or preferences access.
Input: upstream OSXCore/InputReceiver.swift at commit
19e2778af80dac378603e8862208d60b2c8dc01d. See README for limitations.
"""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile


def method(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]


if len(sys.argv) != 2:
    raise SystemExit("Usage: test_gureum_state.py /path/to/upstream/InputReceiver.swift")
source = Path(sys.argv[1]).read_text()
# Ignore only trailing newlines introduced by retrieval tools.
digest = hashlib.sha256(source.rstrip("\n").encode()).hexdigest()
expected = "c4f3f888465f9f7a38d20f300a1b8165186a8ba9e484bbe98bee2f775b4c0903"
if digest != expected:
    raise SystemExit("Unexpected upstream source; refusing an unreviewed fixture")

input2 = method(source, "    func input2(", "    // MARK: InputTextDelegate")
input_text = method(source, "    func input(text", "    func input(event:")
commit = method(source, "    @discardableResult\n    func commitCompositionEvent", "    func updateCompositionEvent")
assert input_text.count("        inputting = true") == 1
assert input_text.count("        inputting = false") == 1

support = r'''
import AppKit
let DEBUG_INPUT_RECEIVER = false
let DEBUG_INPUTCONTROLLER = false
let DEBUG_LOGGING = false
func dlog(_ enabled: Bool, _ format: String, _ args: Any...) {}
enum KeyCode: Int { case a = 0 }
enum InputAction { case none, commit }
struct InputResult { var processed: Bool; var action: InputAction }
enum InputEvent { case unused }
protocol IMKTextInput {
    func selectedRange() -> NSRange
    func markedRange() -> NSRange
}
protocol IMKUnicodeTextInput {}
final class Client: NSObject, IMKTextInput, IMKUnicodeTextInput {
    func selectedRange() -> NSRange { NSRange(location: 0, length: 0) }
    func markedRange() -> NSRange { NSRange(location: NSNotFound, length: 0) }
    func insertText(_ text: String, replacementRange: NSRange) {}
}
final class Controller {
    let textClient = Client()
    func selectionRange() -> NSRange { textClient.selectedRange() }
    func client() -> Client { textClient }
}
final class Composer {
    var result = InputResult(processed: true, action: .none)
    func filterCommand(keyCode: KeyCode, modifiers: NSEvent.ModifierFlags,
                       client: IMKTextInput & IMKUnicodeTextInput) -> InputEvent? { nil }
    func input(text: String?, key: KeyCode, modifiers: NSEvent.ModifierFlags,
               client: IMKTextInput & IMKUnicodeTextInput) -> InputResult { result }
    func dequeueCommitString() -> String { "" }
}
final class InputMethodServer {
    static let shared = InputMethodServer()
    func showOrHideCandidates(controller: Controller) {}
}
final class Receiver {
    var inputting = false
    var hasSelectionRange = false
    var _internalComposedString = ""
    let composer = Composer()
    let controller = Controller()
    var externalCancelCount = 0
    func input(event: InputEvent, client: IMKTextInput & IMKUnicodeTextInput) -> InputResult {
        fatalError("This fixture never produces command events")
    }
    func cancelComposition() {}
    func updateComposition() {}
    func cancelCompositionEvent() { externalCancelCount += 1 }
'''
checks = r'''
}
var failures = 0
func check(_ condition: Bool, _ name: String) {
    print("\(condition ? "PASS" : "FAIL") \(name)")
    if !condition { failures += 1 }
}
for (name, flags, text, action) in [
    ("control", NSEvent.ModifierFlags.control, Optional("a"), InputAction.none),
    ("command", NSEvent.ModifierFlags.command, Optional("a"), InputAction.none),
    ("nil text", NSEvent.ModifierFlags(), Optional<String>.none, InputAction.none),
    ("composer commit", NSEvent.ModifierFlags(), Optional("a"), InputAction.commit),
    ("ordinary text", NSEvent.ModifierFlags(), Optional("a"), InputAction.none)
] {
    for initial in [false, true] {
        let receiver = Receiver()
        receiver.inputting = initial
        receiver.composer.result.action = action
        _ = receiver.input(text: text, key: .a, modifiers: flags, client: receiver.controller.client())
        check(receiver.inputting == initial, "\(name): restore initial=\(initial)")
        receiver.externalCancelCount = 0
        _ = receiver.commitCompositionEvent(receiver.controller.client())
        check(receiver.externalCancelCount == (initial ? 0 : 1),
              "\(name): subsequent commit classification initial=\(initial)")
    }
}
exit(failures == 0 ? 0 : 1)
'''

with tempfile.TemporaryDirectory(prefix="keystitch-ime-state-") as directory:
    tmp = Path(directory)
    fixture = tmp / "OSXCore" / "InputReceiver.swift"
    fixture.parent.mkdir()
    fixture.write_text(source)
    patch = Path(__file__).resolve().with_name("gureum-1.13.2-inputting.patch")
    subprocess.run(["git", "apply", "--check", str(patch)], cwd=tmp, check=True)
    subprocess.run(["git", "apply", str(patch)], cwd=tmp, check=True)
    fixed = method(fixture.read_text(), "    func input(text", "    func input(event:")
    for name, body, should_pass in [("upstream", input_text, False), ("candidate", fixed, True)]:
        swift = tmp / (name + ".swift")
        binary = tmp / name
        swift.write_text(support + input2 + body + commit + checks)
        subprocess.run(["xcrun", "swiftc", "-module-cache-path", str(tmp / "modules"),
                        str(swift), "-o", str(binary)], check=True, timeout=120)
        result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=10)
        print(f"--- {name} ---\n{result.stdout}", flush=True)
        expected_code = 0 if should_pass else 1
        if result.returncode != expected_code:
            raise SystemExit(f"Unexpected {name} result: {result.returncode}\n{result.stderr}")
        passes = sum(line.startswith("PASS ") for line in result.stdout.splitlines())
        failures = sum(line.startswith("FAIL ") for line in result.stdout.splitlines())
        if (passes, failures) != ((20, 0) if should_pass else (10, 10)):
            raise SystemExit(f"Unexpected assertion counts for {name}: {passes}, {failures}")
print("Regression reproduced in upstream; scoped-state candidate passes 20 assertions.")
print("This does NOT establish Safari/IMK integration correctness.")
