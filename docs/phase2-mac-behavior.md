# Phase 2 偵察：macOS 中英混打相關功能行為規格

來源：Mac fork `/Users/roger/Developer/McBopomofo` @ `7446fd0`（以下 `KH` = `Source/KeyHandler.mm`，`EW` = `Source/EnglishWords.swift`，`PF` = `Source/Preferences.swift`，`AD` = `Source/AppDelegate.swift`，`IMC` = `Source/InputMethodController.swift`，`UG` = `Source/UserGuide.html`，`LMM` = `Source/LanguageModelManager.mm`，`CS` = `Source/CommonSentences.swift`）。
目標：fcitx5 port `/Users/roger/Developer/fcitx5-mcbopomofo`（branch `smart-learning` @ `88c99d8`），以下 `fKH` = `src/KeyHandler.cpp`，`fKHh` = `src/KeyHandler.h`，`fMB` = `src/McBopomofo.cpp`，`fMBh` = `src/McBopomofo.h`。

所有行號都是上述 commit 的行號。無法確定者標 **UNKNOWN**。

---

## 0. 共通基礎（所有功能共用）

### 0.1 Reading 編碼（`McBopomofoLM`，fcitx 版與 Mac 版 `.h` 完全相同，`.cpp` 只多一行 `#include <iterator>`）

- `kLiteralKeyPrefix = "_literal_"`（`Source/Engine/McBopomofoLM.h:106`）：reading `"_literal_" + text` 的唯一候選就是 `text`，分數 0（`McBopomofoLM.cpp:178-184`）。`isLiteralKey` 要求 text 非空，且 text 為 `"-"` 或不含 `-`（`McBopomofoLM.cpp:50-53`，因為 grid 會查 `_literal_a-_literal_b` 這種組合 key，必須拒絕）。`hasUnigrams` 同理（`McBopomofoLM.cpp:400-402`）。
- `kEnglishKeyPrefix = "_english_"`（`McBopomofoLM.h:110`）：`"_english_" + word`，word 必須全部是 ASCII 字母（`isEnglishKey`，`McBopomofoLM.cpp:43-48`）。候選第一個是 word 本身（分數 0），其後是 `englishCompletionProvider_(word)` 回傳且 `!= word` 者（分數 -1）（`McBopomofoLM.cpp:160-176`）。provider 由 `setEnglishCompletionProvider` 注入（`McBopomofoLM.cpp:286-289`）；Mac 在 `KH:205-214` 以 `EnglishWords.completions(for:)` 注入。
- key `" "`：永遠有一個值為 `" "` 的 unigram（`McBopomofoLM.cpp:152-157`、`396-399`）。
- 英文字母一律以「一個字母一個 reading」存成 `_literal_x`（`_insertLiteralText`，`KH:688-701`：依 composed character sequence 拆字，每個都 `insertReading("_literal_" + ch)`，全部插完才 `_walk` 一次）。英文聯想（Tab）選定後整個字變成單一 reading `_english_word`。
- 自動空白 reading 是 `kAutoSpaceReading = "_literal_ "`（`KH:53`）。注意：英文模式下使用者自己按的空白也是 `_insertLiteralText(@" ")` → 同一個 reading `_literal_ `，**兩者無法區分**。

### 0.2 Reading 分類函式（`KH:51-92`，都是純函式，適合直接搬到純 C++ 模組）

1. `IsEnglishReading(r)`（`KH:57-64`）：`r` 以 `_english_` 開頭 → true；或 `r.size() == len("_literal_")+1` 且以 `_literal_` 開頭且最後一個 byte `isalpha` → true。（`_letter_X`、`_punctuation_...` 都不算英文。）
2. `IsHalfWidthAlphanumericReading(r)`（`KH:67-79`）：`IsEnglishReading(r)` → true；否則若 `r` 是 `_literal_` + 1 byte 或 `_numpad_` + 1 byte，回傳該 byte `isdigit`。
3. `IsChineseReading(r)`（`KH:83-92`）：若 `r` 是 `_literal_` + 剛好 3 bytes 且首 byte 在 `0xE3..0xE9`（常用句等直接放入的中文字）→ true；否則 `!r.empty() && r[0] != '_' && r != " "`（即注音 syllable）。
   - 推論：`_literal_３`（全形數字，首 byte 0xEF）既不是中文也不是數字/英文。

### 0.3 KeyHandler 新增狀態（`KH:113-126`）

- `std::string _rawKeysOfReading`：目前注音 reading 實際按下的鍵。BPMF 分支每次 `combineKey` 後 append（`KH:1246-1249`，buffer 為空時先 clear）。
- `BOOL _smartEnglishMode`、`BOOL _smartEnglishDigitsOnly`。
- `BOOL _lastKeyWasTrimmingEsc`。
- `_rawKeysOfCurrentReading`（`KH:671-685`）：buffer 空 → `""`；用 `_rawKeysOfReading` 重組一個暫存 buffer，若 `composedString` 與目前 buffer 相同就回傳 raw keys，否則（例如 Backspace 後不同步）回傳 `keyboardLayout()->keySequenceFromSyllable(syllable())`（canonical 按鍵序列）。
- `clear`（`KH:950-958`）會 `_setSmartEnglishMode:NO`。

fcitx 對照：`fKHh:327` 只有 `reading_`，沒有 raw keys；`fKH:757-761` `reset()`；`fKH:763-766` `clearReading()`。這三個 flag 與 `rawKeysOfReading_` 都要新增，`reset()` 要清 smart mode。

### 0.4 fcitx 結構差異（影響所有 wiring）

- fcitx `KeyHandler::handle`（`fKH:117-580`）不處理候選窗狀態（由 `fMB:946-1004` `handleCandidateKeyEvent` 處理），Mac 的 smart 區塊位於候選窗處理之後、BPMF 之前（`KH:1146-1233`）。fcitx 對應插入點：`fKH:143-146`（`simpleAscii` 計算）之後、`fKH:147` 之前（`keepInvalidSyllableForFurtherInput_` 區塊之前）。
- `simpleAscii` 在 ctrl/shift/numpad 時為 `'\0'`（`fKH:143-146`）；大寫字母由 `MapFcitxKey` 以 `asciiKey('A', shift=false)` 傳入（`fMB:93-95`、`src/Key.h` 註解），所以 `key.ascii` 在 `'A'..'Z'` 即可判斷大寫。Alt/Super 已在 `fMB:899-902` 過濾。
- **Enter 差異**：Mac `composeReading |= (!buffer.isEmpty() && (charCode == 32 || charCode == 13))`（`KH:1290`），Enter 也會組字；fcitx 只有 Space（`fKH:181-184`），Enter + 未完成 reading 會 error（`fKH:351-355`）。Smart 規則「Enter + 不成字 reading → 變英文後送出」（`KH:1300-1309`）需要 fcitx 在 `fKH:351` 之前另外處理。
- **Caps Lock 差異**：Mac 在 smart 區塊之前就把 Caps Lock 下的鍵以小寫直接送出（`KH:1049-1071`）；fcitx 在 `fMB:906-914` 若 `capsLockAllowChineseInput` 為 false 就 reset 並放行。兩邊都讓 Caps Lock 繞過 smart。
- **Hsu 數字進組字區**（Mac learning.7，`KH:1578-1596`：`_numpad_<d>` 並 override 成半形數字）fcitx **沒有**；fcitx 只有數字鍵盤的 `_numpad_`（`fKH:556-562`）。`IsHalfWidthAlphanumericReading` 認得 `_numpad_` 數字，因此「Hsu 中打數字後標點半形」「數字不加空格」等行為在 fcitx 需要先補這段（不在本次 8 項內，但為依賴）。
- **符號鍵無標點時直接放符號**（`KH:1645-1651`）fcitx 也沒有，該分支同樣使用 `_isAfterHalfWidthAlphanumeric` 決定全/半形。
- **測試 LM**：fcitx `src/KeyHandlerTest.cpp:117` 用 `ParselessLM`，它不認 `_literal_` / `_english_` / `" "`。KeyHandler 層級的 smart 測試必須改用 `McBopomofoLM`（或包一層），否則所有 `insertReading("_literal_x")` 都會失敗。
- Tooltip：fcitx `InputStates::NotEmpty::tooltip` 是 `const std::string`（`src/InputState.h:93`），只能在建構時給；`buildInputtingState()`（`fKH:1582-1592`）需要加一個可帶 tooltip override 的版本。顯示位置是 `setAuxDown`（`fMB:2061`）。
- 字串在地化：`KeyHandler::LocalizedStrings`（`fKHh:355-385`），實作在 `fMB:354-404`，翻譯檔 `po/zh_TW.po`。

