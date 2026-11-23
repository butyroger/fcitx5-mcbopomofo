# Mac 版採用共用模組計畫

來源：Mac fork `/Users/roger/Developer/McBopomofo` @ `7446fd0`（3.1.1-learning.23）加上 `545569e`（除以零不給結果）。目標：fcitx5 port `/Users/roger/Developer/fcitx5-mcbopomofo`（branch `smart-learning` @ `c82c309`，3.1.1-learning.4）的 `src/SmartLearning/`。設計文件：Mac repo `docs/superpowers/specs/2026-11-23-linux-fcitx5-port-design.md`（目標第 4 點「新功能的邏輯寫成不依賴 fcitx5 的純 C++ 共用模組，附單元測試，將來 Mac 版可逐步改用」）。

本文件只是計畫，不改 Mac 程式碼。每一步都要另開 Mac 分支、另做驗收。

Mac 端縮寫（行號皆為 `7446fd0`；`545569e` 只改了 `QN`，`QN` 的行號以 `545569e` 為準）：

- `KH` = `Source/KeyHandler.mm`
- `LMM` = `Source/LanguageModelManager.mm`
- `QN` = `Source/QuickNumbers.swift`
- `CS` = `Source/CommonSentences.swift`（含 `RemovedEntries`）
- `EW` = `Source/EnglishWords.swift`
- `FC` = `Source/FloatingComposition.swift`
- `IS` = `Source/InputState.swift`
- `IMC` = `Source/InputMethodController.swift`
- `AD` = `Source/AppDelegate.swift`
- `PF` = `Source/Preferences.swift`
- `SP` = `Source/ServiceProvider.swift`
- `VM` = `Source/VocabularyManager.swift`
- `GC` = `Packages/CandidateUI/Sources/CandidateUI/GridCandidateController.swift`

Linux 端：`fSL/` = `src/SmartLearning/`。行為規格與已知差異的出處：`docs/phase2-mac-behavior.md`（`P2`）、`docs/phase3-mac-behavior.md`（`P3`）、`docs/phase4-mac-behavior.md`（`P4`）、`docs/acceptance-phase3.md`（`A3`）、`docs/file-formats.md`（`FF`）。

---

## 0. 結論摘要

1. Mac 已經在 app target 裡直接編譯 C++ 引擎（`McBopomofoLM.cpp`、`UserOverrideModel.cpp` 等都在 `McBopomofo.xcodeproj` 的 Sources，`CLANG_CXX_LANGUAGE_STANDARD = c++20`），`KeyHandler.mm`、`LanguageModelManager.mm` 是 ObjC++。所以共用模組最省事的接法是：**把 `src/SmartLearning/` 的 `.cpp` 加進同一個 app target，ObjC++ 直接呼叫；Swift 呼叫端保留現有的 `@objc` 類別名稱與 selector，改成 ObjC++ 寫的薄殼**。不建議現在開 Swift C++ interop（§2.2）。
2. 測試：Mac CI 已經跑 `Source/` 的 CMake + `ctest`（`.github/workflows/continuous-integration-workflow-xcode-latest.yml` 的「Build/Test McBopomofo Engine」）。把 `SmartLearningLibTest` 加進 `Source/CMakeLists.txt`，gtest 幾乎可以原封不動在 macOS 跑；XCTest 只需要補「薄殼接對了」的少量案例。
3. 順序（風險由低到高）：建置基礎 → 純函式（`MixedInput`、`BopomofoKeys`、`CandidateAdjustment`、`FloatingComposition`）→ 數字快打與範本、符號表解析 → 兩段式學習與 `data.txt` 編輯 → `RemovedEntries` → 暫存詞 → 英文字 → 常用句 → 選字學習 `UserOverrideStore` → 詞彙管理員用的 API。每一步都能單獨發版、單獨回退。
4. 採用前要先在共用模組補幾個「平台接縫」，否則 Mac 會被迫改變行為：裝置名稱的預設值（現在寫死 `Linux`）、英文字典（Mac 用 `NSSpellChecker`）、ICU（macOS 不提供公開的 ICU 標頭）、iCloud 佔位檔（§2.3）。
5. 檔案格式兩邊已完全相同（`FF`），所以採用只會改變**規則**，不會改變**格式**。和舊版 Mac 共用同步資料夾的風險集中在「新規則比舊規則保守」的地方，例如 pending 保護、衝突副本不再重複計數（§3 各步）。
6. 不共用：fcitx 介面層、Qt 詞彙管理員、Mac 的 InputMethodKit 介面與候選窗、SwiftUI 詞彙管理員（§4）。

---

## 1. 模組對照

每個模組列出：做什麼、對應的 Mac 程式、已知行為差異。差異只列已記錄在 `P2`／`P3`／`P4`／`A3`／`FF` 的項目。

### 1.1 `AtomicFile`

- 做什麼：`writeFileAtomically`（暫存檔 `.<name>.tmp-<pid>`、`fsync`、確認 `close` 成功才改名）、`createFileIfAbsent`（`O_CREAT|O_EXCL`，不會截斷同步剛送來的檔）、`readWholeFile`、`FileStamp`／`stampOf`／`readIfChanged`（存在與否、修改時間、大小）、`splitLines`（LF、CR、VT、FF、NEL、U+2028、U+2029 都斷行）。
- Mac 對應：沒有獨立模組。各處用 Foundation 的 `writeToFile:atomically:`／`String.write(toFile:atomically:)`：`EW` `save`（`EW:273-285`）、`CS` `save`（`CS:307-323`）與 `RemovedEntries.save`（`CS:80-85`）、`LMM` `_saveTemporaryPhrases`（`LMM:900-914`）、`saveUserOverrideModel`（`LMM:983-1008`）、`_writePhrase:atEndOfPath:`（`LMM:337-374`）、`_removePhrase:atPath:`（`LMM:376-425`）、`VM` `write`／`remove`（`VM:360-388`）。
- 已知差異：
  - Linux 原子寫入**保留原檔權限**，新檔用 0666 減 umask（`A3` 驗收後修正：`data.txt` 曾由 644 變成 600）。Foundation 原子寫入對權限的處理沒有查證（UNKNOWN，§5）。
  - Linux 寫入失敗一律刪暫存檔、保留原檔（`FF` 第 128 行）；Mac 多數呼叫端用 `try?` 忽略錯誤。
  - macOS 的 `fsync` 不保證寫到實體媒體（要 `F_FULLFSYNC`）。行為上不影響正確性，只影響斷電時的保證，可以接受；若要對等，可在 `AtomicFile.cpp` 以 `#ifdef __APPLE__` 改用 `fcntl(fd, F_FULLFSYNC)`。

### 1.2 `LearningFileUtils`

- 做什麼：資料夾正規化、`split`、`parseInteger`／`parseNumber`、`isPlainPart`（不可含空白、tab、換行）、steady clock 秒數、`stampsOf`／`txtFileStamps`（一個資料夾所有 `.txt` 的 stamp，用來判斷要不要重新合併）。
- Mac 對應：散在各 store 的私有讀檔函式：`EW` `read`（`EW:235-246`）、`CS` `read`（`CS:263-278`）、`RemovedEntries.loadFiles`（`CS:67-77`）、`LTReadTemporaryPhrases`（`LMM:837-854`）。Mac 不看 stamp，改由 FSEvents（`AD:331-355`）與 60 秒快取觸發重讀。
- 已知差異：Linux 以 stamp 判斷變動，Mac 以 FSEvents 判斷。採用後 Mac 兩者並存：FSEvents 觸發 `reloadIfChanged()`，由 stamp 決定是否真的重讀。

### 1.3 `LearningPaths`

