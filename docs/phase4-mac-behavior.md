# Phase 4 偵察：macOS 管理詞彙視窗、浮動組字窗、面板主題、使用說明與「關於」行為規格

來源：Mac fork `/Users/roger/Developer/McBopomofo` @ `7446fd0`。目標：fcitx5 port `/Users/roger/Developer/fcitx5-mcbopomofo`（branch `smart-learning` @ `b3fdc17`）。設計依據：Mac repo `docs/superpowers/specs/2026-11-23-linux-fcitx5-port-design.md` 的「階段 4：視窗與周邊」與「留給實作者決定的事」。

Mac 端縮寫（行號皆為 `7446fd0`）：

- `VM` = `Source/VocabularyManager.swift`
- `FC` = `Source/FloatingComposition.swift`
- `IMC` = `Source/InputMethodController.swift`
- `AD` = `Source/AppDelegate.swift`
- `PF` = `Source/Preferences.swift`
- `PV` = `Source/PreferencesUI/PreferencesView.swift`，`PM` = `Source/PreferencesUI/PreferencesModel.swift`
- `LMM` = `Source/LanguageModelManager.mm`
- `CS` = `Source/CommonSentences.swift`（含 `RemovedEntries`）
- `EW` = `Source/EnglishWords.swift`
- `SP` = `Source/ServiceProvider.swift`
- `VCC` = `Packages/CandidateUI/Sources/CandidateUI/VerticalCandidateController.swift`，`HCC` = 同目錄 `HorizontalCandidateController.swift`
- `UG` = `Source/UserGuide.html`
- `CR` = `Source/zh-Hant.lproj/Credits.rtf`，`IPS` = `Source/zh-Hant.lproj/InfoPlist.strings`
- `L10N` = `Source/zh-Hant.lproj/Localizable.strings`
- `CL` = `CHANGELOG.md`

fcitx 端縮寫（行號為 `b3fdc17`）：

- `fMB` = `src/McBopomofo.cpp`，`fMBh` = `src/McBopomofo.h`
- `fLML` = `src/LanguageModelLoader.cpp`
- `fSL/` = `src/SmartLearning/`
- `FF` = `docs/file-formats.md`
- fcitx5 標頭與函式庫：建置映像 `mcbpmf-u2404`（Ubuntu 24.04）內的 fcitx5 **5.1.7**（`/usr/include/Fcitx5/Core/fcitx/*.h`、`libFcitx5Core.so.5.1.7`、`libclassicui.so`、`/usr/share/fcitx5/themes/`）。該映像**沒有安裝 Qt6 開發套件**。

無法從原始碼確定的事項標 **UNKNOWN**，需要使用者決定的標 **決策**，兩者都收在 §8。

---

## 0. 結論摘要

1. **管理詞彙**：Mac 視窗跑在輸入法同一個程序裡，直接改輸入法記憶體中的資料；Linux 是獨立的 Qt6 程式，**兩個程序會同時持有「本機裝置檔」**（`learning/english/`、`learning/sentences/`、`learning/temporary/` 的本機檔）。只照 Mac 的寫法做，輸入法下次存檔會把詞彙管理員的「加入」蓋掉（§1.5）。必須在共用模組補上「本機檔被外部改動時合併」的規則，或改用 IPC。這是本階段最大的設計點。
2. **浮動組字窗**：fcitx5 5.1.7 有現成機制。`InputContext::capabilityFlags()` 在 preedit 被停用時會去掉 `Preedit|FormattedPreedit`（已反組譯確認，§2.2），fcitx port 現有的三處 `useClientPreedit` 判斷本來就會改走 `setPreedit`（組字顯示在 fcitx 面板，送出時才 commit）。Linux 只要在這三處多判斷 `InputContext::program()` 是否在清單內即可。和 Mac 的差異是位置與字級（§2.4），另外 `program()` 可能是空字串（UNKNOWN）。
3. **面板主題**：輸入法無法替單一輸入法指定 classicui 主題或字型，只能**隨套件提供主題檔**（`/usr/share/fcitx5/themes/<名稱>/theme.conf`），使用者在 classicui 設定裡選用。GNOME Wayland 走 kimpanel 擴充套件時，主題完全不適用。
4. **使用說明與關於**：`UG` 可以大致沿用，但要改寫安裝、詞庫位置、快速鍵、模式圖示、浮動組字窗幾節，並刪除「安全輸入」一節。fcitx 沒有「關於」面板，建議在使用說明加一節「關於」，選單用 `xdg-open` 開到該節。

---

## 1. 管理詞彙視窗

### 1.1 入口與視窗（Mac）

- 選單項目「管理詞彙…」（`IMC:154-156`，`L10N:339`），只在小麥注音模式顯示（它放在 `if inputMode == .plainBopomofo {…} else {…}` 之後，所以**兩個模式都會顯示**；`IMC:132-156`）。
- `showVocabularyManager` → `AppDelegate.showVocabularyManager()`，視窗控制器只建立一次，之後重複使用（`IMC:342-344`、`AD:223-230`）。
- 視窗標題「管理詞彙」（`VM:514`、`L10N:336`），可調大小、可最小化，初始大小 760×600，置中（`VM:510-520`）；內容最小 720×560（`VM:455`）。
- `showAndActivate`：每次開啟都先 `reload()`，再顯示並把 App 帶到前景（`VM:527-532`）。`onAppear` 也會 `reload()`（`VM:456`）。

### 1.2 共通介面行為（Mac）

由上而下（`VM:394-457`）：

1. **分頁列**：segmented picker，依序「中文詞彙、排除詞彙、英文詞彙、常用句、暫存詞、文字範本」（`VM:32-46`、`L10N:296-300`、`L10N:126`）。換分頁時：取消編輯、清空選取、`reload()`（`VM:99-105`）。
2. **說明文字**：各分頁一段（`VM:48-63`，中文見 `L10N:301-306`）。
3. **搜尋欄**「搜尋」與筆數「%d 筆」（`VM:409-415`、`L10N:328-329`）。過濾規則：關鍵字去頭尾空白後若為空就全列；否則任一欄 `localizedCaseInsensitiveContains` 關鍵字即保留（`VM:123-131`）。筆數是過濾後的筆數。
4. **表頭與清單**：欄名見各分頁（`VM:65-73`）。可多選（`List(selection:)`，`VM:418`）。第 0 欄與「寬欄」可伸展，其他欄寬 110（`VM:420-426`、`VM:459-463`；寬欄 = 中文詞彙／排除詞彙／文字範本的第 1 欄）。單行顯示，過長截斷（`lineLimit(1)`）。
5. **編輯列**（只有 `fieldTitles` 非空的分頁才有，暫存詞沒有，`VM:432-434`）：一或兩個輸入框，欄名同 `fieldTitles`（`VM:75-84`）。按鈕「新增」（編輯中為「儲存」），是預設按鈕（Enter 觸發）；編輯中多一個「取消」（`VM:479-503`、`L10N:334-335`、`L10N:129`）。
6. **按鈕列**（`VM:436-452`）：
   - 「編輯」：只有可編輯的分頁（中文詞彙、排除詞彙、文字範本，`VM:86-88`）才顯示；恰好選一筆時才可按。
   - 「刪除」：有選取才可按。
   - 「加入中文詞彙」：只在暫存詞分頁，有選取才可按（`L10N:332`）。
   - 訊息文字（最多兩行）。
   - 「在 Finder 中顯示」（`L10N:333`）。