---

## 1. 智慧中英切換（SmartChineseEnglishSwitchEnabled）

- Mac key：`SmartChineseEnglishSwitchEnabled`，預設 **true**（`PF:40`、`PF:351-352`）。UI 文字：「智慧中英切換：不按 Shift、Caps Lock 也能打英文（空白、符號或倒退鍵回到中文）」（`zh-Hant.lproj/Localizable.strings:278-279`）。
- 使用說明：`UG:140-163`。

### 1.1 可用條件 `_isSmartSwitchingAvailableForInput`（`KH:648-653`）

全部成立才啟用：
1. `Preferences.smartChineseEnglishSwitchEnabled`
2. `_inputMode == InputModeBopomofo`（Plain Bopomofo 不啟用）
3. `keyboardLayout != HanyuPinyin`
4. 沒按 Control、Command、Option，且不是數字鍵盤（`!isNumericPad`）

注意：按住 Control 時 `smartSwitching == NO`，**英文模式區塊整段被跳過、模式不會被關掉**（`KH:1150-1151`），例如英文模式中 Ctrl+`,` 會走 `_ctrl_punctuation_` 打出「，」而 `_smartEnglishMode` 仍為 YES。

### 1.2 英文模式中（`smartSwitching && _smartEnglishMode`，`KH:1151-1186`）

依序：
1. 若 `!_smartEnglishDigitsOnly` 且 `_convertTrailingEnglishToChineseWithKey:charCode` 成功 → return（見 §2）（`KH:1152-1154`）。
2. `charCode == 32`（空白）（`KH:1155-1165`）：
   - 若 `!_smartEnglishDigitsOnly`：先 `_fixCaseOfEnglishWordBeforeCursor`（§5），再插入 `_literal_ `。
   - 若只有數字：**不插空白**（例：`2026` 空白 `年` →「2026年」）。
   - 兩者都 `_setSmartEnglishMode:NO`，回 Inputting。
3. `charCode < 0x80 && isprint(charCode)`（`KH:1166-1175`）：`_insertLiteralText:input.inputText`（原樣 ASCII，含大寫、數字、符號）。
   - 若 `isalnum`：`_smartEnglishDigitsOnly = digitsOnly && isdigit(c)`，維持英文模式。
   - 否則（符號）：插入符號後 `_setSmartEnglishMode:NO`（符號本身是半形原樣，**不經標點表**）。
   - 之後呼叫 `_appendArithmeticResultAfterKey`（`=` 算式，與本功能無關）。
4. Tab 且非 Shift（`KH:1177-1184`）：`_enterEnglishCompletionWithInput`（英文聯想，見 §1.6），失敗則 `errorCallback`，一律 return YES。
5. 其他鍵（Backspace、Enter、Esc、方向鍵…）：`_setSmartEnglishMode:NO` 後**往下照一般流程處理**（`KH:1185-1186`）。所以 Backspace 刪一個字母之後已回到中文模式，再打字母會當注音。

### 1.3 中文模式中進入英文的觸發條件（`smartSwitching && !_smartEnglishMode`，`KH:1187-1233`）

依程式順序：

1. **Tab 接在未完成 reading 後**（`KH:1188-1199`）：Tab、非 Shift、reading 非空，且 `_rawKeysOfCurrentReading` 全是字母 → `_switchToSmartEnglishWithText:keys`，然後嘗試 `_enterEnglishCompletionWithInput`，失敗就回 Inputting。（例：Hsu `hel` = ㄏㄧㄥ，Tab → hello 聯想。）
2. **大寫字母**（`KH:1201-1206`）：`'A' <= c <= 'Z'`，且 `!switchInputSourceUponShiftLetterKeyComboEnabled` 且 `letterBehavior == 0`（0 = 直接輸出大寫，`PF:580-585`）→ `_switchToSmartEnglishWithText:(rawKeysOfCurrentReading + c)`。組字區為空時也成立（`ChatGPT` 從 C 開始）。
   - fcitx 對應：`config_.shiftLetterKeys == ShiftLetterKeys::DirectlyOutputUppercase`（`fMBh` ShiftLetterKeys 選項；`fKH:787-789` `putLowercaseLettersToComposingBuffer_` 為 false）。fcitx 沒有 switchInputSourceUponShiftLetterKeyCombo。
3. 以下需 `reading 非空 && charCode < 0x80 && isprint && charCode != 32`（`KH:1208`）：
   - 3a. **注音位置被取代**（`KH:1209-1218`）：`!skipBpmfHandling`（非 reserved key、非 Control，`KH:1147`）且 `isValidKey(c)`：複製 buffer `combineKey(c)`，若結果 `!hasToneMarker()` 且 `componentCount(new) <= componentCount(old)`（`_componentCount` = consonant + middleVowel + vowel 個數，`KH:664-667`）→ `_switchToSmartEnglishWithText:(rawKeys + c)`。例：標準鍵盤 `app`（ㄇㄣ 再按 ㄣ）。
   - 3b. **未完成注音接數字或符號**（`KH:1219-1231`）：`else if (!isUppercaseLetter)`（即 key 不是合法注音鍵、或 skipBpmfHandling）→ `keys = rawKeys`；清 reading；`_insertLiteralText:keys`；
     - 若 `isalnum(c)`（數字，或該配列不用的字母）→ `_switchToSmartEnglishWithText:c`（**英文模式的 `digitsOnly` 只看這個 c**，所以 `ok3` 之後 digitsOnly == YES，空白不插空白也不修大小寫——邊界行為，照搬）。
     - 否則（符號）→ `_insertLiteralText:input.inputText`（半形原樣），**不進英文模式**（例：`ok?`）。
4. **組出的 syllable 不成字**（BPMF 組字段，`KH:1287-1314`）：`composeReading` 成立但 `!_languageModel->hasUnigrams(reading)` 且 `smartSwitching`（注意此處用的是本鍵算出的 `smartSwitching`）：
   - `keys = rawKeysOfCurrentReading`（tone 鍵已在 `KH:1248` append 進去；空白/Enter 不會 append）；清 reading。
   - 空白：插 keys → `_fixCaseOfEnglishWordBeforeCursor` → 插 `_literal_ ` → 回 Inputting（**不進英文模式**）（`KH:1300-1307`）。例：`you` + 空白（ㄗㄧㄟ 不是字）。
   - Enter：插 keys → `_handleEnterWithState`（連同英文一起送出，不修大小寫）（`KH:1308-1309`）。
   - 其他（tone 鍵是字母或數字）：`_switchToSmartEnglishWithText:keys`（`KH:1311-1313`）。
   - smart 不可用時維持原行為：error、`keepReadingUponCompositionError` 等（`KH:1316-1328`）。