- 做什麼：`newDeviceID`（8 碼大寫十六進位）、`sanitizeDeviceName`（`/ : \ \n \r \t` 換成 `_`，空字串變 `Linux`）、`learningFilename`（`<裝置ID>-<名稱>.txt`）、`isSyncConflictCopy`（檔名含 `_Conflict` 或 `.sync-conflict-`）、`otherDevicesLearningFiles`（排除本機檔、隱藏檔、衝突副本，依檔名排序）、`findRenamedOwnFile`、`learningFolderState`。
- Mac 對應：`_userOverrideModelDeviceID`（`LMM:674-683`，`NSUUID` 前 8 碼，存在 defaults `UserOverrideModelDeviceID`）、`userOverrideModelDataPath`（`LMM:685-699`，`SCDynamicStoreCopyComputerName`，空字串變 `Mac`）、`_loadLocalUserOverrideModelIfNeeded` 的改名搬移（`LMM:749-786`）、`reloadUserOverrideModel` 的列舉（`LMM:788-815`）、`EW` `loadIfNeeded`（`EW:249-271`）與 `CS` `loadFiles`（`CS:287-305`）的列舉。
- 已知差異：
  - 名稱為空時的預設值：Mac `Mac`、Linux `Linux`（`fSL/LearningPaths.cpp:65`）。Mac 採用前必須把預設值改成參數（§2.3）。
  - Linux 排除隱藏檔與衝突副本；Mac 只要副檔名是 `.txt` 都併入，衝突副本的次數會重複計入（`FF` 第 181-191 行、`P2` §5.3 第 3 點、`P3` §5.6）。這是刻意差異。
  - Mac 讀到 `.<name>.icloud` 佔位檔會觸發下載（`LMM:731-745`、`LMM:797-801`）；Linux 沒有這段（§2.3）。
  - 改名：Mac 只要找到同裝置 ID 前綴的其他 `.txt` 就搬（`LMM:756-764`，目標已存在時 `moveItemAtPath` 會失敗）；Linux 只在本機檔不存在時才搬，而且不搬衝突副本，搬失敗就維持 pending（`fSL/UserOverrideStore.cpp:138-148`）。

### 1.4 `UserDataFolder`

- 做什麼：`resolveUserDataFolder`（空字串用預設資料夾、展開 `~/`、只建立一層缺少的資料夾，其餘情形退回預設並給警告）、`shouldCreateTemplateFiles`（只在預設資料夾建立範本檔）。
- Mac 對應：`dataFolderPath`（`LMM:550-561`）、`UserPhraseLocationHelper`（`PF:608` 起）、偏好視窗的資料夾檢查（`Source/PreferencesUI/PreferencesModel.swift:280` 附近）、`checkIfUserDataFolderExists`（`LMM:221`）、`ensureFileExists:populateWithTemplate:extension:`（`LMM:246`）。
- 已知差異：Mac 由偏好視窗選資料夾，路徑一定是絕對路徑；不展開 `~`。Mac 在任何資料夾都會用範本建立 `data.txt` 等檔（`checkIfUserLanguageModelFilesExist`，`LMM:267-288`）；Linux 只在預設資料夾建立（`fSL/UserDataFolder.h`）。

### 1.5 `BopomofoKeys`

- 做什麼：`ComponentCount`、`KeyReplacesComponent`（例如 `app` 的第二個 `p` 只是取代聲母）、`RawKeysOfCurrentReading`、`PlanTrailingEnglishToChinese`（英文後直接接中文：決定刪幾個字母、補上哪個讀音）。
- Mac 對應：`_componentCount:`（`KH:664`）、`KH:1212-1214` 的判斷、`_rawKeysOfCurrentReading`（`KH:671-685`）、`_convertTrailingEnglishToChineseWithKey:stateCallback:`（`KH:852-933`）的判斷部分。
- 已知差異：無。`P2` §2 逐條對照，`acceptance-phase2.md` 第 2 節實機結果與 Mac 一致（含許氏 `hellojkq` 的情形）。

### 1.6 `MixedInput`

- 做什麼：讀音分類 `IsEnglishReading`、`IsHalfWidthAlphanumericReading`、`IsChineseReading`、`IsLetterReading`；`TrailingLetters`、`PlanEnglishCompletion`（Tab 英文聯想要刪幾個字母、插入哪個 `_english_` 讀音）、`AutoSpaceNeeded`、`ShouldRemoveAutoSpaceAfterDeletingEnglish`、`PlanTrimBeforeCursor`（Esc 清亂碼）、`LanguageModeLabel`、`SwitchReminderText`、`WidthModeLabel`、`FullWidthSymbol`。
- Mac 對應：`KH:51-92` 的三個 static 函式、`_enterEnglishCompletionWithInput:stateCallback:`（`KH:808-845` 的前半）、`_insertAutoSpaceIfNeededBeforeEnglish:`（`KH:705-720`）、`_removeAutoSpaceAfterDeletingEnglish`（`KH:724-735`）、`_trimMistypedTextBeforeCursor`（`KH:1748-1818`）、`ChineseEnglishSwitchReminder` 與 `ModeStatusBar`（`AD:497-616`）、`KH:1645-1652` 的全形換算（`charCode + 0xFEE0`）。
- 已知差異：無。`SwitchReminderText` 的「尚未知道 Caps Lock 狀態時只記基準、不提示」是 Linux 為 fcitx 啟用時序加的，Mac 由 `AD:515-551` 的通知時機自然做到，結果相同。

### 1.7 `QuickNumbers`

- 做什麼：`NormalizeNumberCode`（全形數字與運算子轉半形，數字間的「。」「‧」轉小數點）、`IsNumberCode`、`QuickNumberCandidates`（日期、時間、金額、運算結果）、`ArithmeticResult`（`=` 自動補答案）、`EvaluateArithmetic`。
- Mac 對應：`QN` 的 `normalized`（`QN:47`）、`isNumberCode`（`QN:61`）、`candidates(for:)`（`QN:69`）、`arithmeticResult(endingWith:)`（`QN:90`）、`date`／`time`／`amount`／`arithmetic`／`formatted`（`QN:116-280`）、`Expression.evaluate`（`QN:283-`）；呼叫端 `_appendArithmeticResultAfterKey:stateCallback:`（`KH:785-804`）與 `IS` 的 `TextTemplate`。
- 已知差異：
  - **除以零**：原本 Mac 的 `5/0+1=` 會補上 `+1` 的結果 1；`545569e` 改成「任何除以零都不給結果」，與 Linux 相同（`fSL/QuickNumbers.h` 的 `ArithmeticResult` 註解、`A3` 第 5 節）。這條規則現在兩邊一致。
  - Linux 另加「巢狀超過 64 層不算」的保護（`EvaluateArithmetic`）。Mac 沒有這個上限，正常輸入碰不到。
  - 中文數字：Linux 連結 C++ 的 `ChineseNumbersLib`（`src/ChineseNumbers/`），Mac 用 Swift 套件 `Packages/ChineseNumbers`。兩者是否對所有輸入逐字相同未確認（`P3` §12 第 6 點）。
  - 「今年」：Linux 由傳入的 `time_t` 在本地時區算；Mac 用 `Calendar.current`。

### 1.8 `CandidateAdjustment`

- 做什麼：`CheckCandidateAdjustable`（候選字 `+`／`-`：巨集安靜忽略；`_` 開頭、空值、含空白或換行就響；單字可以）、`PageOfCandidateIndex`（取消後把反白還原到原頁）。
- Mac 對應：`KH:2500-2571`（尤其 `KH:2514-2525` 的檢查順序），取消還原 `KH:2560-2563`、`KH:2618-2624`。
- 已知差異：無。檢查順序（先巨集、再前綴）照 Mac（`P3` §2.3 第 3 點）。

### 1.9 `FloatingComposition`

- 做什麼：`UsesFloatingComposition`（程式是否在清單內）、`UseClientPreedit`、`ToggleFloatingComposition`（加到清單尾端，或移除所有同名項目）。
- Mac 對應：`FloatingCompositionPanel.isEnabled(for:)`（`FC:66`）、`toggleFloatingComposition(_:)`（`IMC:346-370`）、`Preferences.floatingCompositionBundleIDs`（`PF:378-382`）。
- 已知差異：Linux 的程式名稱來自 fcitx `InputContext::program()`，可能是空字串；Mac 是 bundle ID。浮動窗的位置與字級兩邊不同（`P4` §2.4），但那是介面層，不在本模組。

### 1.10 `LearnPhrase`

- 做什麼：`PlanLearnPickedPhrase`（選兩字以上的詞：先成暫存詞，選第 2 次學進 `data.txt`）、`PlanLearnBrokenUpPhrase`（拆詞學習，例如「許是」改成「許氏」）、`PhrasesToLearnOnCommit`（暫存詞在之後的組字中再打出來送出就轉正）。
- Mac 對應：`_learnPhraseIfFrequentlyChosen:`（`KH:455-489`）、`_learnBrokenUpPhraseIfFrequentlyChosen:walkBefore:cursor:`（`KH:496-554`）、`_learnFromCommittedComposition` 的暫存詞部分（`KH:402-449`）、`kLearnPhraseThreshold`（`KH:453`）、`_addTemporaryPhrase:`（`KH:390`）。
- 已知差異：無規則差異。實作上 Linux 引擎的 `McBopomofoLM` 多了 `isFirstUserPhraseOrExcluded`（`src/Engine/McBopomofoLM.h:118-123`，由 `src/KeyHandler.cpp:2633` 呼叫，用來在寫檔前先用記憶體判斷「已是第一個／已排除」），Mac 引擎沒有。這是 Mac 與 Linux `src/Engine/` 目前唯一的差異（另一處是 `.cpp` 多一行 `#include <iterator>`），採用前先把它同步到 Mac 的 `Source/Engine/`。