7. **編輯**：`beginEditing` 把該列第 0、1 欄填進兩個輸入框，標記 `readingEdited = true`（之後不自動帶注音），記下 `editingKey = row.key`（`VM:228-241`）。
8. **自動帶注音**：只在中文詞彙／排除詞彙分頁，第一欄（詞）每次改變時，若使用者沒有手動改過注音欄，就把注音欄設為 `extractReading(詞)`（詞為空則清空）（`VM:109-113`、`VM:212-226`）。手動改注音欄會設 `readingEdited = true`（`VM:486-494`）。`cancelEditing` 清空兩欄並重設旗標（`VM:243-250`）。
9. **`extractReading`**（`SP:50-83`）：從頭貪婪比對最長的詞：對每個候選子字串先做簡轉繁（OpenCC），再查 `LanguageModelManager.reading(for:)`；查不到就縮短一個字再試；單字也查不到就輸出「？」並前進一字。各段以 `-` 連接。`reading(for:)` 只查**內建詞庫**（`McBopomofoLM::getReading` → `languageModel_.getReadings`，取分數最高的讀音；`LMM:1025-1033`，fcitx 引擎 `src/Engine/McBopomofoLM.cpp:424-436` 相同），不查使用者詞。
10. **送出 `commit`**（`VM:253-302`）：兩欄都去頭尾空白；第一欄為空 → 響一聲結束。各分頁驗證見 §1.3。成功後訊息為「已加入「%@」。」（新增）或「已儲存。」（編輯），然後取消編輯、`reload()`（`VM:299-301`、`L10N:324-325`）。
11. **刪除 `deleteSelected`**（`VM:304-328`）：選取的列依清單順序取 key；沒有選取 → 響一聲。刪除後訊息「已刪除 %d 筆。」（`L10N:326`），清空選取、取消編輯、`reload()`。**沒有確認對話框。**
12. **`revealFile`**（`VM:346-355`）：中文詞彙 → `data.txt`，排除詞彙 → `exclude-phrases.txt`，文字範本 → `text-templates.txt`，其他三個分頁 → `learning/` 資料夾；在 Finder 選取該項目。
13. **重新整理**：只在開啟視窗、換分頁、每次新增／儲存／刪除／加入之後 `reload()`。**視窗開著時不監看檔案**，其他裝置同步進來的變動要換分頁或重開才看得到。
14. `reload()` 之後，選取集合只保留仍存在的 id（`VM:188`）。

### 1.3 六個分頁

#### 1.3.1 中文詞彙（`.phrases`）

- **檔案**：`<資料夾>/data.txt`（`LMM:630-633`）。只有小麥注音模式的檔，**不管** `data-plain-bpmf.txt`。
- **讀取**（`VM:191-208`）：以 `\n` 切行；去掉「去頭尾空白後為空」與「以 `#` 開頭」的行（注意：只檢查行首就是 `#`，前面有空白的 `#` 行不算註解）。每行以**單一空格**切一次（`maxSplits: 1`，略過空段），恰好兩段才列出：第 0 欄詞、第 1 欄注音。以 tab 分隔的行切不出兩段，不列出。
- **列的 id／key**：id = `"<序號>\t<原始行>"`，key = 原始行（`VM:205-206`）。同一行重複出現會是兩列。
- **順序**：檔案順序。
- **欄**：「詞」「注音」（`L10N:307-308`）。
- **新增／儲存**（`VM:261-275`）：
  - 注音為空、注音含全形「？」、或詞含空格 → 訊息「請輸入注音，字與字之間用「-」隔開，例如 ㄒㄧㄠˇ-ㄇㄞˋ。」（`L10N:319`）。**不檢查注音是否合法。**
  - 新的一行 = `"詞 注音"`。新增（非編輯）時若清單已有完全相同的 key → 訊息「「%@」已經在清單裡。」（`L10N:320`）。編輯時不檢查重複。
  - 寫檔見 §1.4 `write`：編輯時把第一個等於舊 key 的行換成新行；找不到舊行或新增時，附加到檔尾。
  - **只動 data.txt**：不會從 `exclude-phrases.txt` 移除（和輸入法的 `writeUserPhrase` 不同，`LMM:427-446`）。
- **刪除**：§1.4 `remove`，只刪 data.txt 的行，**不會**寫入 exclude。
- **其他裝置**：data.txt 是共用的單一檔，沒有裝置之分。

#### 1.3.2 排除詞彙（`.excluded`）

- **檔案**：`<資料夾>/exclude-phrases.txt`（`LMM:640-643`），不含純注音模式的排除檔。
- 讀取、欄、驗證、新增、編輯、刪除完全同 §1.3.1，只是換成這個檔。只動這個檔，不碰 data.txt。

#### 1.3.3 英文詞彙（`.english`）

- **資料**：`EnglishWords.allWords()`（`EW:191-197`）：強制重新載入（`lastLoaded = distantPast`），合併本機檔與 `learning/english/` 內其他 `.txt`，同字次數**相加**，濾掉 `removed.txt` 中 kind = `english` 有記錄的字（`EW:249-271`）。依小寫字母序排列。
- **其他裝置**：只顯示合併後的總次數，不區分裝置。
- **id／key**：字本身（`VM:142-144`）。欄：「英文字」「次數」（`L10N:309-310`）。
- **新增**（`VM:284-289`）：字長至少 2，且全部是 ASCII 字母，否則訊息「英文字至少要兩個字母。」（`L10N:322`）。注意輸入法自動記錄的最短長度是 3（`FF` 英文字詞一節），手動加入允許 2。
  - `EnglishWords.add`（`EW:199-207`）：先從 `removed.txt` **移除**該字的記錄（`CS:54-59`），再把**本機**次數設為 `max(原本機次數, 2)`，存本機檔。
- **刪除**：`EnglishWords.remove`（`EW:209-215`）：從**本機**次數移除、存本機檔，再在 `removed.txt` **加入** `english\t<字>\t<現在>`（`CS:48-52`）。其他裝置的檔案不動；其他裝置（包括 Mac）讀到記錄就濾掉（「有記錄即刪除」）。
- **不可編輯**。
- 大小寫：key 是原樣的字，`removed.txt` 比對也是原樣（區分大小寫）。

#### 1.3.4 常用句（`.sentences`）

- **資料**：`CommonSentences.allEntries()`（`CS:224-234`）：強制 `loadFiles()`（本機檔只在第一次讀），合併其他裝置：次數相加、最後使用時間取大、釘選取聯集（`CS:287-305`）。只列 `isAlive` 的句子：`removed.txt` 中 kind = `sentence` 的刪除時間 >= 最後使用時間就算刪除；未釘選且超過保存期限也不列（`CS:158-163`）。排序：次數多的在前，同次數最後使用較新的在前。
- **注意**：列表**包含還沒累積到 3 次的句子**（建議時才要求 3 次）。
- **欄**：「句子」「次數」「狀態」（`L10N:311-312`）。狀態（`VM:146-159`）：釘選 → 「手動加入」；次數 >= 3 → 「已記住」；否則「還差 %d 次」（`L10N:316-318`）。
- **新增**（`VM:290-295`）：`CommonSentences.isValid`（6 到 40 字、英文字或數字串算一字、不含標點符號換行 tab，`CS:140-149`），否則訊息「句子要 6 到 40 個字（英文單字算一個），不能有標點。」（`L10N:323`）。
  - `CommonSentences.add`（`CS:236-249`）：從 `removed.txt` 移除該句記錄；本機項目 `count = max(count, 3)`、`lastUsed = 現在`、`pinned = true`；存本機檔；重新合併。