`_switchToSmartEnglishWithText:`（`KH:937-948`）：清 reading 與 rawKeys；`digitsOnly` = text 全為數字；`_insertLiteralText:text`（若首字是字母且前面是中文 → 先插自動空白，§3）；`_setSmartEnglishMode:YES`；`_smartEnglishDigitsOnly = digitsOnly`。

### 1.4 回到中文的方式

1. 英文模式按空白（§1.2-2）。
2. 英文模式按任何可列印符號（§1.2-3）。
3. 英文模式按任何非可列印鍵（Backspace/Enter/Esc/方向鍵…）（§1.2-5）。
4. `_convertTrailingEnglishToChineseWithKey` 成功（§2）。
5. Esc 修剪後游標前不再是字母（`KH:1809-1815`）。
6. Tab 帶入常用句（`KH:1135-1144`）。
7. `clear`（commit、Enter、reset 等）。
- `_setSmartEnglishMode` 只有值改變時才通知 `ModeStatusBar.shared.setSmartEnglishMode`（`KH:655-662`）→ §7。

### 1.5 英文的儲存與學習

- 每個字母一個 `_literal_x` reading（§0.1）。組字區顯示為原樣字母，commit 時與中文一起輸出。
- Commit 前 `_learnFromCommittedComposition`（`KH:405-443`；僅 `InputModeBopomofo`）掃 `_latestWalk.nodes`：連續的「reading 以 `_literal_` 開頭且 value 是單一 ASCII 字母」或「reading 以 `_english_` 開頭」節點串成一個字；遇到其他節點就斷字。收集後 `[EnglishWords record:]`（§5.3）。
- 呼叫點：force commit `KH:970`、空白在句尾 commit `KH:1406`、反引號文字範本前 commit `KH:1565`、Enter `KH:2188`。
- fcitx 對應 commit 點：Enter `fKH:440-447`、Shift+Space/空白 commit `fKH:273-290`（`fKH:281`）、`handleForceCommitAndReset` `fKH:949-956`、大寫字母 commit `fKH:546-551`（smart 啟用時不會走到）。

### 1.6 英文聯想（Tab）——與 smart 模式綁在一起，列入參考

`_enterEnglishCompletionWithInput`（`KH:808-845`）：游標前連續 `_literal_<letter>` 至少 2 個 → key = `_english_` + word；`getUnigrams(key).size() < 2` → NO；否則刪掉那些字母 readings、插入 key、walk，必要時 cursor-1，進入候選窗。`completions(for:)` 依賴 NSSpellChecker completions/guesses（`EW:52-101`）。**本次 8 項未要求，若 phase 2 不做，Tab 在英文模式應維持 error**（`KH:1180-1182`）。

### 1.7 邊界情形整理

- 很短的英文可能剛好是合法注音（Hsu `a`+空白 →「疵」）；說明書要求改用 Shift/Caps Lock（`UG:162`）。
- 游標不在尾端時，所有插入都在游標處（`insertReading` 依 cursor），trailing run 判斷都以 cursor 為準。
- 數字鍵盤、Control 一律不觸發。
- Hanyu Pinyin 配列不觸發。

### 1.8 fcitx 掛點與模組邊界

- 掛點：
  - 英文模式區塊與中文模式觸發 1–3：`fKH:146` 與 `fKH:147` 之間（`simpleAscii` 之後、BPMF 之前）。Mac 用 `charCode` 判斷，fcitx 要用 `key.ascii` + `!key.ctrlPressed && !key.isFromNumberPad`（Shift+Space 這類 `shiftPressed` 組合鍵 `key.ascii` 仍有值，需另外判斷）。
  - rawKeys 記錄：`fKH:168-169`（`reading_.combineKey` 處）。
  - 觸發 4（不成字）：`fKH:189-205`（`!lm_->hasUnigrams(syllable)` 分支，在 `errorCallback()` 之前分流）。
  - Enter + 未完成 reading：`fKH:351-355`。
  - 成功組字前的自動空白：`fKH:207-208`（§3）。
  - reset：`fKH:757-761`。
  - 學習英文字：§1.5 所列 commit 點。
  - 設定：`fMBh` `FCITX_CONFIGURATION`（`fMBh:102-240`）新增 `SmartChineseEnglishSwitchEnabled`（預設 true），`fMB:775-830` activate 時 `keyHandler_->setSmartChineseEnglishSwitchEnabled(...)`。
- 純 C++ 模組建議（`src/SmartLearning/MixedInput.{h,cpp}`，只依賴 `MandarinLib`）：
  - `bool IsEnglishReading(const std::string&)`、`IsHalfWidthAlphanumericReading`、`IsChineseReading`（逐字搬 `KH:57-92`）。
  - `enum class SmartTrigger { None, ComponentReplaced, ... }` 或較簡單地提供 `bool KeyReplacesComponent(const BopomofoReadingBuffer& buf, char key)`（`KH:1212-1214` 的判斷）與 `std::string RawKeysOfCurrentReading(const BopomofoReadingBuffer&, const std::string& rawKeys)`（`KH:671-685`）。
  - 狀態機本身（mode / digitsOnly）留在 KeyHandler，因為它要操作 grid；但「給 readings + cursor 計算要刪幾個、插什麼」都做成純函式回傳 plan，方便 gtest。

---

## 2. 英文後直接接中文（`_convertTrailingEnglishToChineseWithKey`，`KH:852-933`）

無獨立偏好；屬於 smart 模式。只在英文模式且 `!_smartEnglishDigitsOnly` 時、每個鍵最先嘗試（`KH:1152`）。說明：`UG:159`。

### 2.1 條件（全部依序）

1. `key >= 0x80` → NO（`KH:854-856`）。
2. `run` = 游標前連續的 `_literal_<alpha>` readings 的字母（不含 `_english_` 聯想字）（`KH:857-870`）。
3. 單字檢查（`KH:871-881`）：
   - key 是空白：`isWord(run)` → NO（整串是單字就讓空白照英文處理）。
   - key 是字母（`isalpha`）：`isWordOrPrefix(run + key)` → NO（例：`helpf` 仍可能是 helpful）。
   - key 是數字（標準鍵盤 tone 鍵 3/4/6/7）：不做單字檢查。
4. `for count = min(3, run.size()); count >= 1; count--`（**最長優先**，`KH:884`）：
   a. `prefix = run[0 .. size-count)`；`prefix.size() < 2` → continue（`KH:885-888`）。
   b. `keys = run` 最後 `count` 個字母；以目前配列新建 buffer，每個 key 都要 `isValidKey`，逐一 `combineKey`（`KH:889-897`）。
   c. 必須：buffer 非空、**沒有** tone marker、且 `keyboardLayout()->keySequenceFromSyllable(buffer.syllable()) == keys`（**按鍵序列必須是該 syllable 的 canonical 打法**）（`KH:898-903`）。Hsu 註解：`hellone` 的 `one` 組起來是 ㄋㄧㄡ，但 Hsu canonical 是 `neo`，不相等 → 跳過；改取 `ne`（ㄋㄧ）。
   d. key 不是空白：`isValidKey(key)` 且 `combineKey(key)` 後必須 `hasToneMarker() && !hasToneMarkerOnly()`（`KH:904-912`）。key 是空白：視為一聲，不 combine。
   e. `reading = buffer.syllable().composedString()`，`_languageModel->hasUnigrams(reading)` 必須為真（`KH:913-916`）。
   f. `EnglishWords.isWord(prefix)` 必須為真（`KH:917-919`）。（PDF 這類全大寫縮寫由 isWord 規則 2 成立。）