### 1.11 `ReadingExtraction`

- 做什麼：`ExtractReading`：從頭貪婪比對內建詞庫最長的詞，各段讀音以 `-` 連接，查不到的字是「？」。給詞彙管理員自動帶出注音。
- Mac 對應：`ServiceProvider.extractReading(from:)`（`SP:50-83`），讀音來自 `LanguageModelManager.reading(for:)`（`LMM:1025-1033`）；`VM` 的 `fillReading`（`VM:212`）呼叫它。
- 已知差異：Mac 每個候選子字串先經 OpenCC 簡轉繁再查（`SP:63`）；Linux 不做轉換（`P4` §1.2 第 9 點、決策 D4）。Linux 全部查不到時回傳 nullopt，Mac 回傳一串「？」。

### 1.12 `SymbolTable`（含 `SymbolGrid`）

- 做什麼：`BuiltInSymbolTable`、`ParseSymbolTable`（`#` 與 BOM、分類名稱尾端 `*`、全形空白是符號）、`SymbolTableOfFile`、`SymbolTableFile`（stamp 變了才重讀）、`CandidateDisplayText`（同值候選加讀音，符號除外）、`SelectionKeyIndexIgnoringCase`、`SymbolGrid`（兩鍵選符號的格狀排列與狀態）。
- Mac 對應：`kDefaultSymbolTable`（`LMM:59-87`）、`_loadSymbolTable`（`LMM:578-613`）、`symbolSectionsForReading:`／`symbolInitialIndexForReading:`（`LMM:615-628`）、`defaultSymbolTable`（`LMM:573`）；格狀候選窗 `GC` 與 `KH:2361-2382`、`KH:2804-2823`。
- 已知差異：
  - Mac 在每次 `loadUserPhrases` 時重讀（`LMM:182-188`）；Linux 以 stamp 判斷。
  - `SymbolGrid` 是 Linux 為了在 fcitx 面板上模擬 Mac 格狀候選窗而寫的狀態機，欄數用選字鍵數（`P3` §6.6、§12 第 9 點）；Mac 依螢幕寬度由 `GC` 計算。Mac **不採用** `SymbolGrid`（§4）。

### 1.13 `TextTemplates`

- 做什麼：`ParseTextTemplates`（tab 分隔、`#` 註解、`\n`／`\t`／`\\` 跳脫）、`MatchTextTemplates`（完全相同 → 開頭相同 → 代碼或內容包含，不分大小寫）、`TextTemplateCandidates`（數字代碼先給數字快打）、`TextTemplateDisplayText`、`IsNumberOperatorKey`、`kMaxTextTemplateCodeLength = 50`、`TextTemplateStore`（2 秒檢查一次 stamp、只在檔案不存在時寫範例）。
- Mac 對應：`IS:287-` 的 `InputState.TextTemplate`：`entries(matching:)`、`loadEntries`、`unescape`、`createSampleFileIfNeeded`；`_handleTextTemplateState:`（`KH:339`）；`textTemplatesDataPath`（`LMM:568`）。
- 已知差異：
  - 「包含」比對：Mac 用 `localizedCaseInsensitiveContains`，Linux 用 ICU case folding；非 ASCII 內容的結果是否完全一致未確認（`P3` §6.2、§12 第 7 點）。
  - Mac 每次輸入代碼都重讀檔案（`loadEntries`）；Linux 最多 2 秒查一次 stamp。
  - 範例檔：Linux 只在預設資料夾寫，並用 `createFileIfAbsent`（`P3` §6.2）。

### 1.14 `RemovedEntries`

- 做什麼：`removed.txt` 的讀取（60 秒快取、`reloadIfChanged`）與寫入（`add`／`remove`：先重讀、整份排序後原子寫入、時間取整數秒、拒收含 tab 或任何換行的 key）。
- Mac 對應：`CS:29-86` 的 `RemovedEntries`（`removedTime`、`add`、`remove`、`loadIfNeeded`、`loadFiles`、`save`）。
- 已知差異：
  - **寫入順序**：Mac `EnglishWords.remove` 先寫本機檔、再在 `removed.txt` 加記錄（`EW:209-215`）；`CommonSentences.remove` 同樣先存本機檔（`CS:251-257`）；`add` 則先從 `removed.txt` 移除再寫本機檔（`EW:200-207`、`CS:238-249`）。Linux 的 `add`／`remove`／`forget` 都**先處理 `removed.txt`**，`removed.txt` 讀不到或寫不了就什麼都不改、回傳 false（`fSL/EnglishWords.h`、`fSL/CommonSentences.h`、`fSL/TemporaryPhrases.h` 的註解）。這樣不會出現「本機檔已刪、但其他裝置沒收到刪除記錄」的半套狀態。
  - Linux 的 `add` 在「`removed.txt` 存在但讀不到」或「用戶資料夾不存在」時不寫；Mac 的 `loadFiles` 讀不到就當空表，接著整份覆寫，可能把別台的刪除記錄洗掉。
  - Linux 的時間由呼叫端傳入牆鐘（`wallNow`），快取間隔用 steady clock；Mac 都用 `Date()`（`P3` §0.2）。

### 1.15 `UserPhraseFile`

- 做什麼：`learn`（學進 `data.txt`：插到同讀音第一個詞之前，或附加到檔尾；已排除的不學）、批次 `learn`（每個檔只讀一次、`data.txt` 最多寫一次）、`write`（`+`：先從排除檔移除再附加）、`remove`（`-`：先從 `data.txt` 移除再附加到排除檔）、`fitsInColumn`。
- Mac 對應：`learnUserPhrase:`（`LMM:448-509`）、`writeUserPhrase:`（`LMM:427-446`）、`removeUserPhrase:`（`LMM:511-530`）、`_checkIfPhrase:existAtPath:`（`LMM:303-335`）、`_writePhrase:atEndOfPath:`（`LMM:337-374`）、`_removePhrase:atPath:`（`LMM:376-425`）。
- 已知差異：
  - 讀不到的檔案一律不寫（`fSL/UserPhraseFile.h`）。Mac `learnUserPhrase` 讀不到 `data.txt` 也回 NO，相同；但 Mac 在這之前先呼叫 `checkIfUserLanguageModelFilesExist`（`LMM:450`），它會以範本建立缺少的檔案（`LMM:267-288`）。Mac 採用時保留這個前置呼叫，行為就不變。
  - 檔案權限保留（同 §1.1）。
  - 「讀不到排除檔就不學」：Linux 在排除檔存在但讀不到時拒絕學習；Mac 的 `_checkIfPhrase` 讀不到回 NO（當作沒排除），會學。這是 Linux 較保守的地方。

### 1.16 `VocabularyFile`

- 做什麼：詞彙管理員對單一共寫檔（`data.txt`、`exclude-phrases.txt`、`text-templates.txt`）的單行編輯：`replaceOrAppendLine`、`removeLines`（每個要刪的行只刪第一個相同的行），以 `\n` 切行、原樣比對。
- Mac 對應：`VocabularyModel.write(line:replacing:path:)`、`remove(lines:path:)`（`VM:360-388`）。
- 已知差異：**檔案不存在時不寫**（決策 D6，`P4` §1.6 第 5 點）；Mac `write` 讀不到檔時會建立只有這一行的新檔。Linux 這樣做是為了不讓同步還沒送到的 `data.txt` 被一個只有一行的新檔取代。

### 1.17 `EnglishWords`（`EnglishLexicon`、`EnglishWordStore`）