- **刪除**：`CommonSentences.remove`（`CS:251-257`）：刪掉本機項目、存本機檔，在 `removed.txt` 加入 `sentence\t<句子>\t<現在>`。其他裝置的項目因「刪除時間 >= 最後使用時間」而隱藏；那台裝置之後再用到這句（`lastUsed` 變新），它會再出現（帶著那台的舊次數）。
- **不可編輯**。
- 即使偏好「常用句」關閉，管理員仍可列出、加入、刪除（`allEntries`／`add`／`remove` 不檢查 `commonSentencesEnabled`，只有 `record` 與 `completion` 檢查，`CS:166-169`、`CS:193-196`）。

#### 1.3.5 暫存詞（`.temporary`）

- **資料**：`LanguageModelManager.allTemporaryPhrases()`（`LMM:932-937`）→ `_reloadTemporaryPhrases`（`LMM:856-886`）：本機與其他裝置合併，同鍵取較新時間；超過 30 天的行讀檔時就丟掉（`LMM:837-854`）；`removed.txt` kind = `temporary` 的刪除時間 >= 該筆時間則不列。依時間新到舊排序（`VM:163-164`）。
- 鍵是 `"值 讀音"`，以空格切成恰好兩段才列出（`VM:166-169`）。
- **欄**：「詞」「注音」「選取日期」，日期格式 `yyyy-MM-dd`（本地時區，`VM:161-162`、`VM:170-173`、`L10N:313`）。
- **沒有編輯列**，不能新增、不能編輯。
- **刪除**：`forgetTemporaryPhrase`（`LMM:939-947`）：`removed.txt` 加入 `temporary\t<值 讀音>\t<現在>`，移除本機與合併的項目，更新語言模型，存本機檔。
- **加入中文詞彙**（`VM:330-344`）：每個選取的鍵先 `writeUserPhrase`（從 `exclude-phrases.txt` 移除同一行，data.txt 已有就不寫，否則附加到檔尾；`LMM:427-446`），再 `forgetTemporaryPhrase`。訊息「已將 %d 筆加入中文詞彙。」（`L10N:327`）。沒有選取 → 響一聲。**這條路徑會改 exclude，和 §1.3.1 的「新增」不同。**

#### 1.3.6 文字範本（`.templates`）

- **檔案**：`<資料夾>/text-templates.txt`（`LMM:568-571`）。
- **讀取**（`VM:175-186`）：同 §1.3.1 的 `lines()`（略過空行、`#` 開頭行），以**第一個 tab** 切一次（保留空段），恰好兩段才列出。內容欄顯示**原樣**（`\n` 不展開）。注意這和輸入法解析（`FF`：代碼去頭尾空白、代碼或內容為空就略過）不完全相同：管理員會列出代碼為空的行。
- **id／key**：同 §1.3.1。欄：「代碼」「內容」（`L10N:314-315`）。
- **新增／儲存**（`VM:276-283`）：內容為空、或代碼或內容含 tab → 訊息「請輸入代碼和內容。」（`L10N:321`）。**不檢查重複。** 新的一行 = `"代碼\t內容"`，寫檔同 §1.4 `write`。
- **刪除**：§1.4 `remove`。
- 說明文字提到「內容要換行請寫 \n」（`L10N:306`），使用者在內容欄自己輸入 `\n` 兩個字元。

### 1.4 單一共寫檔的寫法（Mac）

`VM:359-388`，用於 data.txt、exclude-phrases.txt、text-templates.txt：

- `write(line, replacing: oldLine, path)`：讀整個檔（讀不到當作空字串）。以 `\n` 切行（保留空行與註解）；若有 `oldLine` 且找得到**第一個完全相同**的行，就換掉；否則附加：內容非空且最後不是 `\n` 時先補 `\n`，再加 `line + "\n"`。整份 atomic 覆寫。**讀不到檔時也會寫**（等於建立只有這一行的新檔）。
- `remove(lines, path)`：讀不到檔就什麼都不做。以 `\n` 切行，每個要刪的行只刪**第一個**完全相同的行（同一行出現兩次、選了一列，只刪一行），其餘行原樣保留，以 `\n` 接回後 atomic 覆寫。
- 比對用的是**原始行**（含行尾空白、`\r`）。
- 寫入後輸入法由 FSEvents 監看資料夾自動重新載入（`AD:331-355`）：`learning/` 以外的變動重新載入使用者詞與替換表；`learning/` 內的變動重新載入選字記憶與暫存詞。

### 1.5 跨程序問題：Linux 必須處理，Mac 沒有

Mac 的詞彙管理員跑在輸入法程序裡，`EnglishWords`、`CommonSentences`、暫存詞都是同一份**記憶體中的靜態資料**，改完就生效，也不會被輸入法自己的存檔蓋掉。Linux 的 Qt 程式是另一個程序，各自建一份 store，下列情況會出錯：

1. **英文字「加入」被蓋掉**：管理員把本機檔的字設成次數 2 後存檔；輸入法記憶體中的 `localCounts_` 沒有這個字，下次 `EnglishWordStore::save()`（打完英文 2 秒後、或 `deactivate`，`fMB:749-770`、`fMB:921-935`、`fMB:1164-1170`）整份覆寫本機檔，加入就消失了。`removed.txt` 的記錄已被移除，所以字只是變回「沒有加入」。
2. **常用句「加入」（釘選）被蓋掉**：同上，`CommonSentenceStore` 的 `localEntries_` 只在第一次載入本機檔（`fSL/CommonSentences.h` `loadLocalFileIfNeeded`）。
3. **「刪除」後次數復活**：管理員刪掉本機項目，但輸入法記憶體裡還有，下次存檔又寫回去。英文字因為「有記錄即刪除」所以仍隱藏；常用句只要再用一次（`lastUsed` 晚於刪除時間），就會帶著**舊次數**立刻復活。Mac 刪除後本機次數歸零，要重新累積三次。暫存詞不受影響：`TemporaryPhraseStore::merge` 會丟掉本機中刪除時間 >= 該筆時間的項目（`fSL/TemporaryPhrases.cpp:325-330`），下次存檔就清掉。
4. **輸入法晚 60 秒才看到 removed.txt**：`RemovedEntries` 快取 60 秒，`EnglishWordStore`、`CommonSentenceStore` 也是每 60 秒才重新合併（`fSL/RemovedEntries.h`、`fSL/EnglishWords.h` `kReloadInterval`、`fSL/CommonSentences.h` `kReloadInterval`）。只有 `TemporaryPhraseStore::reloadIfChanged` 會偵測 removed.txt 的 stamp 並立即 `reload()`（`fSL/TemporaryPhrases.cpp:146-170`）。
5. data.txt、exclude-phrases.txt、text-templates.txt 沒有這個問題：輸入法每次寫入前都重讀（`UserPhraseFile`，`fSL/UserPhraseFile.h:18-21`），讀取靠 stamp（`fLML:209-226`、`TextTemplateStore::kRecheckInterval = 2`）。removed.txt 只有詞彙管理員會寫，輸入法只讀。

**建議做法（決策 D1，推薦 A）**：