5. 成功時（`KH:920-930`）：刪 `count` 個 readings；`_setSmartEnglishMode:NO`；`_fixCaseOfEnglishWordBeforeCursor`（§5，修 prefix 的大小寫）；`_insertAutoSpaceIfNeededBeforeEnglish:NO`（§3，英文後接中文插空白）；`insertReading(reading)`；`_walk`；回 Inputting。
   - **不**跑 user override suggestion、**不**觸發聯想詞（與一般組字 `KH:1335-1344`、`KH:1351-1353` 不同）；`_rawKeysOfReading` 不變。

例：`hello` + ㄏㄠˇ（Hsu `hwf`；標準 `cl3`）→「hello 好」；`PDF` + ㄉㄤˇ →「PDF 檔」（`UG:159`）。

### 2.2 Hsu 配列注意

- Hsu 的 tone 鍵是字母（ˊ=d、ˇ=f、ˋ=j、˙=s；空白一聲），所以 §2.1-3 的 `isWordOrPrefix(run + key)` 在 Hsu 下是主要的防誤判；標準鍵盤 tone 是數字鍵，不檢查。
- Hsu 同一鍵可代表聲母或韻母，`combineKey` 會重新解讀；§2.1-4c 的 canonical 比對確保只接受「本來就是這樣打」的 syllable。fcitx 的 `Mandarin.h/.cpp` 與 Mac 逐字相同（已 diff 確認），`keySequenceFromSyllable` 在 `src/Engine/Mandarin/Mandarin.h:207`。

### 2.3 fcitx 掛點與模組

- 掛點：英文模式區塊第一步（§1.8）。
- 純函式建議：
  ```cpp
  struct TrailingConversion { size_t lettersToDelete; std::string reading; };
  std::optional<TrailingConversion> PlanTrailingEnglishToChinese(
      const std::string& run, char key,
      const Formosa::Mandarin::BopomofoKeyboardLayout* layout,
      const std::function<bool(const std::string&)>& hasUnigrams,
      const EnglishWordsQuery& words);   // isWord / isWordOrPrefix
  ```
  輸入只有 run、key、layout、兩個 predicate；可以完全在 gtest 中以假字典測試 `hellone`、`helpf`、`PDF`+ㄉㄤˇ 等。

---

## 3. 中英文之間自動加空格（AutoSpaceBetweenChineseAndEnglish）

- Mac key：`AutoSpaceBetweenChineseAndEnglish`，預設 **true**（`PF:41`、`PF:356-357`）。說明 `UG:168-177`。

### 3.1 插入規則 `_insertAutoSpaceIfNeededBeforeEnglish:(BOOL)beforeEnglish`（`KH:705-720`）

1. 偏好關閉或非 `InputModeBopomofo` → NO。
2. cursor == 0 → NO。
3. `previous = readings[cursor-1]`：
   - `beforeEnglish == YES`：`IsChineseReading(previous)` 才插。
   - `beforeEnglish == NO`（即將插中文）：`IsEnglishReading(previous)` 才插。
4. 插入 `_literal_ `（不 walk；呼叫端負責）。

呼叫點：
- 英文前：`_insertLiteralText` 中，若 text 首字 `< 0x80 && isalpha` 才檢查（`KH:690-693`）。**數字開頭不插**（「2026年」「3個」），符號開頭不插。所有 `_insertLiteralText` 都適用，包括 `_switchToSmartEnglishWithText`、大小寫修正重插（見 §5 邊界）、常用句 Tab 補完（`KH:1140`）。
- 中文前：一般組字成功時 `KH:1332`（在 `insertReading(reading)` 前）；`_convertTrailing...` 成功時 `KH:926`。
- 中文接在數字後：previous 是數字 → `IsEnglishReading` false → 不插。
- 標點：不插（標點走 `_handlePunctuation`，不經上述函式）。

### 3.2 Backspace 連同空格刪除 `_removeAutoSpaceAfterDeletingEnglish`（`KH:724-735`）

在 `_handleBackspaceWithState`（`KH:2032-2063`）：reading 為空且 cursor > 0 時，先記 `deletingEnglish = IsEnglishReading(readings[cursor-1])`，刪除後若 `deletingEnglish` 才呼叫（`KH:2041-2047`）：
1. 偏好關閉、或 `cursor < 2`、或 `readings[cursor-1] != "_literal_ "`、或 `!IsChineseReading(readings[cursor-2])` → 不動。
2. 若 cursor 之後還有英文（`cursor < size && IsEnglishReading(readings[cursor])`）→ 不動。
3. 否則再 `deleteReadingBeforeCursor()` 刪空白。

邊界：
- 只有 Backspace；Delete 鍵（`_handleDeleteWithState`）不處理。
- 空白無法區分是自動插入或使用者在英文模式按的（§0.1），只要符合「中文、空白、（剛刪掉最後一個英文字）」就會刪。
- 刪的是英文字母或 `_english_` 聯想字都算。

### 3.3 fcitx 掛點與模組

- 插入：在 fcitx 新增的 `insertLiteralText()`（對應 `_insertLiteralText`）中；中文前插入 `fKH:207`（`grid_.insertReading(syllable)` 前）。
- Backspace：`handleDeleteKeys` `fKH:1108-1110`（`key.ascii == Key::BACKSPACE && grid_.cursor() > 0` 分支，刪除前記 `deletingEnglish`，刪除後呼叫）。
- 設定：新增 `AutoSpaceBetweenChineseAndEnglish`（預設 true）。
- 純函式：`bool AutoSpaceNeeded(const std::vector<std::string>& readings, size_t cursor, bool beforeEnglish)`、`bool ShouldRemoveAutoSpaceAfterDeletingEnglish(const std::vector<std::string>& readings, size_t cursor)`。

---

## 4. 英文、數字後的標點用半形（HalfWidthPunctuationAfterEnglish）

- Mac key：`HalfWidthPunctuationAfterEnglish`，預設 **true**（`PF:42`、`PF:361-362`）。說明 `UG:178-184`。

### 4.1 判斷 `_isAfterHalfWidthAlphanumeric`（`KH:774-781`）

全部成立：
1. 偏好開啟；`_inputMode == InputModeBopomofo`；reading buffer 為空。
2. cursor > 0 且 `IsHalfWidthAlphanumericReading(readings[cursor-1])`（英文字母 `_literal_x`、聯想字 `_english_...`、半形數字 `_literal_<d>`、`_numpad_<d>`）。
- `_letter_X`（Shift+字母放小寫進組字區的 letterBehavior 1）**不算**；中文、全形字元不算。

### 4.2 使用處

1. 標點前綴選擇（`KH:1601-1609`）：
   - `input.isControlHold` → `_ctrl_punctuation_`（**Control 永遠優先 → 全形**，例 Ctrl+`,` =「，」，`UG:184`）。
   - 否則 `halfWidthPunctuationEnabled || _isAfterHalfWidthAlphanumeric` → `_half_punctuation_`。
   - 否則 `_punctuation_`。
   - 之後先試 `prefix + layoutName + "_" + c`，再試 `prefix + c`（`KH:1610-1622`）。**Mac 選定半形前綴後不會回退到全形**；兩個都沒有 unigram 時往下走到符號分支。
2. 「無標點的符號鍵直接放符號」分支（`KH:1645-1651`）：同條件下放原樣 ASCII，否則放 `c + 0xFEE0`（全形）。fcitx 無此分支（§0.4）。
3. Plain Bopomofo 的標點（`KH:3000-3008`）**不**使用此判斷。