- 做什麼：字表（排序、小寫、只收 ASCII 字母）；學過的英文字（每台裝置一個檔、讀時相加、60 秒重讀、排除 `removed.txt` 的 `english`）；`preferredCase`、`isWord`、`isWordOrPrefix`、`completions`；詞彙管理員用的 `allWords`／`add`／`remove`。
- Mac 對應：`EW` 全部：`record`（`EW:39`）、`completions(for:)`（`EW:52-101`）、`isMixedCase`、`preferredCase(for:)`（`EW:131`）、`isWord`（`EW:148`）、`isWordOrPrefix`（`EW:172`）、`allWords`／`add`／`remove`（`EW:192-215`）、`isSubsequence`（`EW:217`）、`read`／`loadIfNeeded`／`save`（`EW:235-285`）；字典是 `NSSpellChecker`（`EW:166-186`）。
- 已知差異：
  - **英文聯想的排序**：Linux 用字表，沒有詞頻，順序是「長度遞增、同長度依字母」，再把沒有母音的縮寫（如 assn、asst、blvd）排到字表補完的最後，並且不列出與輸入相同的字（`P3` §3.4、`A3` 第 3 節與驗收後修正）。Mac 用 `NSSpellChecker.completions` 加 `guesses`（`EW:72-79`）。Mac 採用時要保留 `NSSpellChecker`（§2.3）。
  - **pending**：Linux 在本機檔讀到之前不寫（`fSL/EnglishWords.h`）；Mac 的 `localCounts` 第一次讀檔失敗就當空表，之後照寫（`P2` §5.3）。
  - **外部改動合併**：Linux 在重新載入與存檔前，若本機檔 stamp 變了就重讀並逐字取大（`P4` §1.5 A）。Mac 的詞彙管理員在同一程序，原本不需要；採用後無害。
  - 衝突副本與隱藏檔不併入（§1.3）。
  - 存檔時機：Linux 由呼叫端排程（fcitx 打完 2 秒後或停用時）；Mac 每次 `record` 都在背景佇列寫（`EW:273-285`）。

### 1.18 `CommonSentences`

- 做什麼：`SentenceLength`、`IsValidSentence`、`SplitSentences`；`CommonSentenceStore`（送出 3 次才記住、6 到 40 字、壽命 90 天起跳、釘選不過期、多裝置相加、`removed.txt` 的 `sentence` 以時間比較）、`completion`、詞彙管理員用的 `allEntries`／`add`／`remove`。
- Mac 對應：`CS:92-323`：`length(of:)`（`CS:121`）、`isValid`（`CS:142`）、`lifetime(of:)`（`CS:154`）、`isAlive`（`CS:158`）、`record`（`CS:166`）、`completion(forTypedText:)`（`CS:193`）、`allEntries`／`add`／`remove`（`CS:225-257`）、`read`／`loadIfNeeded`／`loadFiles`／`save`（`CS:263-323`）；呼叫端 `buildInputtingState` 結尾（`KH:3536-3546`）與 Tab（`KH:1133-1146`）。
- 已知差異：
  - **pending**：同 §1.17，Mac 沒有（`P3` §5.6）。
  - **外部改動合併**：次數取大、時間取晚、釘選取聯集；本機中已在 `removed.txt` 刪除的句子丟掉（`P4` §1.5 A 第 3 點）。這同時修掉「刪除後帶著舊次數復活」的問題（`P4` §1.5 第 3 點）。
  - 斷句與字元分類：Linux 用 ICU（`u_charType`、grapheme break iterator），Mac 用 Foundation 的 `CharacterSet` 與 `Character`（`P3` §5.2）。Emoji 與新版 Unicode 字元的分類可能因 ICU 版本而不同。
  - 存檔時機：Linux 排程（2 秒延遲）；Mac 每次 `record` 在主執行緒同步寫（`P3` §5.3 第 5 點、§5.7）。

### 1.19 `TemporaryPhrases`

- 做什麼：`TemporaryPhraseStore`：暫存詞（每台裝置一個檔、`值 讀音 時間`、30 天過期、多裝置取較新時間、`removed.txt` 的 `temporary` 以時間比較）、`contains`／`add`／`remove`（轉正用，不寫 `removed.txt`）、`phrasesByReading`（給 `setTemporaryPhrases`）、`allPhrases`／`forget`（詞彙管理員用，寫 `removed.txt`）、`reloadIfChanged`（偵測 `temporary/` 與 `removed.txt` 的 stamp）。
- Mac 對應：`LMM:817-959`：`gLocalTemporaryPhrases`／`gTemporaryPhrases`、`_temporaryPhrasesDataPath`、`LTReadTemporaryPhrases`、`_reloadTemporaryPhrases`、`_applyTemporaryPhrases`、`_saveTemporaryPhrases`、`isTemporaryPhrase:`、`addTemporaryPhrase:`、`allTemporaryPhrases`、`forgetTemporaryPhrase:`、`removeTemporaryPhrase:`。
- 已知差異：
  - **pending**：Mac 沒有「本機檔還沒讀到就不寫」的保護，本機檔還沒同步下來時第一次 `addTemporaryPhrase` 會把只有一筆的內容寫上去；Linux 刻意加上（`P3` §1.4 邊界情形）。
  - 同讀音多個值的順序：Mac 是字典迭代順序，不固定；Linux 是最近選的在前（`P3` §12 第 1 點）。
  - 過期：Mac 只在讀檔時丟掉過期項目，本機表讀一次後不再過濾；Linux 每次合併都過濾。
  - 存檔：Mac 每次加入或移除都立刻寫（`LMM:900-914`）；Linux 的 `add`／`remove` 只改記憶體，由呼叫端 `save()`。

### 1.20 `UserOverrideStore`

- 做什麼：選字學習的本機模型與合併模型（容量 5000／20000、半衰期 30 天）、`observe`、`reloadIfChanged`、`reload`、`save`、pending。
- Mac 對應：`LMM:34-45` 的常數與兩個 `UserOverrideModel`、`userOverrideModel`（`LMM:721-729`）、`_loadLocalUserOverrideModelIfNeeded`（`LMM:749-786`）、`reloadUserOverrideModel`（`LMM:788-815`）、`observeUserOverrideWithWalkBefore:walkAfter:cursor:`（`LMM:961`）、`_scheduleSavingUserOverrideModel`（`LMM:972`，2 秒）、`saveUserOverrideModel`（`LMM:983-1008`）、`_userOverrideModelIOQueue`（`LMM:701-709`）。
- 已知差異：
  - pending 兩邊都有（Mac `gLocalUserOverrideModelFilePending`，`LMM:747`）。規則相同。
  - 衝突副本、隱藏檔、改名規則見 §1.3；iCloud 佔位檔見 §2.3。
  - Linux 另外偵測「資料夾本身是否存在」的變化，資料夾消失再出現時重讀（`UserOverrideStore.h` 的 `Snapshot`）。

---

## 2. 橋接方式

### 2.1 建議：直接編進 app target，ObjC++ 直接呼叫

- Mac 現況：`McBopomofo.xcodeproj` 把 `Source/Engine/*.cpp` 直接列在 McBopomofo target 的 Sources，`KeyHandler.mm`、`LanguageModelManager.mm` 直接 `#include` 引擎標頭使用 C++ 物件（例如 `static McBopomofo::UserOverrideModel gLocalUserOverrideModel`，`LMM:42`）。同一份引擎另由 `Source/CMakeLists.txt`／`Source/Engine/CMakeLists.txt` 建成 `McBopomofoLMLib` 跑 gtest。
- 做法：
  1. 把 `src/SmartLearning/` 放到 Mac repo 的 `Source/SmartLearning/`（只放 `.h`、`.cpp`、`*Test.cpp`、`CMakeLists.txt`、`NoFcitxIncludeTest.cmake`）。
  2. 非測試的 `.cpp` 加進 McBopomofo target；測試檔只進 CMake。
  3. `Source/CMakeLists.txt` 加 `add_subdirectory(SmartLearning)`，CI 現有的 `ctest` 步驟就會跑 `SmartLearningLibTest`。
  4. KeyHandler、LanguageModelManager 這類 ObjC++ 呼叫端直接用 C++ API。
- 單一來源：以 fcitx repo 的 `src/SmartLearning/` 為準，Mac 端用腳本複製（或 `git subtree`），CI 加一步比對兩邊內容，不一致就失敗。兩邊都要改時，先改共用模組並在兩邊各自跑測試，不允許只在 Mac 端改共用檔。日後若兩邊都穩定，可再抽成獨立 repo。

### 2.2 Swift 呼叫端：保留 `@objc` 名稱的 ObjC++ 薄殼