- **A. 檔案層級合併（不需要 IPC）**。在共用模組加一條規則，Mac 現有格式與行為不變：
  1. `EnglishWordStore`、`CommonSentenceStore`、`TemporaryPhraseStore` 記住本機檔最後一次由自己載入或寫入時的 stamp（`AtomicFile.h` 的 `FileStamp`）。
  2. 在 `reloadIfChanged`／`reloadIfNeeded` 以及 `save()` 寫檔**之前**，若本機檔 stamp 不同，就重讀本機檔，和記憶體中的本機資料**逐鍵合併**：英文字次數取大；常用句次數取大、`lastUsed` 取大、`pinned` 取聯集；暫存詞時間取大。
  3. 刪除**只看 removed.txt**，不看「檔案中消失」（符合設計文件「刪除以 removed.txt／exclude 的紀錄為準」）。所以合併時也要丟掉本機中已被刪除的項目：英文字有記錄就丟、常用句刪除時間 >= `lastUsed` 就丟（暫存詞已經這樣做）。這同時修好第 3 點。
  4. 輸入法啟用時（`activate`，`fMB:1142-1157`）若 removed.txt 的 stamp 變了，`removedEntries_->reload()`，並強制英文字與常用句重新合併（不等 60 秒）。
  5. 詞彙管理員每次寫完，用 D-Bus 呼叫 `org.fcitx.Fcitx.Controller1.ReloadAddonConfig("mcbopomofo")`（5.1.7 `libdbus.so` 內有此方法名）。輸入法的 `reloadConfig()`（`fMB:948-951`）除了讀設定，也強制所有 store 重新檢查。fcitx 沒在跑時呼叫失敗，忽略即可，檔案下次啟動時就會讀到。
  - 已知的小風險：兩個程序在同一秒內寫同一個本機檔時，後寫的一方可能沒看到另一方的修改。使用者不會一邊在管理員加詞、一邊在別的程式打字，可以接受（同設計文件對單一共寫檔的判斷）。
- **B. IPC**：輸入法以 fcitx 的 dbus 模組匯出方法（例如 `AddEnglishWord`、`RemoveSentence`），管理員呼叫，由輸入法程序裡的 store 執行，行為和 Mac 完全一樣；fcitx 沒在跑時，管理員退回直接寫檔。缺點是要維護兩條路徑，在 fcitx 外也難以測試。

### 1.6 Linux 架構

- **程式**：`mcbopomofo-vocabulary`（暫定名，決策 D2），Qt6 Widgets（Ubuntu 24.04／Mint 22 的 `qt6-base`），原始碼放 `src/VocabularyManager/`，CMake 選項 `ENABLE_VOCABULARY_MANAGER`（預設開，找不到 Qt6 就關）。建置映像要加裝 `qt6-base-dev`（目前沒有）。`.deb` 加 `libqt6widgets6` 相依。
- **不得重寫格式**：連結 `SmartLearningLib`（已連 `McBopomofoLMLib`，`fSL/CMakeLists.txt`），所有讀寫都透過共用模組。Qt 程式**不 include fcitx 標頭、不連 Fcitx5Core**（設定檔自己用 INI 讀，見下）。
- **開啟方式**：`fMB` 新增 `SimpleAction`「管理詞彙…」，`fcitx::startProcess({"mcbopomofo-vocabulary", "--data-folder", <userDataPath>, "--device-id", <deviceID>})`，同 `editUserPhrasesAction_` 的寫法（`fMB:712-731`）。也要能從桌面選單單獨開啟（`.desktop` 檔）：沒有參數時讀 `~/.config/fcitx5/conf/mcbopomofo.conf` 的 `UserDataFolder`、`DeviceID`（`fMBh:297-303`，依 `UserDataFolder.h` `resolveUserDataFolder` 解析；預設資料夾 `~/.local/share/fcitx5/mcbopomofo`，`fLML:106-120`）。`DeviceID` 為空（輸入法從沒跑過）時，**唯讀開啟**，不寫任何本機裝置檔（決策 D3）。
- **裝置檔名**：必須和輸入法完全相同 `learningFilename(deviceID, sanitizeDeviceName(gethostname()))`（`fMB:502-530`），否則管理員會寫出「第三台裝置」的檔。
- **單一視窗**：再次從選單開啟時，帶到前景並 `reload()`，不開第二個視窗（Mac `AD:223-230`）。用 `QLocalServer` 或 D-Bus 唯一名稱（決策 D2）。
- **注音自動帶出**：用 `McBopomofoLM::loadLanguageModel(<內建 data 檔>)` + `getReading`，路徑同輸入法 `data/mcbopomofo-data.txt`（`fLML:41`，安裝在 `${FCITX_INSTALL_PKGDATADIR}/data/`，即 `/usr/share/fcitx5/data/`）。演算法照 §1.2 第 9 點移到共用模組（例如 `fSL/ReadingExtraction`），附測試。Mac 的簡轉繁一步需要 OpenCC（UNKNOWN U4／決策 D4）。
- **「在 Finder 中顯示」** → 「在檔案管理員中顯示」：D-Bus `org.freedesktop.FileManager1.ShowItems`，失敗時 `xdg-open` 上層資料夾。
- **字串**：沿用 `L10N:296-336` 的中文；fcitx port 用 gettext（`po/`），Qt 程式用 `QObject::tr` 或共用 gettext（決策 D5）。

需要在共用模組新增的 API（目前缺少）：

1. `RemovedEntries::add(kind, key, wallNow)`、`remove(kind, key)`：重讀、修改、輸出前依行排序、時間取整數秒、atomic 覆寫（`CS:48-59`、`CS:80-85`）。現在的建構子吃的是 steady clock（`fMB:518-519`），寫入要另外傳牆鐘時間。檔案讀不到時（存在但無法讀）不寫。
2. `EnglishWordStore::allWords()`（強制重新合併、濾掉已刪除、依小寫排序）、`add(word)`、`remove(word)`，行為照 `EW:191-215`。
3. `CommonSentenceStore::allEntries()`、`add(sentence)`、`remove(sentence)`，照 `CS:224-257`。
4. `TemporaryPhraseStore::allPhrases()`（含時間，先 `reload()`）、`forget(value, reading)` = removed.txt 加記錄 + 移除本機項目 + 存檔（`LMM:939-947`）。現有的 `remove` 是轉正時用的，**不寫** removed.txt，不能拿來當管理員的刪除。
5. 管理員用的「單一行」編輯：`replaceOrAppendLine(path, line, oldLine)`、`removeLines(path, lines)`，照 §1.4（只動一個檔）。「加入中文詞彙」改用現有的 `UserPhraseFile::write`（從 exclude 移除再附加到 data.txt，已有就不寫），它和 Mac `writeUserPhrase` 相同（`fSL/UserPhraseFile.h:48-51`）。
   - Linux 的安全規則和 Mac 有一處不同：Mac `write` 讀不到檔時會直接建立新檔；Linux 若在同步資料夾還沒同步完成時這樣做，會產生空的 data.txt 蓋掉同步進來的檔。建議：**檔案不存在時不寫**，訊息顯示「找不到 data.txt，詞庫資料夾可能還在同步」（決策 D6）。
6. §1.5 A 的本機檔外部變動合併。

### 1.7 Linux 新增、Mac 沒有的行為（皆為決策 D7，建議都做）

- 視窗重新取得焦點時（`QEvent::WindowActivate`）重新 `reload()` 目前分頁。Mac 不做，但 Linux 的輸入法與管理員是兩個程序，資料較容易在視窗開著時改變。不加 `QFileSystemWatcher`（同步工具整批寫入時會一直觸發）。
- 刪除仍**不跳確認**（照 Mac）。

### 1.8 測試

- Mac 沒有詞彙管理員的測試（`McBopomofoTests/` 內無 `VocabularyManager`）。
- Linux 建議：共用模組新 API 的 GoogleTest（removed.txt 寫入排序與整數時間、英文字加入／刪除對 removed.txt 的影響、常用句釘選、暫存詞 forget、單一行編輯保留註解與順序、重複行只刪一行、本機檔外部變動合併）。Qt 介面不寫自動測試，實機驗收照 `docs/acceptance-phase3.md` 的格式。

