'功能:
'	啟動一個 cmd.exe 設定環境變數來使用 VC++ 的編譯工具
'	把工作目錄切換到這個 script 檔案所在目錄
'
'撰寫原因:
'	每次啟動 Visual studio 2008 命令提示字元後還要手動切換當前工作目錄太麻煩了
'
'撰寫日期:
'	2010.3.27

Option Explicit		'強制變數需宣告

Dim wsh_shell, arg

Set wsh_shell = CreateObject("WScript.Shell")

'Visual studio 2008 命令提示字元設定檔
arg = "C:\Program Files\Microsoft Visual Studio 9.0\VC\vcvarsall.bat"

'/K :執行字串中所描述的命令然後保留命令視窗
wsh_shell.Run("cmd /K""" & arg & """")

'SendKeys method 會抓前景視窗並送出訊息
'這裡必須先暫停程式一下讓剛啟動的命令視窗完成創建 否則 SendKeys 會抓不到它
'Wscript.sleep 300

'送出 ctrl+whitespace 以消除左下方的倉頡輸入法
'要送出兩次 ctrl+whitespace 才會消除倉頡輸入法 原因不明
'wsh_shell.SendKeys "^ ^ "