- 現在由 Swift 實作、被 ObjC++ 與 Swift 呼叫的類別：`QuickNumbers`（`QN:29`，`@objc`）、`RemovedEntries`（`CS:29`）、`CommonSentences`（`CS:92`）、`EnglishWords`（`EW:28`）、`InputState.TextTemplate` 的靜態函式（`IS:287-`）。
- 做法：每一步把對應的 Swift 實作換成同名、同 selector 的 ObjC++ 類別（例如 `SLQuickNumbers.mm` 以 `@interface QuickNumbers : NSObject` 宣告 `+isNumberCode:`、`+candidatesFor:`、`+arithmeticResultEndingWith:`），放進 app 的 bridging header（`Source/McBopomofo-Bridging-Header.h`）。KeyHandler 原本透過 `McBopomofo-Swift.h` 呼叫的地方改成直接呼叫 C++，Swift 端（`IS`、`VM`、`SP`）呼叫名稱不變。
- 薄殼只做型別轉換（`NSString` ↔ UTF-8 `std::string`、`NSArray` ↔ `std::vector`），不含任何規則。這是 Mac 端唯一要寫的新程式碼。
- 不建議 Swift C++ interop（`SWIFT_OBJC_INTEROP_MODE = objcxx`）：它是整個 target 的設定，所有 Swift 檔都要以 C++ 模式重新編譯；`std::function`、`std::filesystem::path`、`std::optional<std::string>` 在 Swift 端的對應仍不完整；Packages 是獨立的 SwiftPM 套件，也不能直接看到 app target 的 C++。等 Swift 端呼叫需求變多再評估。
- 不建議包成 SwiftPM C++ 套件：Mac 的引擎本來就不走 SwiftPM，共用模組依賴 `McBopomofoLMLib`、`gramambular2`、`Mandarin`，拆出去要連引擎一起搬。

### 2.3 採用前要補的平台接縫

這些改動都在共用模組，Linux 行為不變，Linux 的 gtest 要同步補案例。

1. **裝置名稱預設值**：`sanitizeDeviceName` 增加參數或常數注入（Mac 傳 `Mac`、Linux 傳 `Linux`），否則電腦名稱為空的 Mac 會改用 `<ID>-Linux.txt`，等於變成另一台裝置。
2. **英文字典**：`EnglishWordStore` 現在吃具體類別 `EnglishLexicon`（讀字表檔）。改成介面或三個函式注入：`isWord`、`hasLongerWordWithPrefix`、`wordsWithPrefix`，另加選用的 `guesses`。Linux 用字表實作，Mac 用 ObjC++ 包 `NSSpellChecker`（`EW:166-186` 的呼叫照搬）。字表專屬的排序規則（長度遞增、縮寫排後，`A3` 驗收後修正）留在 Linux 的字表實作裡，不要放進共用排序。
3. **ICU**：`CommonSentences.cpp` 用 `ubrk`／`u_charType`，`TextTemplates.cpp` 用 `BreakIterator`／`UnicodeString`。macOS 有 `libicucore`，但不提供公開標頭，Apple 不保證其 ABI。兩個選項：
   - A（建議）：把 ICU 呼叫集中到一個小介面（字元是否為分隔字元、grapheme 數、不分大小寫包含），Linux 用 ICU 實作，Mac 用 ObjC++ 包 Foundation（`NSCharacterSet.punctuationCharacterSet`／`symbolCharacterSet`、`NSString` 的 composed character、`localizedCaseInsensitiveContainsString:`）。Mac 的結果就與現在的 Swift 版一致。
   - B：Mac 也靜態連結一份 ICU。兩邊分類完全一致，但 app 變大、多一個要更新的相依，而且 Mac 現有行為會被改掉。
4. **iCloud 佔位檔**：Mac 讀到 `.<name>.icloud` 會觸發下載並維持 pending（`LMM:774-781`、`LMM:797-801`）。詞庫已搬到 Synology Drive，設計文件說「在新位置不會用到，但不需移除」。共用模組加一個選用的回呼：列舉資料夾時遇到 `.icloud` 佔位檔就交給平台處理（Mac 呼叫 `startDownloadingUbiquitousItemAtURL:`），並把本機檔視為 pending。Linux 不設回呼。
5. **時鐘**：各 store 已注入 steady clock 與牆鐘，Mac 傳 `CFAbsoluteTimeGetCurrent`／`[NSDate date].timeIntervalSince1970` 即可，不用改。
6. 共用模組加一個與 `NoFcitxIncludeTest.cmake` 同樣做法的檢查，確認沒有 include `<Foundation/...>`、`<CoreFoundation/...>`，平台實作一律放在 Mac 端的 `.mm`。

### 2.4 執行緒

- Mac 的 store 都是行程內的靜態狀態：`EW`、`CS` 是 Swift `static var`，暫存詞與選字模型是 `LMM` 的 C++ 全域變數。IMK 的按鍵處理、`VocabularyManager` 的 SwiftUI、`FSEvents` 回呼轉回的 `DispatchQueue.main.async`（`AD:340-353`）都在主執行緒；寫檔則分散在背景佇列：`EW` 用 `DispatchQueue.global`（`EW:279-284`）、暫存詞與選字模型用 `_userOverrideModelIOQueue`（`LMM:701-709`），常用句在主執行緒同步寫。
- 共用模組的 store 不是執行緒安全的。規則：
  1. 每種 store 在 Mac 端只有一個實例，由 `LanguageModelManager` 持有（取代現在的全域變數），只在主執行緒呼叫。
  2. `McBopomofoLM::setEnglishCompletionProvider` 的回呼會在 walk 期間被呼叫（`P3` §3.4 第 5 點），walk 也在主執行緒，沒有衝突；不要把 walk 移到背景。
  3. 存檔：先照 Linux 的做法在主執行緒同步呼叫 `save()`，2 秒延遲排程沿用 `_scheduleSavingUserOverrideModel` 的計時器。若量測後主執行緒寫檔會卡，再在共用模組把 `save()` 拆成「在主執行緒產生內容與 stamp」和「在 I/O 佇列寫入」兩段，不要讓背景佇列直接碰 store。
  4. 共用模組的 `RemovedEntries` 以 `shared_ptr` 由三個 store 共用（`fSL/EnglishWords.h` 等的建構子），Mac 也照這樣接，不要讓 Swift 薄殼另建一份。

---

## 3. 採用順序

每一步都包含：要做的事、Mac 端可以刪掉的程式、風險、測試。每一步單獨合進 Mac 的 `smart-learning`、單獨發一版 learning.N，實機用幾天再進下一步。

所有步驟共通的測試：

- `ctest`：Mac CI 的引擎測試加上 `SmartLearningLibTest`（§2.1），macOS 與 Linux 兩邊都要綠。
- `xcodebuild -scheme McBopomofo test`：`McBopomofoTests/` 全部要過（`KeyHandlerBopomofoTests.swift` 106 個、`KeyHandlerPlainBopomofoTests.swift` 21 個、`PreferencesTests.swift` 27 個、`InputMacroTests.swift` 36 個、`ServiceProviderTests.swift` 18 個等）。下面各步只另外點出和該步直接相關的測試。
- 實機驗收照 `docs/acceptance-phase3.md` 的格式：測前整個備份用戶資料夾，測完只扣掉測試加入的內容，再和備份比對。

### 3.0 建置基礎（不改行為）

- 做的事：
  1. 把 `isFirstUserPhraseOrExcluded` 同步進 Mac 的 `Source/Engine/McBopomofoLM.{h,cpp}`（§1.10），兩邊 `src/Engine/` 恢復完全相同（`.clang-format`、`.gitignore` 除外）。
  2. 放入 `Source/SmartLearning/`、加進 CMake 與 xcodeproj，但先不呼叫。
  3. `QuickNumbers` 依賴 C++ 的 `ChineseNumbersLib`，把 fcitx 的 `src/ChineseNumbers/`（`ChineseNumbers`、`SuzhouNumbers`、`StringUtils` 與測試）一起放進 `Source/ChineseNumbers/`（C++ 版），只供共用模組使用；Swift 的 `Packages/ChineseNumbers` 維持給 Ctrl+\ 功能選單的 `NumberInputHelper.swift` 用。
  4. 完成 §2.3 的接縫（裝置名稱、字典介面、Unicode 介面、iCloud 回呼），Linux 一起發一版確認行為不變。
- 刪除：無。
- 風險：只有建置面：app 多了 C++ 程式碼、多一份 ICU 或 Unicode 介面的實作。
- 測試：`SmartLearningLibTest` 在 macOS 全過。目前已知要調整的只有 `LearningPathsTest` 的 `sanitizeDeviceName("") == "Linux"`（改成用注入的預設值），以及 `ReadingExtractionTest` 的 `MCBOPOMOFO_TEST_FULL_DATA_PATH`（Mac 要指到建好的 `data.txt`，Mac 的詞庫原始檔在 `Source/Data/`，要先跑 `Source/Data/Makefile`）。`EnglishWordsTest` 依賴字表實作的案例改用 `fromWords` 的小字表，不要依賴 `/usr/share/dict/american-english`。