---

## 2. 浮動組字窗

### 2.1 Mac 行為

- **用途**（`FC:27-30`、`UG:303-307`）：有些 App 會自己改動組字中的文字，造成漏字或補字重複。對這些 App，組字內容改顯示在游標上方的小視窗，App 只收到送出的文字。
- **設定**：`FloatingCompositionBundleIDs`，字串陣列，預設空（`PF:46`、`PF:378-382`）。**偏好設定視窗沒有這個選項**（`PV` 無），只能從選單切換。
- **選單**：「此 App 使用浮動組字窗」（`L10N:340`），只在目前 client 的 bundle ID 非空時顯示，勾選狀態 = 是否在清單中（`IMC:175-181`）。點選（`IMC:346-374`）：加入或移出清單，通知「「%@」已使用浮動組字窗」／「「%@」已停用浮動組字窗」（`L10N:341-342`，%@ 為 App 名稱，找不到時用 bundle ID），然後 `keyHandler.clear()` 並進入 Empty 狀態。Mac 的 Empty 狀態處理會先 commit 前一個狀態的組字內容（`IMC:631-642`）。
- **`setMarkedText` 包裝**（`IMC:580-597`）：所有狀態都經過它。不在清單 → 隱藏浮動窗、正常 `setMarkedText`。在清單 → 對 App `setMarkedText("")`，把文字與游標位置交給浮動窗。
- **浮動窗**（`FC:31-125`）：無邊框、不搶焦點、不吃滑鼠、彈出選單層級、所有桌面空間都顯示；popover 材質、圓角 6、內距 6。字級 = `clamp(CandidateListTextSize, 14, 24)`（使用者設定 64 → 24）；文字加底線，游標位置插入強調色的「|」。位置在游標所在行的**上方** 2pt（避開下方的候選字窗），超出螢幕頂端改放到行的下方；水平方向夾在螢幕可見範圍內。空字串就隱藏。
- **Deactivated** 時隱藏（`IMC:599-601`）。

### 2.2 fcitx5 5.1.7 能做到的（已在建置映像確認）

- `InputContext::program()`：「程式名稱，依應用程式可能是空的」（`fcitx/inputcontext.h:76-78`）。
- `InputContext::setEnablePreedit(bool)`／`isPreeditEnabled()`：「覆寫 client 的 preedit 提示」（`inputcontext.h:133-136`）。全域設定 `GlobalConfig::preeditEnabledByDefault()` 與切換鍵 `togglePreeditKeys()`（`globalconfig.h:37`、`:45`）。
- **反組譯 `libFcitx5Core.so.5.1.7`**：`InputContext::capabilityFlags()` 在 preedit 被停用時回傳 `flags & ~0x12`，也就是去掉 `Preedit (1<<1)` 與 `FormattedPreedit (1<<4)`；`setEnablePreedit` 只有在旗標實際改變時才送出能力變更事件。
- fcitx port 現有程式：`useClientPreedit = capabilityFlags().test(Preedit)`，為真時 `setClientPreedit`（嵌在 App 裡），否則 `setPreedit`（畫在 fcitx 面板的組字列），共三處（`fMB:1196`、`fMB:2679`、`fMB:2741`）。所以 **preedit 被停用的 IC，現在就已經是「組字顯示在面板、送出才交給 App」**，跟 Mac 浮動組字窗的效果相同。
- `InputPanel::setAuxUp/AuxDown`：可放提示文字（`fMB:2765` 用 AuxDown 放 tooltip）。

### 2.3 Linux 對應方案

1. 設定：`fcitx::Option<std::vector<std::string>> floatingCompositionPrograms{this, "FloatingCompositionPrograms", _("Programs using the floating composition window"), {}}`，在 fcitx5 設定工具中可編輯（Mac 沒有在偏好設定顯示，Linux 顯示出來方便刪除已經不用的程式；決策 D8）。
2. 新增 `bool McBopomofoEngine::useClientPreedit(fcitx::InputContext*)` = `capabilityFlags().test(Preedit) && !isFloating(context->program())`，取代三處判斷。**不呼叫 `setEnablePreedit`**：那是使用者用 fcitx 全域切換鍵（預設 Ctrl+Alt+P）控制的狀態，輸入法去改會和使用者的操作打架。
3. 選單：`SimpleAction`「此程式使用浮動組字窗」，checkable，只在 `program()` 非空時加入 statusArea（`activate` 時依當前 IC 決定）。點選：切換清單、`safeSaveAsIni`、通知「「%s」已使用浮動組字窗」／「…已停用…」（%s 用 `program()`，Linux 沒有 App 顯示名稱可查）、清掉 App 裡現有的 client preedit（`setClientPreedit({})` + `updatePreedit`）。清 keyHandler 時要決定 commit 還是丟棄：Mac 會 commit，fcitx 的切換鍵盤配置則刻意丟棄（`fMB:988-996` `selectQuickLayout`）；建議比照 Mac commit（決策 D9）。
4. 比對：`program()` 完全相同（區分大小寫）。

### 2.4 和 Mac 的差異（要寫進使用說明）

- **位置**：fcitx classicui 把 server 端 preedit 畫在候選字面板最上面一列，面板在游標**下方**；Mac 是游標上方的獨立小窗，候選字窗另外在下方。沒有候選字時，classicui 面板只顯示組字列。
- **字級**：classicui 的組字列和候選字共用同一個主題字型（§3），沒辦法做到 Mac 的「14–24 夾住」。
- **游標**：classicui 在組字列畫游標線（不是強調色的「|」）。
- **辨識單位**：Mac 是 bundle ID（整個 App）；Linux 是 `program()`，由前端回報（UNKNOWN U1：GTK／Qt 的 fcitx 模組回報的是可執行檔名稱或 `g_get_prgname()`；XIM 與 Wayland text-input 前端可能是空字串，這時選單不出現）。Chrome 系瀏覽器在不同啟動方式下回報的名稱可能不同。
- **GNOME Wayland（kimpanel）**：組字列由 GNOME 擴充套件畫在它的面板裡，效果相同，外觀不同。

### 2.5 測試

- Mac 沒有測試。Linux：把「是否用 client preedit」寫成純函式（輸入：能力旗標中的 Preedit 位元、program、清單），GoogleTest 涵蓋空 program、在清單、不在清單、能力本來就沒有 Preedit。實機：在 xed 加入清單後打字，確認 App 內沒有底線文字、面板顯示組字、Enter 後才送出。

---

## 3. 候選字面板主題

### 3.1 Mac 行為

- `CandidatePanelTheme`：0 跟隨系統、1 淺色、2 深色，預設 0（`PF:38`、`PF:337-340`）；偏好設定「面板主題：跟隨系統／淺色／深色」（`PV:479-488`、`L10N:270-273`）。套用方式是設定候選窗的 `NSAppearance`（aqua／darkAqua／nil，`IMC:1056-1061`），所有候選字面板與分類符號表都適用（`CL:80`）。使用者目前的值是 **1（淺色）**（設計文件「已定的實作決策」）。
- `CandidateListTextSize`：預設 16、範圍 12–196（`PF:76-78`、`PF:155-180`）；偏好選項 12、14、16、18、24、32、64、96（`PM:214-222`）。使用者目前 **64**。文字範本狀態上限 32（`IMC:1017-1021`）。選字鍵字級 = `max(字級 / 2, 10)`（`IMC:40`、`IMC:1022`）。可另設候選字與選字鍵字型名稱（`PF:56-57`、`PF:425-429`）。
- 外觀（macOS 11 以後）：popover 材質的毛玻璃背景、圓角 4、陰影（`VCC:250-262`、`HCC:411-422`）；直式選字鍵欄的反白用 `selectedControlColor`，反白文字用 `selectedControlTextColor`，一般文字用 `labelColor`（`VCC:35-100`）；橫式反白用 `controlAccentColor`、白字（`HCC:240-272`）。