實際觸發情境（因為英文模式中的符號在 §1.2-3 就被當英文原樣插入，不經標點表）：中文模式下游標前是英文/數字時，例如 Backspace 離開英文模式後、選了英文聯想字之後、Hsu 打數字（`_numpad_`）之後、數字模式按空白離開之後、smart 關閉但前面是數字時。「價格是300.」→ 半形「.」；「好,」→「，」（`UG:180-183`）。

### 4.3 字典依賴

`_half_punctuation_*` unigram 已存在於資料檔：fcitx `data/data.txt` 有 76 筆 `_half_punctuation_`（Mac `Source/Data/BPMFPunctuations.txt` 亦為 76 筆）。

### 4.4 fcitx 掛點與模組

- 掛點：`fKH:492-509`。目前 fcitx 邏輯：ctrl → `_ctrl_punctuation_`（`fKH:492-495`）；`halfWidthPunctuationEnabled_` → 試兩個 `_half_punctuation_` 鍵（`fKH:497-509`），**失敗會回退到全形** `_punctuation_`（`fKH:511-521`）——與 Mac 不同。只要把 `fKH:497` 改為 `halfWidthPunctuationEnabled_ || isAfterHalfWidthAlphanumeric()`；是否要改成「不回退」需決定（建議比照 Mac：選了半形就不回退，否則 `!`、`?` 之外沒有半形對應的鍵可能被打成全形）。注意 fcitx 半形 key 格式 `_half_punctuation_<Layout>_<c>`（`fKH:498-500`）與 Mac `_currentLayout`（`KH:977-982`，`name + "_"`）一致。
- `isAfterHalfWidthAlphanumeric()` 需要 `reading_.isEmpty()` 判斷，留在 KeyHandler；判斷本體用 §0.2 純函式。
- 設定：新增 `HalfWidthPunctuationAfterEnglish`（預設 true）。

---

## 5. 英文大小寫修正（FixEnglishWordCase）

- Mac key：`FixEnglishWordCase`，預設 **true**（`PF:43`、`PF:365-366`）。UI：「按空白時修正英文大小寫（如 iPhone）」。說明 `UG:185-195`。

### 5.1 `_fixCaseOfEnglishWordBeforeCursor`（`KH:739-770`）

1. 偏好關閉 → return。
2. 取游標前連續 `_literal_<alpha>` readings（不含 `_english_`），少於 2 個 → return。
3. `fixed = EnglishWords.preferredCase(for: word)`；若 `nil`、長度不同、或與原字相同 → return。
4. 刪掉該串 readings，`_insertLiteralText:fixed`。
   - 邊界：重插時 `_insertLiteralText` 會再檢查「前面是中文就插空白」；正常情況前面已是自動空白所以不會重複；但若當初沒插空白（例如偏好剛切換），修正時可能補插一個空白。

呼叫點（只有這三處）：英文模式按空白 `KH:1159`（digitsOnly 時不呼叫）；不成字 + 空白 `KH:1304`；英文後直接接中文 `KH:925`。
**Enter 不修正**（`_handleEnterWithState` `KH:2182-2196` 不呼叫）；標點、Esc 也不修正。

### 5.2 `preferredCase(for:)`（`EW:131-143`）

1. `word.count >= 2` 且 `word == word.lowercased()`（**必須是全小寫輸入**）否則 nil。
2. `loadIfNeeded()`（§5.3）。
3. 候選：`counts` 中 `value >= 2`、`key.lowercased() == word`、且 `isMixedCase(key)` 的鍵；取 count 最大者；同票取字典序**最小**的 key（`max(by:)` 比較子 `a.value != b.value ? a.value < b.value : a.key > b.key`）。
4. 否則查內建表 `wellKnownMixedCaseWords[word]`，沒有則 nil。
- `isMixedCase(w)`（`EW:125-127`）：第 2 個字元之後含大寫，且 `w != w.uppercased()`（iPhone、OpenAI 是；Apple、NASA 不是）。
- 內建表（`EW:105-121`，共 57 個）：iPhone iPad iPod iMac iCloud iMessage iTunes iOS iPadOS macOS watchOS tvOS visionOS AirPods AirDrop AirPlay AirTag MacBook FaceTime AppleCare HomePod CarPlay OpenAI ChatGPT DeepSeek GitHub GitLab YouTube LinkedIn PayPal WhatsApp TikTok WeChat PowerPoint OneDrive OneNote SharePoint FileMaker JavaScript TypeScript PostgreSQL MySQL MariaDB SQLite MongoDB WordPress WooCommerce PlayStation SmartThings DevOps eBay eSIM McDonald LaTeX PhD VoIP。以小寫為 key 存成字典。

### 5.3 學習英文字 store（`EnglishWords`，phase 2 需要最小版本）

依賴學習資料的部分：`preferredCase` 規則 3、`isWord` 規則 4、`isWordOrPrefix` 規則 2、`completions`。

- 常數（`EW:29-31`）：`minimumLength = 3`、`maximumCompletions = 20`、`reloadInterval = 60` 秒。
- 路徑：
  - 資料夾 `englishWordsFolderPath` = `<dataFolder>/learning/english`（`LMM:89`、`LMM:94`、`LMM:820-823`）。
  - 本機檔 `englishWordsDataPath` = 上述資料夾 + **與 user override model 檔同名**（`LMM:825-828`）= `<deviceID>-<ComputerName>.txt`（`LMM:686-699`；deviceID 是 UUID 前 8 碼存在 defaults `UserOverrideModelDeviceID`，`LMM:674-683`；ComputerName 中 `/ : \ \n \r \t` 換成 `_`，空則 `Mac`）。fcitx 對應：`SmartLearning::learningFilename(deviceID, deviceName)`（`src/SmartLearning/LearningPaths.h`），學習資料夾 `<userDataPath>/learning`（`fMB:614-621`）→ 英文資料夾 `<userDataPath>/learning/english`。
- 檔案格式（`EW:233-246`、`EW:273-285`）：每行 `word<TAB>count\n`；讀取時以 `\n` 分行、以 `\t` 切成**剛好 2 段**且 count 可轉 Int 才收；寫入時整檔重寫，每行 `"\(word)\t\(count)\n"` 後**排序**再串接，背景 queue atomically 寫入（並建立資料夾）。word 區分大小寫（`iPhone` 與 `iphone` 是不同 key）。
- `loadIfNeeded`（`EW:249-271`）：
  1. 若已載入且距上次 < 60 秒 → return。
  2. `localCounts` **只在第一次**讀本機檔（之後不再重讀，以記憶體為準）。
  3. `merged = localCounts`；列舉英文資料夾所有檔名 `hasSuffix(".txt")` 且 `!= 本機檔名` 的檔案，逐一 `read` 並把 count **相加**到 merged。（Mac **沒有**排除同步衝突副本、也沒排除點開頭檔名；fcitx 的 `otherDevicesLearningFiles` 會排除 `_Conflict`、`.sync-conflict-`、點開頭與非一般檔案——採用 fcitx 版較安全，屬刻意差異。）
  4. `counts = merged` 過濾掉 `RemovedEntries.removedTime(kind: "english", key: word) != 0` 的字（**只要被刪除過就永遠排除**，除非用 `add` 取消刪除紀錄）。
  5. `lastLoaded = now`。
- `record(_ words:)`（`EW:39-50`）：`loadIfNeeded`；每個 `word.count >= 3` 的字 `localCounts[word] += 1`、`counts[word] += 1`；有變更就 `save()`（只寫本機檔）。
  - 記錄的是**修正後**的字（因為修正已改寫 readings），所以使用者打全小寫 `butypos` 永遠學不到 `ButyPOS`；要用 Shift 打出 `ButyPOS` 兩次以上（`UG:193`）。