### 3.1 純函式：`MixedInput`、`BopomofoKeys`、`CandidateAdjustment`、`FloatingComposition`

- 做的事：`KeyHandler.mm` 改呼叫共用函式；KeyHandler 自己保留狀態機（英文模式旗標、grid 操作），只把「算出要刪幾個、插什麼」交給共用模組（`P2` §1.8 的模組邊界）。浮動組字窗的清單判斷與切換改呼叫 `FloatingComposition`，傳入 bundle ID。
- Mac 端刪除：
  - `KH:51-92` 的 `IsEnglishReading`、`IsHalfWidthAlphanumericReading`、`IsChineseReading`，以及 `KH:53` 的 `kAutoSpaceReading`。
  - `_componentCount:`（`KH:664`）、`_rawKeysOfCurrentReading` 的重組邏輯（`KH:671-685`）、`KH:1212-1214` 的取代判斷。
  - `_convertTrailingEnglishToChineseWithKey:` 的條件判斷（`KH:852-933`，保留實際改 grid 的幾行）。
  - `_enterEnglishCompletionWithInput:` 找字母的迴圈（`KH:810-823`）、`_insertAutoSpaceIfNeededBeforeEnglish:` 與 `_removeAutoSpaceAfterDeletingEnglish` 的判斷、`_trimMistypedTextBeforeCursor` 的掃描（`KH:1748-1818`，保留刪 reading 的部分）。
  - 候選字 `+`／`-` 的檢查（`KH:2514-2525`）。
  - `IMC:346-360` 的清單增刪、`FC:66` 的清單比對。
  - `AD:497-616` 的提示文字與狀態列文字可改呼叫 `SwitchReminderText`／`LanguageModeLabel`／`WidthModeLabel`，但只是幾個字串，**可不做**。
- 風險：低。都是無狀態的純函式，沒有檔案 I/O、沒有同步檔相容問題。唯一要注意的是 UTF-8 與 UTF-16 的換算：共用函式吃 UTF-8 讀音字串，KeyHandler 裡的游標是 reading 索引，不是字元位置，兩者一致；不要把 `NSString` 的 UTF-16 長度傳進去。
- 測試：
  - gtest：`MixedInputTest`、`BopomofoKeysTest`、`CandidateAdjustmentTest`、`FloatingCompositionTest`。
  - XCTest 必須維持通過：`testUppercaseLetterWhenEmpty1/2`、`testUppercaseLetterWhenNotEmpty1/2`、`testIgnorePunctuationTable`、`testInvalidBpmf`、`testHalfPunctuationComma`、`testPunctuationComma`、`testHalfPunctuationPeriod`、`testPunctuationPeriod`、`testCtrlPunctuationPeriod`、`testEscToClearReadingAndGoToEmpty`、`testEscToClearReadingAndGoToInputting`、`testEscToClearAll`、`testEscKey`、`testEscKeyWithCandidate`、`testBackspace*`、`testInputSpace`、`testInputSpaceInBetween`、`testTabKey`、`testMacroAndTabKey`（`P2` §10 列的清單）。
  - Mac 原本沒有中英混打的正向測試（`P2` §10）。把 fcitx 的 `src/KeyHandlerSmartTest.cpp` 案例改寫成 XCTest 補上（`app`、`you`＋空白、`ChatGPT`、`hello`＋許氏 `hwf`、自動空白、Esc 清亂碼），以後每一步都當回歸測試。

### 3.2 數字快打、範本、符號表解析：`QuickNumbers`、`TextTemplates`（不含 store）、`SymbolTable`（不含 `SymbolGrid`）

- 做的事：`QuickNumbers` 的 `@objc` 類別改成 ObjC++ 薄殼；`InputState.TextTemplate` 的 `loadEntries`、`entries(matching:)`、`unescape` 改呼叫 `ParseTextTemplates`／`MatchTextTemplates`／`TextTemplateCandidates`；`_loadSymbolTable` 的解析改呼叫 `SymbolTableOfFile`，內建表改用 `BuiltInSymbolTable()`。
- Mac 端刪除：`QN` 的實作（保留薄殼）、`IS` 的 `TextTemplate` 解析與比對函式（保留 state 類別本身與 `CandidateProvider` 部分）、`kDefaultSymbolTable`（`LMM:59-87`）與 `_loadSymbolTable` 的逐行解析（`LMM:578-613`，保留 `setSymbolTable` 呼叫）。`defaultSymbolTable`（`LMM:573`，選單「編輯符號表」寫範例檔用）改回傳 `BuiltInSymbolTable()`。
- 風險：
  - 中文數字：C++ `ChineseNumbers` 與 Swift 套件輸出若有差，「陸仟伍佰肆拾捌元整」這類候選會變（`P3` §12 第 6 點）。先寫一個比對測試：兩邊對 0 到 100000 與幾個大數、小數逐一比較，再決定。
  - 除以零規則兩邊已一致（`545569e`），不會再改變結果。
  - 範本「包含」比對的大小寫規則：照 §2.3 選項 A 接 Foundation，結果與現在一致。
  - 範本與符號表都是使用者手寫的單一檔，只讀不寫（範例檔除外），沒有同步相容問題。