### 3.2 fcitx5 能做的

- 外觀由 UI 外掛決定，輸入法**只能**給排版方向（`CandidateLayoutHint`，fcitx port 已有 `CandidateLayout` 設定，`fMBh:147`）。字型、顏色、主題都是 classicui 的**全域**設定，套用到所有輸入法。
- classicui 設定（`~/.config/fcitx5/conf/classicui.conf`，5.1.7 `libclassicui.so` 內可見的鍵）：`Vertical Candidate List`、`WheelForPaging`、`MenuFont`、`TrayFont`、`TrayOutlineColor`、`TrayTextColor`、`PreferTextIcon`、`ShowLayoutNameInIcon`、`UseInputMethodLanguageToDisplayText`、`UseDarkTheme`（「Follow system light/dark color scheme」）、`UseAccentColor`（「Follow system accent color…」）、`PerScreenDPI`、`ForceWaylandDPI`、`EnableFractionalScale`；以及 `Theme`、`DarkTheme`、`Font`（UNKNOWN U5：這三個鍵名沒在 `strings` 結果中直接看到，依 fcitx5 原始碼慣例推定，實作前要在測試機的 `classicui.conf` 確認）。
- 主題檔：`/usr/share/fcitx5/themes/<名稱>/theme.conf`（系統，套件可安裝）或 `~/.local/share/fcitx5/themes/<名稱>/`（使用者）。映像內有 `default`、`default-dark`。可用鍵（同上 `strings`）：`[InputPanel]` 的 `Font`、`NormalColor`、`HighlightCandidateColor`、`HighlightColor`、`HighlightBackgroundColor`、`PageButtonAlignment`、`FullWidthHighlight`、`Spacing`；`[InputPanel/Background]` 與 `[InputPanel/Highlight]` 的 `Image`、`Color`、`BorderColor`、`BorderWidth`、`Margin`、`Overlay`、`Gravity`、`OverlayOffsetX/Y`、`OverlayClipMargin`、`HideOverlayIfOversize`；`[InputPanel/Blur]` 類的 `EnableBlur`、`BlurMargin`、`BlurMask`；`ShadowMargin`；`TextMargin`、`ContentMargin`；`[Metadata] ScaleWithDPI`；`[AccentColorField]`（哪些顏色跟系統強調色走）。
- 圓角與陰影：只能用 `Image`（PNG 九宮格，以 `Margin` 指定不拉伸的邊）做；純色背景是直角。

### 3.3 建議主題（決策 D10）

隨套件安裝兩個主題 `mcbopomofo-light`、`mcbopomofo-dark`（`data/themes/` → `${CMAKE_INSTALL_DATADIR}/fcitx5/themes/`），**不自動改使用者的 classicui 設定**（那是全域的，會影響其他輸入法），在使用說明教使用者選用：

- 對應 Mac「淺色」（使用者目前的值）：classicui `Theme=mcbopomofo-light`、`UseDarkTheme=False`。
- 對應「跟隨系統」：`Theme=mcbopomofo-light`、`DarkTheme=mcbopomofo-dark`、`UseDarkTheme=True`。
- 對應「深色」：`Theme=mcbopomofo-dark`、`UseDarkTheme=False`。
- `UseAccentColor=True`，主題的 `[AccentColorField]` 列入候選反白背景，反白跟著桌面強調色，接近 Mac 的 `controlAccentColor`。

建議的 `mcbopomofo-light/theme.conf` 草稿（顏色是依 macOS 淺色 popover 的近似值，UNKNOWN U6：要拿使用者 Mac 的截圖比對）：

```ini
[Metadata]
Name=McBopomofo Light
Name[zh_TW]=小麥注音淺色
Version=1
Author=McBopomofo Smart Learning
Description=Candidate panel close to the macOS look
ScaleWithDPI=True

[InputPanel]
NormalColor=#1d1d1f
HighlightCandidateColor=#ffffff
HighlightColor=#1d1d1f
HighlightBackgroundColor=#d0d0d0
PageButtonAlignment=Last Candidate
FullWidthHighlight=True

[InputPanel/TextMargin]
Left=8
Right=10
Top=4
Bottom=4

[InputPanel/ContentMargin]
Left=4
Right=4
Top=4
Bottom=4

[InputPanel/Background]
Image=panel.png
Color=#f4f4f4
BorderColor=#c8c8c8
BorderWidth=1

[InputPanel/Background/Margin]
Left=6
Right=6
Top=6
Bottom=6

[InputPanel/Highlight]
Color=#0a64d6

[InputPanel/Highlight/Margin]
Left=4
Right=4
Top=2
Bottom=2

[AccentColorField]
0=Input Panel Highlight Candidate Background
```

- `panel.png`：淺灰底、圓角 6、1px 邊框的九宮格圖；不提供圖時退回 `Color`（直角）。陰影用 `ShadowMargin` 搭配帶透明陰影的圖。
- 深色版：`NormalColor=#e8e8ea`、背景 `#2a2a2c`、邊框 `#3e3e40`、反白 `#0a64d6`（同樣跟強調色走）、`HighlightBackgroundColor=#505052`。
- **字級**：classicui `Font`（Pango 字型字串，單位 pt，以 96 DPI 換算）。Mac 的 64 是邏輯點（約等於 64 px），約 48 pt；建議 `Font="Noto Sans CJK TC 48"`。也可以寫在主題的 `[InputPanel] Font`，但那會讓使用者無法在 classicui 設定改字級，建議主題**不寫 Font**，由使用者設定（決策 D10）。

### 3.4 做不到的（記錄差異）

- 選字鍵用較小字級（Mac 為字級一半）：classicui 選字鍵與候選字同一字型。
- 文字範本狀態把字級降到 32：主題是全域的，不能依狀態改。
- 毛玻璃（popover vibrancy）：classicui 的 `EnableBlur` 要合成器支援（X11 Cinnamon 通常不支援，UNKNOWN U7），效果也不同。
- 只對小麥注音生效：做不到，主題影響所有輸入法。
- GNOME Wayland 走 kimpanel 擴充套件時，classicui 主題完全不適用，外觀由擴充套件與 GNOME Shell 主題決定。

---

## 4. 使用說明與「關於」

### 4.1 Mac 行為