- `RemovedEntries`（`CS:29-86`）：檔案 `<dataFolder>/learning/removed.txt`（**沒有** device 後綴，各裝置共用一檔），每行 `kind<TAB>key<TAB>time`（time 整數秒；讀取用 `omittingEmptySubsequences: false` 切成剛好 3 段），60 秒重讀，`add` / `remove` 由詞彙管理員呼叫。最小版 phase 2 可唯讀 `english` kind。
- `add` / `remove` / `allWords`（`EW:192-215`）屬詞彙管理員 UI，phase 2 可不做。

### 5.4 fcitx 掛點與模組

- 掛點：§5.1 的三個呼叫點分別對應 fcitx 英文模式空白、`fKH:189` 不成字分支、§2 成功路徑。
- 純 C++ 模組：`src/SmartLearning/EnglishWords.{h,cpp}`
  ```cpp
  class EnglishLexicon {            // 拼字字典抽象，§5.5
   public:
    virtual ~EnglishLexicon() = default;
    virtual bool isWord(const std::string& lowercased) const = 0;
    virtual bool hasLongerWordWithPrefix(const std::string& lowercased) const = 0;
    virtual std::vector<std::string> completions(const std::string& prefix) const = 0;  // 聯想用，可選
  };
  class EnglishWordStore {
   public:
    EnglishWordStore(std::string deviceID, std::string deviceName,
                     std::shared_ptr<EnglishLexicon> lexicon,
                     std::function<double()> clock);   // 可注入時鐘測 60 秒重讀
    void setFolder(std::filesystem::path englishFolder, std::filesystem::path removedFile);
    void record(const std::vector<std::string>& words);
    bool save();                                       // 用 AtomicFile
    std::optional<std::string> preferredCase(const std::string& word);
    bool isWord(const std::string& word);
    bool isWordOrPrefix(const std::string& text);
  };
  ```
  讀寫用既有 `AtomicFile`、列舉用 `otherDevicesLearningFiles`；存檔排程可比照 `fMB:686-704` `scheduleLearningSave`。

### 5.5 字典依賴與 Linux 替代

- Mac 用 `NSSpellChecker`（`EW:166-169` `checkSpelling(of:lowercased, language:"en")`；`EW:182-186` `completions(forPartialWordRange:in:language:"en")`；聯想另用 `guesses`）。macOS 系統字典的實際詞條數與「使用者已學習拼字」是否被納入：**UNKNOWN**。專有名詞以小寫送檢（如 `london`）是否判為正確：**UNKNOWN**。
- Linux 建議：
  - `isWord`：libhunspell `Hunspell::spell()`（Debian 套件 `libhunspell-dev` + `hunspell-en-us`，字典 `/usr/share/hunspell/en_US.{aff,dic}`）。注意 hunspell 對小寫專有名詞會回 false，與 NSSpellChecker 可能不同（UNKNOWN）。
  - `isWordOrPrefix` / completions：hunspell **沒有前綴補完 API**。需要一份展開後的單字表做排序 + `lower_bound`：可在 build 時以 hunspell 的 `unmunch`（或 `wordforms`）把 `en_US.dic` 展開成小寫排序清單隨套件安裝，或執行期讀 `/usr/share/dict/words`（Debian `wamerican`）。兩者實際詞數未在本機驗證（**UNKNOWN**，需在 Debian 目標機確認）。
  - 建議：`EnglishLexicon` 的預設實作直接讀一份**隨套件出貨**的排序小寫字表（例如 `data/english-words.txt`，自 SCOWL/en_US 展開），單一來源同時滿足 `isWord` 與前綴查詢，且單元測試可用小字表；hunspell 改為選用。

---

## 6. Esc 清除亂碼（EscTrimsMistypedText）與連按兩下清除全部

- Mac key：`EscTrimsMistypedText`，預設 **true**（`PF:53`、`PF:417-418`）；舊鍵 `EscToCleanInputBuffer`，預設 **false**（`PF:52`、`PF:411-412`）。說明 `UG:120-137`、`UG:368-369`。UI：「按 ESC 只清除字尾打錯的亂碼，保留最後一個完整的中文或英文字（連按兩下清除全部）」。

### 6.1 連按偵測（`KH:988-989`）

`handleInput` 一開始：`escPressedTwice = _lastKeyWasTrimmingEsc && charCode == 27`，然後**每個鍵**都把 `_lastKeyWasTrimmingEsc = NO`。任何其他鍵介入都會打斷「兩下」。（注意：候選窗、Big5 等狀態的鍵也會先經過這兩行。）

Esc 到達 `_handleEscWithState` 前的路徑：英文模式中 Esc 會先 `_setSmartEnglishMode:NO`（§1.2-5）再往下；候選窗中的 Esc 在 `KH:1112-1114` 由候選窗處理（關窗），不會修剪。

### 6.2 `_handleEscWithState`（`KH:1820-1880`）

1. state 不是 `InputStateInputting` → return NO（例如第一下已清空成 Empty，第二下 Esc 放行給 App）。
2. 若 `escTrimsMistypedText`（**完全取代** `escToCleanInputBuffer`）：
   a. `!pressedTwice && _trimMistypedTextBeforeCursor` 為真 → `_lastKeyWasTrimmingEsc = YES`；若 reading 與 grid 都空 → `EmptyIgnoringPreviousState`；否則 Inputting，**只有在原 tooltip 為空時**才設 tooltip「再按一次 Esc 清除全部」（`Press Esc again to clear all`，`zh-Hant.lproj/Localizable.strings:345`）（`KH:1827-1838`）。
   b. `!pressedTwice` 且沒有東西可修剪 → `_lastKeyWasTrimmingEsc = YES`，Inputting **強制**設 tooltip（覆蓋既有 tooltip），不刪任何字（`KH:1840-1846`）——避免誤刪整句（`UG:134`）。
   c. `pressedTwice` → `clear` + `EmptyIgnoringPreviousState`（`KH:1848-1850`）。
3. 否則舊行為（`KH:1853-1879`）：`escToCleanInputBuffer` → 全清；否則只在 reading 非空時清 reading（reading 空時什麼都不做、仍回 YES）。

### 6.3 `_trimMistypedTextBeforeCursor`（`KH:1748-1818`）

1. reading buffer 非空 → 清 reading 與 rawKeys，`trimmed = YES`，**繼續**往下修剪 grid（`KH:1750-1755`）。（`下午 3 點 ㄒㄧ` → `下午 3 點`）
2. 迴圈 while cursor > 0，`last = readings[cursor-1]`：
   a. 保留並停止：`IsChineseReading(last)`、或 `_english_` 聯想字（`IsEnglishReading && !isLetter`）、或數字（`IsHalfWidthAlphanumericReading && !IsEnglishReading`，即 `_literal_<d>` / `_numpad_<d>`）（`KH:1769-1773`）。
   b. 不是字母 `_literal_<alpha>`（標點 `_punctuation_*`、`_half_punctuation_*`、空白、符號、全形 literal 等）→ 刪除，`trimmed = YES`，continue（`KH:1774-1778`）。**中文標點也會被刪**（reading 以 `_` 開頭）。
   c. 字母串 `run`（`KH:1780-1787`）：
      - `isWord(run)` → 停止（整串是單字就保留，2 個字母的單字如 `ok` 也算）。
      - 否則 `keep` = 從 `run.size()-1` 遞減到 **3** 找第一個 `isWord(run[0..length))` 的長度（2 字母前綴不算，因為太常被當單字，如 `lsqq` 的 `ls`）（`KH:1791-1799`）。
      - 刪除 `run.size() - keep` 個字母，`trimmed = YES`；`keep > 0` 則停止，否則 continue（繼續刪更前面的符號等）。（`hellojkq` → `hello`）
