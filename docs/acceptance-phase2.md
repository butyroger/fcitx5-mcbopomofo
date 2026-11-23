# Phase 2 實機驗收（learning.2）

測試機：Linux Mint 22.3（fcitx5）。套件：`fcitx5-mcbopomofo-smart-learning_3.1.1-learning.2_amd64.deb`。
結果欄請驗收時填寫（通過／失敗＋備註）。

按鍵說明：許氏鍵盤的注音對應 `hwf` = ㄏㄠˇ、`dk`+`f` = ㄉㄤˇ（標準鍵盤請改打對應的標準鍵位，如 ㄏㄠˇ = `cl3`、ㄉㄤˇ = `2;3`；「ˇ」為 `3`）。

## 1. 標準與許氏鍵盤從狀態列選單互切
- 操作：點狀態列（面板）的小麥注音選單，選「標準」或「許氏」，立即打字確認。
- 許氏打 `hwf`（好）；切到標準後打 `cl3`（好）。
- 預期：切換後立即生效，不需重啟輸入法。
- 結果：通過。2026-11-23 以 xdotool 在 xed 實測：選單最上方有「注音－標準鍵盤」「注音－許氏鍵盤」，點選後設定檔立即改為 standard／hsu，接著打字即生效（許氏 `nefhwf` → 你好；標準 `su3cl3` → 你好）。

## 2. 英文與中文混打結果與 Mac 相同
- `app`：標準鍵盤打 `app`，組字緩衝區應以英文顯示 `app`，與 Mac 比對。
- `you` + 空白：You／you 的結果與 Mac 相同。
- `ChatGPT`：保持 ChatGPT 大小寫。
- 許氏：`hello` 再 `hwf` → hello 好。標準：`hello` 再 `cl3`。
- 許氏：`PDF` 再 `dk` + `f` → 「PDF 黨」（黨是隨附資料中的第一候選；要選「檔」請從候選字窗挑選）。標準：`PDF` 再 `2;3`。
- 結果：通過。標準 `app` → app；`you`＋空白 → `you `；`ChatGPT` → ChatGPT；許氏 `hello`＋`hwf` → hello 好；標準 `hello`＋`cl3` → hello 好；許氏 `PDFdk`＋`f` → PDF 黨。另：許氏 `hellojkq` 在打到 `j`（第四聲）時就依規則轉成「hell 噢」（hell 是英文單字），與 Mac 規則一致。

## 3. 自動空白與 Backspace
- 「你好 hello」：打你好後接 `hello`，確認中間有空白。
- 「用 iPhone 拍」：用、iPhone、拍之間皆有空白。
- Backspace 把英文逐字刪光：自動補的空白要一起消失。
- 結果：通過。許氏「你好」＋`hello` → 你好 hello；接著 Backspace 五次 → 你好（空白一起刪除）。「用 iPhone 拍」未個別測。

## 4. 標點半形／全形
- 「價格是300.」：數字後的句點為半形。
- 「好，」：中文後的逗號為全形。
- 英文後的標點同樣為半形（例如 `hello,`）。
- 結果：部分通過。「好」＋`,` → 好，（全形）；`hello`＋`,` → hello,（半形）。「價格是300.」未測。

## 5. iphone 大小寫修正
- `iphone` + 空白 → iPhone。
- `iphone` + Enter → 保留 iphone（小寫）。
- 結果：部分通過。`iphone`＋空白 → iPhone。Enter 保留小寫未測。

## 6. Esc 修剪
- 打 `今天開會 jqvx`（許氏／標準皆可，亂碼為組字中的未完成注音），按 Esc 一次：只清掉亂碼。
- 再按 Esc 一次（連按兩次）：清除全部組字。
- 結果：通過。許氏「你好」＋`Jqvx`＋Esc → 你好；標準 `hellojkq`＋Esc → hello；連按兩次 Esc → 清空；字尾無亂碼時按一次 Esc 不刪字。

## 7. 圖示與 Caps Lock 提醒
- 中／英圖示隨切換改變；全／半圖示隨切換改變。
- 按 Caps Lock：是否跳出提醒？目前還不確定桌面會不會把 Caps Lock 鍵送給輸入法，請記錄實際結果。目前設定 `capsLockAllowChineseInput=False`。
- 結果：Caps Lock 提醒通過：按 Caps Lock 跳出「ABC」通知，桌面確實把 Caps Lock 送給輸入法。Cinnamon 系統匣只顯示輸入法圖示，「中／英」以選單項目出現在小麥選單中（顯示「中」）；全／半圖示預設關閉，未測。

## 8. 學習到的英文字詞
- 前提：Mac 上打過兩次以上的英文字（`learning/english/`）已同步到 Mint 的用戶資料夾。
- 在 Mint 打該英文字的小寫，確認被修正成學到的大小寫。
- 結果：未測。