- 選單「使用說明」（`IMC:186-188`、`L10N:284`）用預設瀏覽器開啟 App bundle 內的 `UserGuide.html`（`IMC:426-430`）。
- `UG` 是單一 HTML，CSS 內嵌，支援深色模式（`prefers-color-scheme`）、側邊目錄、窄螢幕改單欄（`UG:1-80`）。目錄 18 節（`UG:84-102`），各節 `<section id>` 在 `UG:108-356`。
- 選單「關於小麥注音…」（`IMC:192-194`、`L10N:2`）→ `NSApp.orderFrontStandardAboutPanel`（`IMC:494-497`），內容是 App 名稱、版本、版權（`IPS`：「Copyright © 2011-2026 Mengjuei Hsieh et al.\nAll Rights Reserved.\n智慧學習版 © 2026 butyroger」）與 `CR` 的連結：
  1. 智慧學習版說明：`https://github.com/butyroger/McBopomofo#readme`
  2. 智慧學習版 GitHub 專案頁面：`https://github.com/butyroger/McBopomofo`
  3. 問題回報（智慧學習版）：`https://github.com/butyroger/McBopomofo/issues`
  4. （空一行）原版網站：`https://mcbopomofo.openvanilla.org/`
  5. 原版使用手冊：`https://github.com/openvanilla/McBopomofo/wiki/使用手冊`
  6. 原版常見問題：`https://github.com/openvanilla/McBopomofo/wiki/常見問題`
  7. 原版意見討論：`https://github.com/openvanilla/McBopomofo/discussions`

### 4.2 Linux 版使用說明要改寫的地方（依 `UG` 各節）

- 字型：`font` 改為 `system-ui, "Noto Sans CJK TC", sans-serif`，等寬改 `ui-monospace, "DejaVu Sans Mono", monospace`（`UG:35`、`UG:58-62`）。
- 副標題「從輸入法選單的「使用說明」可以再次開啟本頁」保留（`UG:106`）。
- **新增「安裝與設定」一節**（目錄第一項）：安裝 `.deb`（`sudo apt install ./fcitx5-mcbopomofo-smart-learning_<版本>_amd64.deb`）；用 fcitx5 設定工具加入「小麥注音」；Ubuntu GNOME 要裝 kimpanel 擴充套件；Mint 不用；面板主題（§3.3）；詞庫資料夾指到同步資料夾（偏好設定「User data folder」）。
- 基本操作（`UG:108-118`）：「輸入法選單」改為「fcitx 選單（系統匣圖示或面板上的選單）」；其餘相同。
- 智慧中英切換、中英混打、英文聯想（`UG:140-212`）：英文聯想的來源 Linux 是 `/usr/share/dict/american-english` 加上打過的字（`fSL/EnglishWords.h` `kDefaultEnglishWordList`），範例清單要用 Linux 實際結果重抓（`UG:203-205` 是 macOS 拼字檢查器的結果）。「iPhone、macOS」等內建大小寫範例保留（是資料，不是平台說明）。
- 常用句、自動學習、管理詞彙（`UG:232-301`）：「多台 Mac」改為「多台電腦（Mac 與 Linux）」；「詞庫放在 iCloud 雲碟時」改為「詞庫放在同步資料夾（例如 Synology Drive）時」。管理詞彙一節加上「在檔案管理員中顯示」與 Linux 的開啟方式。
- 浮動組字窗（`UG:303-307`）：改寫為「此程式使用浮動組字窗」，組字顯示在 fcitx 面板最上方（不是游標上方的小窗），以程式為單位，程式名稱見選單項目；有些程式（Wayland 原生程式等）沒有程式名稱，選單不會出現（§2.4）。
- 中英提示與模式圖示（`UG:309-315`）：「螢幕右上角」「選單列」改為 fcitx 的通知與系統匣／面板圖示，依 phase 2 實作的行為寫。
- **刪除**「輸入法變灰（安全輸入）」一節（`UG:317-328`，macOS 專屬，設計文件非目標）與偏好設定中的「安全輸入提醒」一項（`UG:340`）。
- 偏好設定（`UG:330-342`）：「從輸入法選單「小麥注音偏好設定」開啟」改為「fcitx5 設定工具 → 輸入法 → 小麥注音 → 設定」；「面板主題」改為「見安裝與設定的面板主題」；只列 Linux 實際有的選項。
- 詞庫檔案（`UG:344-354`）：預設位置 `~/.local/share/fcitx5/mcbopomofo/`；「iCloud 雲碟」改為同步資料夾；`learning/` 一項加 `sentences/` 與 `removed.txt`；加一句「同步工具產生的衝突副本不會被讀取」（`FF` 最後一節）。
- 快速鍵一覽（`UG:356-370`）：
  - 「Ctrl ⌘ G 輸出簡體中文」→ 由 fcitx 的繁簡轉換外掛控制（`fMB:1058`、`fMB:1065-1069` 加入的 `chttrans` 動作），快速鍵依該外掛設定（UNKNOWN U8：Ubuntu 24.04 預設鍵）。
  - 「Ctrl ⌘ H 切換半形標點」→ fcitx port 沒有對應快速鍵（`fMB` 只用選單動作 `halfWidthPunctuationAction_`），改寫為「從選單切換」，或另外實作快速鍵（決策 D11）。
  - 加註：Super、Alt 組合鍵輸入法不處理（`fMB:1247-1251`）；fcitx 全域的 Ctrl+Alt+P 是「切換是否在程式內顯示組字」。
- **新增「關於」一節**（§4.3）。

### 4.3 Linux 的安裝位置、開啟方式與「關於」

- 檔案：`data/user-guide.html` → `${FCITX_INSTALL_PKGDATADIR}/data/mcbopomofo-user-guide.html`（和 `mcbopomofo-data.txt` 同處，`CMakeLists.txt` 的 install 規則）。執行時用 `fcitx5_compat::locate("data/mcbopomofo-user-guide.html")` 找（同 `fLML:155`）。
- 選單「使用說明」：`fcitx::startProcess({"xdg-open", <路徑>})`。
- fcitx 沒有「關於」面板。建議（決策 D12）：選單「關於小麥注音…」用 `xdg-open "<路徑>#about"` 開到使用說明的「關於」一節。注意 `xdg-open` 對 `file://…#fragment` 的支援依瀏覽器而定（UNKNOWN U9），不行就只開頁面，「關於」放在目錄第一項。也可以由 Qt 程式提供「關於」對話框（`mcbopomofo-vocabulary --about`）。
- 「關於」一節內容：名稱「小麥注音（智慧學習版）」、版本（CMake 以 `CPACK_PACKAGE_VERSION` 產生，目前 `3.1.1-learning.3`）、版權（照 `IPS` 的文字，加上 fcitx5 port 的授權 `LICENSE.txt`：MIT，「Copyright (c) 2022 and onwards The McBopomofo Authors」）、連結照 §4.1 的 7 條。**智慧學習版的專案頁面與問題回報要連 Linux 版的 repo 還是 Mac 版的 repo**，未定（決策 D13；建議專案頁面與問題回報都連 `https://github.com/butyroger/fcitx5-mcbopomofo`，另加一條連到 Mac 版）。

---

## 5. 輸入法選單對照（Mac `IMC:70-196` → fcitx statusArea）

fcitx 的選單由 `statusArea().addAction(StatusGroup::InputMethod, …)` 組成，在 `activate` 時依模式加入（`fMB:1059-1105`）；`Action::setSeparator` 可做分隔線。