3. 若 trimmed：若游標前已不是字母 → `_setSmartEnglishMode:NO`；`_walk`（`KH:1809-1816`）。
4. 只處理游標**之前**；游標後不動（`UG:135`）。

依賴：`EnglishWords.isWord`（§5.5 字典 + 學習字）。

### 6.4 fcitx 掛點與模組

- 掛點：fcitx Esc 在 `fKH:306-329`；目前邏輯：非 NotEmpty → false；`escKeyClearsEntireComposingBuffer_` → 全清；否則清 reading。fcitx 預設 `EscKeyClearsEntireComposingBuffer = true`（`fMBh:153-155`，Smart Learning 預設設定 commit `d11d0ec`）。新增 `EscTrimsMistypedText`（預設 true，開啟時取代前者），連按 flag 在 `handle()` 開頭（`fKH:117` 後）設定/清除。注意 fcitx 用 `maybeNotEmptyState`（含 Marking）判斷，Mac 只接受 Inputting。
- 提示字串：`LocalizedStrings` 新增 `pressEscAgainToClearAll()`，`po/zh_TW.po` 加「再按一次 Esc 清除全部」。
- 純函式：
  ```cpp
  // 回傳要從 cursor 往前刪幾個 reading（不含注音 buffer）
  size_t PlanTrimBeforeCursor(const std::vector<std::string>& readings, size_t cursor,
                              const std::function<bool(const std::string&)>& isWord);
  ```
  KeyHandler 負責清 reading、刪 grid、tooltip、flag。

---

## 7. 中英提示與模式圖示（ChineseEnglishSwitchReminderEnabled、ShowChineseEnglishModeIcon、ShowFullHalfWidthModeIcon）

- Mac keys / 預設：
  - `ChineseEnglishSwitchReminderEnabled` **true**（`PF:39`、`PF:345-346`）
  - `ShowChineseEnglishModeIcon` **true**（`PF:47`、`PF:385-386`）
  - `ShowFullHalfWidthModeIcon` **false**（`PF:48`、`PF:389-390`）
  - 相關：`HalfWidthPunctuationEnable` **false**（`PF:51`、`PF:403-409`）
- 說明 `UG:310-316`。

### 7.1 切換提醒 `ChineseEnglishSwitchReminder`（`AD:497-551`）

- `notify(isEnglish:)`（`AD:505-513`）：偏好關閉 → 不做；否則 `NotifierController.notify` 顯示「ABC」（`English Mode`）或「中文」（`Chinese Mode`）（`zh-Hant.lproj/Localizable.strings:276-277`），位置在螢幕右上角短暫顯示（`UG:312`；NotifierController 顯示時長 **UNKNOWN**，未讀該檔）。
- 觸發：
  1. 小麥作用中按 Caps Lock：`IMC:259-264` flagsChanged 且 keyCode 為 CapsLock → `notify(isEnglish: capsLockOn)` 並 `ModeStatusBar.setCapsLockOn`。
  2. 在小麥與其他輸入法之間切換：監聽 `kTISNotifySelectedKeyboardInputSourceChanged`（`AD:515-538`），只有「是否為小麥」改變時才通知；`isEnglish = !selected || capsLock 開著`。小麥兩個模式之間切換（同 bundle 前綴）**不**提醒（`AD:546-551`）。
  - **智慧英文模式切換不會觸發提醒**（只有狀態列圖示會變）。

### 7.2 狀態列 `ModeStatusBar`（`AD:555-616`）

- 兩個 NSStatusItem，建立順序讓「全/半」在「中/英」左邊（`AD:563-566`）。
- `update()`（`AD:585-596`）：
  - 語言項：`isMcBopomofoSelected && showChineseEnglishModeIcon` 時顯示；文字 `isCapsLockOn || isSmartEnglishMode ? "英" : "中"`；無點擊動作。
  - 寬度項：`isMcBopomofoSelected && showFullHalfWidthModeIcon` 時顯示；文字 `halfWidthPunctuationEnabled ? "半" : "全"`；點擊切換 `HalfWidthPunctuationEnable`（`AD:611-614`，從狀態列點擊**不**跳通知）。
  - 「全/半」只反映全域半形標點設定，**不**反映 §4 的「英文後自動半形」。
  - 兩者都不顯示且從未建立時不建立 item。
- 更新來源：`setMcBopomofoSelected`（切換輸入法，同時重讀 Caps Lock，`AD:568-573`）、`setCapsLockOn`、`setSmartEnglishMode`（`KH:661`）、選單切換半形（`IMC:392-394`）。

### 7.3 fcitx 掛點與建議

- fcitx 已有「半形標點」狀態列 action：`halfWidthPunctuationAction_`（建立 `fMB:497-523`，activate 時加入狀態列 `fMB:736-741`），短文字為「Half/Full Width Punctuation」。可加一個選項讓 short text 改成「半」/「全」，或維持現狀。
- 「中/英」：建議新增 `fcitx::SimpleAction`（例如 `mcbopomofo-chinese-english-mode`），比照 `fMB:497-523` 註冊、在 `fMB:740` 附近 `statusArea().addAction`；KeyHandler 新增 callback `setOnSmartEnglishModeChanged(std::function<void(bool)>)`（比照 `fKHh:184-186` `setOnUserOverrideObserved`），engine 收到後 `setShortText("英"/"中")` + `update(context)`。也可改用 fcitx5 `InputMethodEngineV2::subModeLabelImpl` 讓托盤顯示子模式；fcitx 版本支援度 **UNKNOWN**（本機無 fcitx5 headers，`McBopomofoEngine` 目前繼承 `fcitx::InputMethodEngine`，`fMBh:264`）。
- Caps Lock：fcitx 在 `fMB:906-914` 才看到 CapsLock state；Caps Lock 鍵本身的 key event 是否送到 engine、以及當下 `rawKey().states()` 是切換前還是切換後的狀態：**UNKNOWN**（需在目標機實測）。
- 輸入法切換提醒：fcitx5 本身在切換輸入法時有顯示輸入法資訊的機制（全域設定），是否足以取代 Mac 的「ABC/中文」提醒：**UNKNOWN**。若要自己做，可用既有 `notifications()->call<fcitx::INotifications::showTip>(...)`（`fMB:511-519` 用法，timeout `kFcitx5NotificationTimeoutInMs = 1000`，`fMB:64`）。
- 設定：新增 `ChineseEnglishSwitchReminderEnabled`（true）、`ShowChineseEnglishModeIcon`（true）、`ShowFullHalfWidthModeIcon`（false）。
- 純 C++ 部分僅有「模式文字」的純函式：`std::string LanguageModeLabel(bool capsLock, bool smartEnglish)` → "英"/"中"，`WidthModeLabel(bool halfWidth)` → "半"/"全"；其餘屬 fcitx UI。

---

## 8. 鍵盤配列快速切換（注音－標準鍵盤／許氏鍵盤）

- Mac 偏好：`KeyboardLayout`（Int enum：standard 0、eten 1、hsu 2、eten26 3、hanyuPinyin 4、IBM 5），預設 **standard**（`PF:26`、`PF:186-209`、`PF:308-313`）。
- 選單（`IMC:70-85`）：輸入法選單**最上方**兩個項目，之後一條分隔線：
  1. 「注音－標準鍵盤」（`Bopomofo - Standard Keyboard`，tag = 0）
  2. 「注音－許氏鍵盤」（`Bopomofo - Hsu Keyboard`，tag = 2）
  - 目前配列打勾（`item.state = keyboardLayout == layout ? .on : .off`）。只有這兩個；其他配列仍在偏好設定。
