# Phase 4 實機驗收（learning.4）

測試機：Linux Mint 22.3（Cinnamon，X11，fcitx5 5.1.7）。套件：`fcitx5-mcbopomofo-smart-learning_3.1.1-learning.4_amd64.deb`（apt 自動帶入 Qt6 相依）。
2026-11-24 以 xdotool 加截圖實測，許氏鍵盤。用戶資料夾為 Synology Drive 同步資料夾，測試前整個備份，測完只扣掉測試加入的內容，再與備份比對確認一致。

## 1. 管理詞彙視窗
- 以選單相同的參數開啟（`mcbopomofo-vocabulary --data-folder … --device-id …`）：視窗標題「管理詞彙」，六個分頁依序為中文詞彙、排除詞彙、英文詞彙、常用句、暫存詞、文字範本，介面為中文。
- 中文詞彙：讀到 `data.txt` 的 9 筆。打「測驗詞」自動帶出「ㄘㄜˋ-ㄧㄢˋ-ㄘˊ」，按「新增」後 `data.txt` 多一行，權限維持 644。輸入法不用切換焦點，馬上打得出「測驗詞」（許氏 `agjemjad`）。刪除後 `data.txt` 與原本完全相同。
- 英文詞彙：新增 `quokkaz`，本機英文檔寫入次數 2。接著在輸入法打英文讓它存檔，`quokkaz` 仍在（兩個程式的修改有合併）。刪除後本機檔移除，`learning/removed.txt` 記上 `english	quokkaz	<時間>`。
- 常用句、暫存詞：列出包含其他裝置（Mac）同步來的資料。
- 文字範本：列出用戶資料夾的範本。
- 結果：通過。

## 2. 選單
- 系統匣右鍵選單有：注音－標準鍵盤／許氏鍵盤、全形標點、聯想詞、編輯使用者詞彙、編輯排除的詞彙、管理詞彙…、編輯文字範本、編輯符號表、使用說明、關於小麥注音…。
- 結果：通過。

## 3. 使用說明與關於
- `xdg-open file:///usr/share/fcitx5-mcbopomofo/user-guide.html#about` 以預設瀏覽器（Chrome）開啟，直接跳到「關於小麥注音」一節，版本顯示 3.1.1-learning.4。
- 結果：通過。

## 4. 候選字面板主題
- 暫時把 classicui 主題設為 `mcbopomofo-light`、`mcbopomofo-dark`，候選字窗分別為淺色與深色，反白與字色正常。測完還原為預設主題。
- 結果：通過。使用者可在「Fcitx 5 設定 → 附加元件 → 經典使用者介面 → 主題」選用。

## 5. 浮動組字窗
- 把 `xed` 加入「浮動組字窗」程式清單後，xed 仍顯示內嵌組字。
- 原因：Mint Cinnamon 的 GTK 程式預設經由 fcitx5 的 ibus 相容介面連線（工作階段沒有設定 `GTK_IM_MODULE`），這條路徑不會回報程式名稱，fcitx 的除錯資訊顯示 `program:`（空白）`frontend:ibus`。程式名稱是空的時候，依設計不套用浮動組字窗，選單也不顯示這一項。
- 要使用此功能，需要讓 GTK 程式改走 fcitx 自己的介面（已安裝 `fcitx5-frontend-gtk3`）：在登入環境設定 `GTK_IM_MODULE=fcitx`（例如寫進 `~/.xprofile` 或 `/etc/environment`），登出再登入。
- 結果：功能本身未能在本機驗證，待使用者決定是否調整登入環境後再測。

## 6. 發現的問題
- 許氏鍵盤打 `q` 開頭的英文字（`q` 不是許氏的注音鍵）按 Tab 沒有英文聯想；`ass` 等其他字正常。列為後續修正。

## 測後清理
- 關閉詞彙管理員，`fcitx5-remote -e` 停止 fcitx5。
- 刪掉本機英文檔中測試加入的 3 筆（含打錯鍵產生的兩個字）與測試建立的 `learning/removed.txt`；`data.txt` 已由詞彙管理員刪除還原。
- 與測試前備份比對完全一致，fcitx 設定檔與 classicui 設定也還原，重新啟動 fcitx5。
