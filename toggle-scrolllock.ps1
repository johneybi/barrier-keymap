$code = @'
using System;
using System.Runtime.InteropServices;

public static class NativeKeyboard {
    [DllImport("user32.dll")]
    public static extern short GetKeyState(int nVirtKey);

    [DllImport("user32.dll")]
    public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
}
'@

Add-Type -TypeDefinition $code -ErrorAction SilentlyContinue

$vkScroll = 0x91
$keyUp = 0x2
$before = ([NativeKeyboard]::GetKeyState($vkScroll) -band 1) -ne 0

if ($before) {
    [NativeKeyboard]::keybd_event([byte]$vkScroll, 0x45, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [NativeKeyboard]::keybd_event([byte]$vkScroll, 0x45, $keyUp, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}

$after = ([NativeKeyboard]::GetKeyState($vkScroll) -band 1) -ne 0
Write-Host "ScrollLock before=$before after=$after"
Start-Sleep -Seconds 1