- 動作 `selectKeyboardLayout`（`IMC:376-390`）：
  1. 從 sender（NSMenuItem 或含 `kIMKCommandMenuItemName` 的 dictionary）取 tag 轉 `KeyboardLayout`。
  2. `Preferences.keyboardLayout = layout`（**持久化**於 UserDefaults `KeyboardLayout`）。
  3. `keyHandler.syncWithPreferences()`（`KH:221-245` 依配列設 `_bpmfReadingBuffer` 的 layout）。
  4. `NotifierController.notify(message: item.title)`（顯示「注音－許氏鍵盤」等）。
  5. 若有 client：`keyHandler.clear()` 並進入 `InputState.Empty`（**丟棄**組字中的內容，不 commit）。
- 說明 `UG:113`。

### 8.1 fcitx 掛點

- 設定已存在：`config_.bopomofoKeyboardLayout`（`fMBh:105-109`，fcitx Smart Learning 預設 **Hsu**），持久化用 `fcitx::safeSaveAsIni(config_, kConfigPath)`（比照 `fMB:504`）。
- 新增兩個 `fcitx::SimpleAction`（`mcbopomofo-layout-standard`、`mcbopomofo-layout-hsu`），在建構子比照 `fMB:497-523` 註冊，`setCheckable(true)` / `setChecked(...)`；activate 時在 `fMB:732` 之前加入狀態列（讓它們在最上方）。
- Activated：設 config → `safeSaveAsIni` → 依配列呼叫 `keyHandler_->setKeyboardLayout(...)`（把 `fMB:778-800` 的 switch 抽成 helper，activate 與 action 共用）→ `keyHandler_->reset()` + `enterNewState(context, Empty)` → `showTip` 顯示標題 → 更新兩個 action 的 checked 狀態。
- 無需純 C++ 模組（只有「BopomofoKeyboardLayout → Formosa layout」對照，可做成 helper 並在 `McBopomofoConfigTest.cpp` 測）。

---

## 9. 偏好對照表（Mac → 建議 fcitx option）

- `SmartChineseEnglishSwitchEnabled` true → 新 `fcitx::Option<bool>`，同名。
- `AutoSpaceBetweenChineseAndEnglish` true → 新，同名。
- `HalfWidthPunctuationAfterEnglish` true → 新，同名。
- `FixEnglishWordCase` true → 新，同名。
- `EscTrimsMistypedText` true → 新，同名；與既有 `EscKeyClearsEntireComposingBuffer`（fcitx 預設 true；Mac 對應 `EscToCleanInputBuffer` 預設 false）並存，開啟時取代之。
- `ChineseEnglishSwitchReminderEnabled` true、`ShowChineseEnglishModeIcon` true、`ShowFullHalfWidthModeIcon` false → 新。
- `HalfWidthPunctuationEnable` false → fcitx 已有 HiddenOption 同名（`fMBh:223-225`）。
- `KeyboardLayout` standard → fcitx 已有 `BopomofoKeyboardLayout`（預設 Hsu）。
- `LetterBehavior` 0 → fcitx `ShiftLetterKeys::DirectlyOutputUppercase`。
- Mac 的 `Preferences` 診斷輸出列出這些值（`PF:741-763`），fcitx 無對應需求。

---

## 10. Mac 測試覆蓋現況（供 port 對照）

**Mac 在 7446fd0 沒有任何「正向」測試涵蓋這 8 項功能**：`McBopomofoTests/` 與 `Source/Engine/*Test.cpp` 皆無 `EnglishWords`、`_literal_`、`_english_`、autoSpace、fixEnglishWordCase、halfWidthPunctuationAfterEnglish 的測試；`McBopomofoLMTest.cpp`（16 個 TEST）也沒有 `_literal_` / `_english_` 案例。現有相關測試都是**把新功能關掉**來保留舊行為（commit `628863d` 說明）：

- `McBopomofoTests/KeyHandlerBopomofoTests.swift`（smart 關閉）：
  - `testUppercaseLetterWhenEmpty1`（438，關 smart 於 441-445）
  - `testUppercaseLetterWhenNotEmpty1`（482，485-489）
  - `testIgnorePunctuationTable`（681，684-689）
  - `testInvalidBpmf`（822，827-832）
- 同檔（`escTrimsMistypedText` 關閉）：
  - `testEscToClearReadingAndGoToEmpty`（2013）
  - `testEscToClearReadingAndGoToInputting`（2052）
  - `testEscToClearAll`（2095）
- 相關但未改偏好（行為與新功能互動，可當回歸測試）：`testUppercaseLetterWhenEmpty2`（459）、`testUppercaseLetterWhenNotEmpty2`（518）、`testHalfPunctuationComma`（714）、`testPunctuationComma`（736）、`testHalfPunctuationPeriod`（757）、`testPunctuationPeriod`（778）、`testCtrlPunctuationPeriod`（800）、`testEscKey`（2134）、`testEscKeyWithCandidate`（2159）、`testBackspace*`（1071、1106、1156、1195）、`testInputSpace`（1486）、`testInputSpaceInBetween`（1547）、`testSyncWithPreferences`（59）。
- `McBopomofoTests/PreferencesTests.swift`：`testHalfWidthPunctuationEnabled`（153）、`testEscToCleanInputBuffer`（162）；新偏好鍵沒有對應測試。
- fcitx 現有 `src/KeyHandlerTest.cpp` 共 34 個 TEST_F，相關者：`UppercaseLetterCommitComposingBufferByDefault`（369）、`UppercaseLetterNotHandledIfComposingBufferIsEmpty`（376）、`UppercaseLetterConvertedToLowercaseInComposingBuffer`（382）——smart 預設開啟後這三個需要像 Mac 一樣在測試中關閉 smart。

建議 port 時自行補的測試（依 `UG` 範例）：`app`→英文、`you`+空白、`ChatGPT`、`ok?`、標準鍵盤 `2026`+空白+年、`hello`+ㄏㄠˇ（Hsu `hwf`）、`PDF`+ㄉㄤˇ、`helpf` 不轉、`hellone`、`你好hello`/`用iPhone拍` 自動空白、Backspace 刪光英文連空白、`價格是300.`、`好,`、Ctrl+`,`、`iphone`+空白→iPhone、Enter 保留小寫、學兩次 `ButyPOS` 後修正、`今天開會 jqvx`、`寄給 email 了 ;/z`、`hellojkq`、`下午 3 點 ㄒㄧ`、無亂碼時第一下只提示、兩下全清、游標在中間時只清游標前。

---

## 11. UNKNOWN 清單

1. NSSpellChecker（`language: "en"`）實際字典詞數、是否包含使用者「學習拼字」、對小寫專有名詞的判定（§5.5）。
2. Debian 上 `hunspell-en-us` 展開後詞數、`wamerican` 詞數；hunspell `spell()` 與 NSSpellChecker 判定差異（§5.5）。
3. `NotifierController` 提示的顯示時長與外觀（未讀 `NotifierController` 原始碼）（§7.1）。
4. fcitx5 目標版本是否支援 `InputMethodEngineV2::subModeLabelImpl`／子模式標籤顯示（§7.3）。
5. fcitx5 中 Caps Lock 鍵本身的事件是否送達 engine，以及當下 `rawKey().states()` 反映切換前或切換後（§7.3）。
6. fcitx5 內建切換輸入法資訊提示是否足以取代 Mac「ABC/中文」提醒（§7.3）。