1. 標準／許氏鍵盤（Mac `IMC:73-85`）→ 已有（`layoutStandardAction_`、`layoutHsuAction_`）。
2. 簡體、半形、聯想詞、注音字型標註（`IMC:87-118`）→ 已有。
3. Option 鍵才顯示的「詞彙替換」（`IMC:120-125`）→ fcitx 沒有按住修飾鍵開選單的概念，不搬（已在 phase 1 範圍外）。
4. 「使用者詞彙」標題列（disabled，`IMC:128-130`）→ 不搬，或用 separator。
5. 編輯使用者詞彙、編輯要排除的詞彙（`IMC:132-152`）→ 已有，但目前只在小麥注音模式加入（`fMB:1100-1105`）；純注音模式的對應檔（`data-plain-bpmf.txt` 等）不在本階段範圍。
6. **管理詞彙…**（`IMC:154-156`）→ 新增，兩個模式都顯示（§1.1）。
7. **編輯文字範本、編輯符號表**（`IMC:157-162`）→ fcitx port 目前**沒有**這兩項（`fMBh:504-514` 的 action 清單），本階段補上，寫法同 `editUserPhrasesAction_`（`GetOpenFileWith`）。
8. 「使用者檔案有問題 ⚠️」（`IMC:164-171`）→ fcitx 用通知顯示（`showAndClearUserFileIssues`），不搬選單項目。
9. 分隔線後：**此程式使用浮動組字窗**（`IMC:175-181`）→ 新增（§2.3）。
10. 小麥注音偏好設定（`IMC:183-185`）→ fcitx 的系統匣選單本來就有「設定」，不另加（決策 D14）。
11. **使用說明**、**關於小麥注音…**（`IMC:186-194`）→ 新增（§4.3）。
12. 檢查更新（`IMC:189-191`）→ 不搬（設計文件：更新檢查不搬）。

UNKNOWN U10：GNOME kimpanel 擴充套件與 Cinnamon 系統匣對 checkable 動作、分隔線的呈現。

---

## 6. 偏好對照（Mac → 建議 fcitx）

- `FloatingCompositionBundleIDs`（`PF:46`、`PF:378-382`）→ `FloatingCompositionPrograms`，`fcitx::Option<std::vector<std::string>>`，預設空（§2.3）。
- `CandidatePanelTheme`（`PF:38`、`PF:337-340`）→ 不搬，由 classicui 主題取代（§3.3；phase 3 文件 §9 已註明）。
- `CandidateListTextSize`（`PF:34`、`PF:325-326`）、`CandidateTextFontName`、`CandidateKeyLabelFontName`（`PF:56-57`）→ 不搬，由 classicui `Font` 取代。
- 詞彙管理員本身沒有偏好。

---

## 7. 實作模組總表

共用模組（`src/SmartLearning/`，不含 fcitx 標頭，附 GoogleTest）：

1. `RemovedEntries`：新增 `add`／`remove`（§1.6 第 1 點）。
2. `EnglishWordStore`：`allWords`／`add`／`remove`，加上本機檔外部變動合併與丟掉已刪除的本機項目（§1.5 A）。
3. `CommonSentenceStore`：`allEntries`／`add`／`remove`，同上。
4. `TemporaryPhraseStore`：`allPhrases`／`forget`，加上本機檔外部變動合併。
5. `VocabularyFile`（新，或併入 `UserPhraseFile`）：單一行的取代／附加／刪除（§1.4），檔案不存在時不寫（決策 D6）。
6. `ReadingExtraction`（新）：§1.2 第 9 點的貪婪讀音比對，輸入為 `McBopomofoLM`。
7. `FloatingComposition`（新，純函式）：是否使用 client preedit（§2.5）。

fcitx 介面層（`src/McBopomofo.cpp`）：

- 選單動作：管理詞彙…、編輯文字範本、編輯符號表、此程式使用浮動組字窗、使用說明、關於小麥注音…（§5）。
- `useClientPreedit(context)` 取代三處判斷（§2.3）。
- `reloadConfig()` 強制 store 重新檢查；`activate` 偵測 removed.txt 變動（§1.5 A 第 4、5 點）。

Qt 程式（`src/VocabularyManager/`，Qt6 Widgets，連 `SmartLearningLib`）：視窗與六個分頁（§1.2、§1.3），寫完呼叫 `ReloadAddonConfig`。

資料（`data/`）：`user-guide.html`、`themes/mcbopomofo-light/`、`themes/mcbopomofo-dark/`、`.desktop` 檔（詞彙管理員）。

---

## 8. UNKNOWN 與決策清單

UNKNOWN：

1. U1：各前端回報的 `InputContext::program()` 值：GTK／Qt fcitx 模組、XIM、Wayland text-input（5.1.7 `waylandim`）、Chrome／Electron 在 X11 與 Wayland 下各是什麼，哪些是空字串（§2.4）。要在 Mint 與 Ubuntu 實機記錄。
2. U2：classicui 在只有 server 端 preedit、沒有候選字時，面板是否總會出現、位置是否穩定（§2.4）。kimpanel 擴充套件的呈現同樣待測。
3. U3：fcitx 在 IC 焦點進入時一定會呼叫 `activate`（本文件 §1.5 A 依賴這點讓管理員的修改在切回程式時生效）。phase 3 的行為看起來是這樣，但沒有明確驗證。
4. U4：Linux 版是否需要 Mac 讀音帶出時的簡轉繁一步（需要 OpenCC 相依）；使用者是否會在管理員輸入簡體字（§1.6）。
5. U5：classicui 設定的 `Theme`、`DarkTheme`、`Font` 鍵名（§3.2）。
6. U6：Mac 淺色、深色候選字面板的實際顏色（背景、反白、文字），要拿使用者 Mac 的截圖比對（§3.3）。
7. U7：Cinnamon X11 是否支援 classicui 的 `EnableBlur`（§3.4）。
8. U8：Ubuntu 24.04 fcitx 繁簡轉換外掛的預設快速鍵（§4.2）。
9. U9：`xdg-open` 開 `file://…#about` 時，各預設瀏覽器是否保留錨點（§4.3）。
10. U10：kimpanel 擴充套件與 Cinnamon 系統匣對 checkable 動作與分隔線的呈現（§5）。

決策：

1. D1：管理員與輸入法兩個程序的本機檔同步方式：A 檔案層級合併 + `ReloadAddonConfig` 通知（推薦），或 B 由輸入法匯出 D-Bus 方法（§1.5）。
2. D2：Qt 程式名稱（暫定 `mcbopomofo-vocabulary`）與單一視窗的做法（`QLocalServer` 或 D-Bus 名稱）（§1.6）。
3. D3：從桌面選單開啟且輸入法沒產生過 `DeviceID` 時，唯讀開啟還是由管理員產生並寫回設定檔（§1.6）。
4. D4：是否加 OpenCC 相依做簡轉繁（§1.6，依 U4）。
5. D5：Qt 程式的翻譯機制（`tr` 或 gettext）（§1.6）。
6. D6：管理員寫 data.txt／exclude-phrases.txt／text-templates.txt 時，檔案不存在要不要建立（Mac 會建立，建議 Linux 不建立）（§1.6）。
7. D7：視窗重新取得焦點時自動重新整理（Mac 沒有）（§1.7）。
8. D8：浮動組字窗清單是否顯示在 fcitx 設定工具（Mac 不顯示）（§2.3）。
9. D9：切換浮動組字窗時，組字中的文字 commit（比照 Mac）還是丟棄（比照 fcitx 的切換鍵盤配置）（§2.3）。
10. D10：是否隨套件提供 `mcbopomofo-light`／`mcbopomofo-dark` 主題、是否附圓角圖、主題是否寫死字型（§3.3）。
11. D11：半形標點是否另做快速鍵（Mac 為 Ctrl+⌘+H，Linux 對應 Ctrl+Super+H 會被 `fMB:1247-1251` 的 Super 放行規則擋掉）（§4.2）。
12. D12：「關於」用使用說明的一節，還是 Qt 對話框（§4.3）。
13. D13：「關於」與使用說明裡的專案頁面、問題回報連到 Linux 版還是 Mac 版 repo（§4.3）。
14. D14：選單是否另加「小麥注音偏好設定」直接開啟 fcitx5 設定工具的小麥注音頁（§5）。