- 測試：
  - gtest：`QuickNumbersTest`、`TextTemplatesTest`、`SymbolTableTest`。
  - XCTest：`testPunctuationTable`、`testBacktickKey`、`testBacktickThenEsc`、`testBacktickThenBackspace`、`testBacktickThenBacktick`、`testBacktickThenPunctuation`、`testInputBufferThenBacktickThenEsc`、`testInputBufferWithCharactersThenBacktickThenEsc`、`testDoubleBackquoteForceCommit`（`P3` §6.7）；`testNumber*`（Ctrl+\ 的數字輸入，不受影響，但確認沒被 C++ `ChineseNumbers` 取代）。
  - 引擎 gtest：`McBopomofoLMTest.SymbolsForPunctuation`。
  - 補 XCTest：`` ` `` 後打 `6548` 的 7 個候選、`（2＋3）＊3＝` 補 15、`5/0+1=` 不補（`A3` 第 4、5 節）。

### 3.3 兩段式學習與 `data.txt` 編輯：`LearnPhrase`、`UserPhraseFile`

- 做的事：`fixNodeWithReading`（`KH:250-285`）與 `_learnFromCommittedComposition`（`KH:402-449`）改呼叫 `PlanLearnPickedPhrase`、`PlanLearnBrokenUpPhrase`、`PhrasesToLearnOnCommit`，依回傳的 `LearnAction` 呼叫現有的暫存詞函式（這一步暫存詞仍用 Mac 原本的 `LMM` 實作）。`learnUserPhrase:`、`writeUserPhrase:`、`removeUserPhrase:` 改成呼叫 `UserPhraseFile::learn`／`write`／`remove`，前面保留 `checkIfUserLanguageModelFilesExist`。
- Mac 端刪除：`_learnPhraseIfFrequentlyChosen:`（`KH:455-489`）、`_learnBrokenUpPhraseIfFrequentlyChosen:`（`KH:496-554`）的判斷、`KH:453` 的門檻常數；`LMM` 的 `_checkIfPhrase:existAtPath:`（`LMM:303-335`）、`_writePhrase:atEndOfPath:`（`LMM:337-374`）、`_removePhrase:atPath:`（`LMM:376-425`）、三個公開方法的本體（`LMM:427-530`）。`checkIfUserPhraseExist:key:`（`LMM:290`）若仍被加詞介面使用，改成用同一套兩欄比對。
- 風險：
  - 這是第一個會寫使用者檔案的步驟。`data.txt` 是兩台 Mac 與 Linux 共寫的單一檔；共用模組寫前一定重讀，格式與排序規則與 Mac 相同（`P3` §1.5），舊版 Mac 讀得懂。
  - 權限：改用共用的原子寫入後，`data.txt` 的權限會被保留（`A3` 的修正）。舊版 Mac 寫出的權限是什麼要先實測（§5）。
  - 「排除檔存在但讀不到時不學」比 Mac 保守（§1.15）。
  - 學會後 Mac 依賴 FSEvents 重讀 `data.txt`（`P3` §1.5 結尾），不用改。
- 測試：
  - gtest：`LearnPhraseTest`、`UserPhraseFileTest`。
  - 引擎 gtest：`UserOverrideModelTest.ObservationCountOfBrokenUpPhrase`、`McBopomofoLMTest` 的 `EarlierUserPhraseComesFirst`。
  - Mac 原本沒有學習流程與 `+`／`-` 的測試（`P3` §1.9、§2.5、§11）。補 XCTest：「許是」選「式」兩次學進 `data.txt`、選一次再打出來送出才學、同一次組字選了又送出不轉正、排除詞不會學回、單字「你」可以 `+`／`-`、`_literal_a` 響一聲、巨集候選安靜。
  - 實機：照 `A3` 第 1、2 節，並確認 `data.txt` 權限不變。

### 3.4 `RemovedEntries`

- 做的事：Mac 的 `RemovedEntries`（Swift，`CS:29-86`）改成 ObjC++ 薄殼，轉呼叫 `LanguageModelManager` 持有的唯一 C++ 實例；`EW`、`CS` 的 Swift 實作暫時仍透過薄殼讀。
- Mac 端刪除：`CS:29-86`（保留同名薄殼直到 §3.6、§3.7 完成，之後薄殼也刪掉）。
- 風險：
  - `removed.txt` 所有裝置共用一檔。共用模組的寫法（重讀、排序、整數秒）與 Mac 相同（`P3` §0.3），舊版 Mac 讀得懂、也寫得出相同格式。
  - 寫入順序改變只影響詞彙管理員的刪除與加入（§1.14），在 §3.6、§3.7 接上 store 之前，`EW`／`CS` 的 Swift 仍照舊順序呼叫薄殼。
  - 共用模組讀不到檔時不寫，比 Mac 安全；舊版 Mac 仍可能在讀不到時覆寫，這是舊版本身的行為，新版改不了。
- 測試：
  - gtest：`RemovedEntriesTest`（排序、整數時間、含 tab 或換行的 key 被拒、讀不到不寫）。
  - XCTest：Mac 沒有 `RemovedEntries` 測試（`P3` §11）。補一個薄殼的測試：`add` 後 `removedTime` 非 0、`remove` 後為 0、檔案內容與舊 Swift 版寫出的逐位元組相同。

### 3.5 暫存詞：`TemporaryPhraseStore`

- 做的事：`LMM:817-959` 改成呼叫 `LanguageModelManager` 持有的 `TemporaryPhraseStore`。`reloadUserOverrideModel` 最後一步（`LMM:811`）改成 `reloadIfChanged()` 後 `setTemporaryPhrases(phrasesByReading())`；`addTemporaryPhrase:`／`removeTemporaryPhrase:` 呼叫 `add`／`remove` 後立刻 `save()`（維持 Mac 「每次都寫」的時機）；`forgetTemporaryPhrase:` 改呼叫 `forget`。
- Mac 端刪除：`gLocalTemporaryPhrases`／`gTemporaryPhrases`（`LMM:817-818`）、`LTReadTemporaryPhrases`、`_reloadTemporaryPhrases`、`_applyTemporaryPhrases`、`_saveTemporaryPhrases`、`kTemporaryPhraseLifetime`（`LMM:95`），以及 `isTemporaryPhrase:`、`allTemporaryPhrases`、`forgetTemporaryPhrase:` 的本體（保留方法名給 `VM` 與 KeyHandler）。
- 風險：
  - **pending 是新行為**：本機檔還沒同步下來時，新版不寫，舊版會寫上一筆蓋掉。兩台 Mac 一新一舊時，新版那台只會更安全。
  - 同讀音多個暫存詞的順序由「不固定」變成「最近選的在前」，使用者可能看到候選順序改變（改善，不是退步）。
  - 過期規則：新版每次合併都濾掉 30 天前的項目，存檔時本機檔也不再留過期項目；舊版 Mac 讀到時本來就會丟掉，格式相容。
  - 執行緒：原本存檔在 `_userOverrideModelIOQueue`；改成主執行緒同步 `save()`（§2.4）。
  - iCloud 佔位檔回呼要接上（§2.3 第 4 點）。
- 測試：
  - gtest：`TemporaryPhrasesTest`（40 個，含 pending、外部改動合併、30 天過期、`removed.txt` 時間比較、多裝置取較新、`forget`）。
  - XCTest：§3.3 補的學習流程測試要繼續過（暫存詞是它的一半）。
  - 實機：一台新版、一台舊版 Mac 共用同步資料夾，各選一個暫存詞，確認兩邊都看得到；在新版 `forget`，確認舊版也隱藏。

### 3.6 英文字：`EnglishWordStore`

- 做的事：`EnglishWords`（`EW`）改成 ObjC++ 薄殼，`record`、`completions(for:)`、`preferredCase(for:)`、`isWord`、`isWordOrPrefix`、`allWords`、`add`、`remove` 轉呼叫共用 store；字典接 §2.3 第 2 點的 `NSSpellChecker` 實作；`KH:205-214` 注入的 completion provider 改用 store 的 `completions`。
- Mac 端刪除：`EW` 全部（`EW:28-285`），只留 `NSSpellChecker` 的轉接（`EW:148-186` 用到的三個呼叫）。
- 風險：
  - **聯想順序**：保留 `NSSpellChecker` 的 `completions` 與 `guesses`，Mac 的順序維持不變；Linux 字表的排序規則（`A3`）不能帶到 Mac。這要求共用的 `completions` 把「字典補完」這段完全交給字典實作，共用部分只負責學過的字（`P3` §3.3 第 1-5、7、8 步）。
  - **衝突副本不再計入**：舊版會把 `<ID>-名稱 (衝突副本).txt` 的次數重複加進來，新版不會（§1.3）。已有衝突副本的資料夾，新版顯示的次數會變少，`isWord`（「打過兩次」）與大小寫修正可能因此改變。採用前先清掉資料夾裡的衝突副本。
  - **pending**：用戶資料夾還沒同步到時，新版不寫本機檔（`FF` 第 155 行），舊版會寫。
  - 存檔時機：舊版每次 `record` 在背景佇列寫；新版改成和選字模型同一個 2 秒排程，在主執行緒寫。
  - `add` 改成「先處理 `removed.txt`，失敗就不改」（§1.14）。
- 測試：
  - gtest：`EnglishWordsTest`（53 個；字表相關案例改用小字表或注入的字典）。
  - XCTest：§3.1 補的中英混打測試全部要過，`isWord`、`preferredCase` 都靠這個 store。補：學兩次 `ChatGPT` 後打 `chat` 排第一、學過 `associated` 後 `aocd` 找到（`P3` §3.7）。
  - 實機：打 `ass` 按 Tab，清單與舊版相同（因為字典沒換）。

### 3.7 常用句：`CommonSentenceStore`

- 做的事：`CommonSentences`（`CS:92-323`）改成 ObjC++ 薄殼，`record`、`completion(forTypedText:)`、`allEntries`、`add`、`remove` 轉呼叫共用 store；斷句與字元分類接 §2.3 第 3 點的 Unicode 介面。`CommonSentencesEnabled`（`PF:45`）在 `syncWithPreferences`（`KH:221`）時呼叫 `setEnabled`。
- Mac 端刪除：`CS:92-323`；§3.4 留下的 `RemovedEntries` 薄殼在這一步之後已無 Swift 呼叫端，一起刪掉。
- 風險：
  - 斷句、長度、emoji 判定：接 Foundation 時結果與舊版一致；若選 §2.3 選項 B（ICU），Mac 的結果會向 Linux 靠攏，可能讓某些含 emoji 或新 Unicode 字元的句子由「記住」變「不記住」或反過來。
  - **外部改動合併與刪除**：新版刪除後若本機記憶體還有舊次數，會依 `removed.txt` 丟掉，不會帶舊次數復活（`P4` §1.5 第 3 點）。和舊版 Mac 混用時，舊版那台的行為不變。
  - 存檔由「每次 `record` 在主執行緒同步寫」改成 2 秒排程。
- 測試：
  - gtest：`CommonSentencesTest`（30 個）。
  - XCTest：Mac 沒有常用句測試（`P3` §11）。補：「你好你好你好」送三次後打「你好」出現「Tab：你好你好」、送兩次不提示、游標不在尾端不提示（`A3` 第 6 節、`P3` §5.9）。

### 3.8 選字學習：`UserOverrideStore`（連同 `LearningPaths`、`AtomicFile`、`LearningFileUtils` 的實際使用）

- 做的事：`LMM:34-45`、`LMM:669-1008` 的選字模型載入、合併、存檔改成 `LanguageModelManager` 持有的 `UserOverrideStore`；`userOverrideModel` 回傳 `mergedModel()`；FSEvents 觸發的 `reloadUserOverrideModel` 改成 `reloadIfChanged()`；`observeUserOverrideWithWalkBefore:` 呼叫 `observe`；`saveUserOverrideModel` 呼叫 `save()`，2 秒延遲排程保留。
- Mac 端刪除：`gLocalUserOverrideModel`／`gUserOverrideModel`（`LMM:42-45`）、`LTReadUserOverrideModelFile`、`_loadLocalUserOverrideModelIfNeeded`、`gLocalUserOverrideModelFilePending`、`reloadUserOverrideModel` 的列舉、`saveUserOverrideModel` 的寫檔；`_userOverrideModelDeviceID` 與 `userOverrideModelDataPath` 的檔名組法改呼叫 `learningFilename(deviceID, sanitizeDeviceName(computerName))`（裝置 ID 仍存在 defaults，產生方式可改用 `newDeviceID()`，兩者格式相同：8 碼大寫十六進位）。`_userOverrideModelIOQueue` 在暫存詞與選字模型都改完後刪除。
- 風險（最高，這是每天用的核心）：
  - 改名規則不同（§1.3）：新版只在本機檔不存在時才搬同 ID 的舊檔。電腦改名後的第一次啟動要實測。
  - 衝突副本與隱藏檔不併入：合併模型的資料量可能變少，建議結果因此改變。
  - 資料夾消失再出現（同步工具重建資料夾）時新版會重讀；舊版要等 FSEvents。
  - iCloud 佔位檔：接上 §2.3 第 4 點的回呼，否則仍在 iCloud 的使用者會失去「等下載完成再讀」的保護。
  - 存檔仍是 2 秒延遲，但改在主執行緒寫（§2.4）。選字模型檔最大 5000 筆，要量測寫檔時間。
- 測試：
  - gtest：`UserOverrideStoreTest`（16 個）、`LearningPathsTest`、`AtomicFileTest`；引擎 `UserOverrideModelTest` 全部。
  - XCTest：`KeyHandlerBopomofoTests` 所有選字相關測試（`testCandidateWithDown`、`testCandidateWithSpace` 等）要過；Mac 的測試用暫存資料夾 `LTITestDataFolderPath`（`LMM:535-548`），共用 store 也要指向它。
  - 實機：一新一舊兩台 Mac 與一台 Linux 共用資料夾，各自選字、確認互相看得到；新版那台改電腦名稱後重開，確認本機檔被搬名而不是新開一個。

### 3.9 詞彙管理員用的部分：`VocabularyFile`、`ReadingExtraction`，以及各 store 的 `all*`／`add`／`remove`／`forget`

- 做的事：`VocabularyModel` 的 `write(line:replacing:path:)`、`remove(lines:path:)` 改呼叫 `VocabularyFile`；`fillReading` 改呼叫 `ExtractReading`；英文、常用句、暫存詞分頁在 §3.5-§3.7 已經轉到共用 store，這一步只確認 `VM` 呼叫的是薄殼。
- Mac 端刪除：`VM:360-388`；`SP:50-83` 的比對迴圈（`ServiceProvider` 的「加注音」服務也用它，保留函式名，改呼叫 `ExtractReading`）。
- 風險：
  - **檔案不存在時不寫**（決策 D6）：Mac 的詞彙管理員原本會建立新檔；新版會失敗並顯示訊息。訊息字串要加進 `Source/zh-Hant.lproj/Localizable.strings`。
  - **簡轉繁**：`ExtractReading` 不做 OpenCC。Mac 可以在呼叫前先把整個詞送 `OpenCCBridge.shared.convertToTraditional` 再交給 `ExtractReading`；簡繁轉換絕大多數是一字對一字，結果與舊版逐段轉換相同，但要用 `ServiceProviderTests.testExtractReading` 的案例確認。
  - `RemovedEntries` 寫入順序改變（§1.14）在這一步才真正影響使用者：刪除失敗時不會留下半套狀態。
- 測試：
  - gtest：`VocabularyFileTest`、`ReadingExtractionTest`。
  - XCTest：`ServiceProviderTests` 的 `testExtractReading`、`testAddReading`、`testConvertToReadings`、`testAddReadingServiceWithPasteboard`。Mac 沒有詞彙管理員的測試（`P4` §1.8）。
  - Linux 的 `src/VocabularyManager/LearnedVocabulary.{h,cpp}`（不依賴 Qt，見 §4）有分頁資料的規則與測試，可以參考來補 Mac 的單元測試。

### 3.10 不採用或延後

- `UserDataFolder`：Mac 的資料夾由偏好視窗選擇並檢查，規則與 Linux 的設定字串解析不同（§1.4），採用沒有好處。`shouldCreateTemplateFiles` 的想法（同步資料夾不先建立範本檔）值得 Mac 另外評估，但會改變 Mac 現有行為，另案處理。
- `SymbolGrid`：見 §4。
- `TextTemplateStore`、`SymbolTableFile`（stamp 重讀）：Mac 由 FSEvents 觸發重讀，已經夠用。§3.2 只採用解析與比對的純函式。

---

## 4. 不共用的部分

- **fcitx 介面層**：`src/McBopomofo.cpp`、`src/McBopomofo.h`（fcitx addon、設定選項、statusArea 選單、候選字清單、`useClientPreedit`、D-Bus `ReloadAddonConfig`）、`src/KeyHandler.cpp` 與 `src/InputState.h`（fcitx 版的狀態機）。它們是共用模組的呼叫端，對應 Mac 的 `KeyHandler.mm`、`InputState.swift`、`InputMethodController*.swift`；兩邊的狀態機結構不同（`P2` §0.4），不合併。
- **Qt 詞彙管理員**：`src/VocabularyManager/` 的 `MainWindow`、`main.cpp`、`ManagerSettings`（讀 fcitx 的 INI 設定）。它為了「和輸入法分屬兩個程序」而存在（`P4` §1.5、§1.6），Mac 的詞彙管理員在輸入法程序內，不需要。其中 `LearnedVocabulary.{h,cpp}` 不依賴 Qt，若 Mac 改寫詞彙管理員時想共用分頁規則，再搬進 `src/SmartLearning/`。
- **Mac InputMethodKit 介面**：`InputMethodController*.swift`、`FloatingCompositionPanel`（`FC`，除了清單判斷）、`Packages/CandidateUI`（含格狀候選窗 `GC`）、`Packages/TooltipUI`、`Packages/NotifierUI`、`AppDelegate` 的狀態列與選單、`NonModalAlertWindowController`、偏好視窗。`SymbolGrid` 是 Linux 在 fcitx 面板上模擬 `GC` 的權宜做法，Mac 已有真正的格狀候選窗，不採用。
- **SwiftUI 詞彙管理員**：`VM:391-` 的 `VocabularyManagerView`、`VocabularyManagerWindowController`，以及 `VocabularyModel` 的介面狀態（選取、編輯、訊息）。只有 §3.9 列的檔案操作與讀音帶出換成共用模組。
- **平台服務**：`NSSpellChecker`、Foundation 的字元分類、OpenCC、FSEvents、iCloud 下載、`SCDynamicStoreCopyComputerName`、`NSUserDefaults` 的裝置 ID 都留在 Mac 端，透過 §2.3 的接縫注入；Linux 的字表、ICU、`gethostname` 留在 Linux 端。
- **偏好設定**：Mac `Preferences.swift` 與 fcitx 設定各自保留，名稱與預設值對齊即可（設計文件「偏好設定預設值」）。

---

## 5. 未確認事項

1. Foundation 的 `writeToFile:atomically:`／`String.write(toFile:atomically:)` 是否保留原檔權限。§3.3 之前在 Mac 上實測（寫一個 0644 的 `data.txt`，用舊版學一個詞，看權限）。
2. C++ `ChineseNumbers` 與 Swift `Packages/ChineseNumbers` 的輸出是否逐字相同（`P3` §12 第 6 點），§3.2 之前寫比對測試。
3. `NSSpellChecker.guesses` 能否由 `aocd` 找到 associated（`P3` §12 第 4 點）。不影響採用，只影響要不要把 `guesses` 放進字典介面。
4. 主執行緒同步寫選字模型檔（最多 5000 筆）的時間，§3.8 之前量測。
5. `localizedCaseInsensitiveContains` 與 Linux ICU 比對的差異（`P3` §12 第 7 點）。照 §2.3 選項 A 接 Foundation 就不影響 Mac。
6. 共用模組的單一來源要用複製腳本、`git subtree` 還是獨立 repo（§2.1），第一次採用前決定。
