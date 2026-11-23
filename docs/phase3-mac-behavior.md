# Phase 3 偵察：macOS 自動學習、聯想、數字快打、常用句、範本與符號表行為規格

來源：Mac fork `/Users/roger/Developer/McBopomofo` @ `7446fd0`。目標：fcitx5 port `/Users/roger/Developer/fcitx5-mcbopomofo`（branch `smart-learning` @ `2fb50e3`）。

Mac 端縮寫（行號皆為 `7446fd0`）：

- `KH` = `Source/KeyHandler.mm`
- `LMM` = `Source/LanguageModelManager.mm`
- `LM` = `Source/Engine/McBopomofoLM.cpp`，`LMh` = `Source/Engine/McBopomofoLM.h`
- `UOM` = `Source/Engine/UserOverrideModel.cpp`，`UOMh` = `.h`
- `QN` = `Source/QuickNumbers.swift`
- `CS` = `Source/CommonSentences.swift`（含 `RemovedEntries`）
- `EW` = `Source/EnglishWords.swift`
- `IS` = `Source/InputState.swift`
- `IMC` = `Source/InputMethodController.swift`
- `IMCc` = `Source/InputMethodController+CandidateControllerDelegate.swift`
- `IMCk` = `Source/InputMethodController+KeyHandlerDelegate.swift`
- `GC` = `Packages/CandidateUI/Sources/CandidateUI/GridCandidateController.swift`
- `AD` = `Source/AppDelegate.swift`
- `PF` = `Source/Preferences.swift`
- `UG` = `Source/UserGuide.html`
- `CL` = `CHANGELOG.md`
- `L10N` = `Source/zh-Hant.lproj/Localizable.strings`

fcitx 端縮寫（行號為 `2fb50e3` 的 `src/` 檔案）：

- `fKH` = `src/KeyHandler.cpp`，`fKHh` = `src/KeyHandler.h`
- `fMB` = `src/McBopomofo.cpp`，`fMBh` = `src/McBopomofo.h`
- `fIS` = `src/InputState.h`
- `fLML` = `src/LanguageModelLoader.cpp`
- `fNIH` = `src/NumberInputHelper.cpp`
- `fSL/` = `src/SmartLearning/`

`src/Engine/McBopomofoLM.h` 與 Mac 版完全相同，`.cpp` 只多一行 `#include <iterator>`；`UserOverrideModel.{h,cpp}` 兩邊完全相同（已用 `diff` 確認）。所以 `setTemporaryPhrases`、`setSymbolTable`、`getSymbolSections`、`getSymbolInitialIndex`、`setEnglishCompletionProvider`、`_symbols_`、`_english_`、`candidateObservationCount`、`observationCount` 在 fcitx 都已經存在，只是**沒有任何呼叫端**（`grep` 在 `src/` 中除 Engine 外找不到 `setSymbolTable`、`setTemporaryPhrases`、`setEnglishCompletionProvider`）。

無法從原始碼確定的事項標 **UNKNOWN**，並收在 §12。

---

## 0. 共通基礎

### 0.1 Commit 時的學習進入點 `_learnFromCommittedComposition`

Mac 在 commit 組字區之前呼叫 `_learnFromCommittedComposition`（`KH:402-449`），它依序做三件事：暫時詞轉正（§1.3）、記錄英文字（phase 2 已做）、記錄常用句（§5）。四個呼叫點：

1. force commit：`KH:970`（`handleForceCommitWithStateCallback`）
2. 空白鍵在句尾 commit（空白不是選字鍵或按 Shift）：`KH:1406`
3. 反引號開啟文字範本前 commit：`KH:1565`
4. Enter：`KH:2188`（`_handleEnterWithState`）

其他 commit 路徑都不學習，例如 Caps Lock（`KH:1049-1071`）、Ctrl+\ 功能選單（`KH:1541-1552`）。

fcitx 目前在對應位置只呼叫 `learnEnglishWordsFromWalk()`：

- Shift+Space／空白 commit：`fKH:386`
- Enter 的英文 commit：`fKH:471`
- Enter 一般 commit：`fKH:565`
- 大寫字母 commit：`fKH:673`
- `handleForceCommitAndReset`：`fKH:1121`

**建議**：新增 `KeyHandler::learnFromCommittedComposition()`，把 `learnEnglishWordsFromWalk()` 包進去，並在這五處改呼叫它。大寫字母那一處（`fKH:673`）在 Mac 不學習，但它只在 smart 關閉時才會走到；是否保留 UNKNOWN（建議比照 Mac 不學暫時詞與常用句，只學英文）。反引號那一處在 §6 新增。

`_learnFromCommittedComposition` 第一行是 `if (_inputMode != InputModeBopomofo) return;`（`KH:404-406`），Plain Bopomofo 模式什麼都不學。

### 0.2 時間

Mac 的暫時詞、常用句、removed.txt 都用牆鐘時間 `[NSDate date].timeIntervalSince1970`／`Date().timeIntervalSince1970`。fcitx 的 `EnglishWordStore` 注入的是 `steady_clock`（`fMB:480-485`），它只拿來算重新載入的間隔。新模組**必須另外注入牆鐘**（`system_clock`，Unix 秒），否則 30 天、90 天的期限與跨裝置比較都會錯。

### 0.3 removed.txt（RemovedEntries）的讀取

格式見 `docs/file-formats.md`。行為（`CS:29-86`）：

1. `removedTime(kind, key)`：沒有記錄回傳 0（`CS:43-46`）。快取 60 秒，距上次載入 >= 60 秒才重讀（`CS:61-65`）。
2. 讀檔：以 `\n` 切行、`\t` 切欄（保留空欄），恰好 3 欄且第 3 欄能轉成 `TimeInterval` 才收（`CS:67-77`）。字典鍵是 `kind\tkey`。
3. 寫入只有 `add`（`CS:48-52`）與 `remove`（`CS:54-59`），都是先完整重讀再整份覆寫，輸出依行排序，時間以 `Int(...)` 截成整數（`CS:80-85`）。
4. 呼叫 `add` 的只有詞彙管理員：`EW:213`（`EnglishWords.remove`）、`CS:255`（`CommonSentences.remove`）、`LMM:939-947`（`forgetTemporaryPhrase`，由 `Source/VocabularyManager.swift:322,339` 呼叫）。呼叫 `remove` 的也只有詞彙管理員的「加入」：`EW:202`、`CS:240`。

fcitx 目前只有 `EnglishWords.cpp:186-200` 的私有函式 `removedEnglishWords()`，只讀 `english` kind。phase 3 需要的是共用的讀取模組（見 §8）。

### 0.4 Reading 前綴（phase 2 已整理，這裡只列 phase 3 會用到的）

- `_literal_<字元>`：原樣字元，分數 0（`LM:178-184`）。
- `_english_<字母>`：第一個候選是字本身（分數 0），其後是 provider 回傳且不等於本字的補完（分數 -1）（`LM:160-176`）；`hasUnigrams` 只檢查是否全是 ASCII 字母（`LM:404-406`）。
- `_symbols_<標點 key>`：見 §6.3。
- `_numpad_<字元>`：資料檔內每個數字都有半形與全形兩個值，例如 `_numpad_0 0`、`_numpad_0 ０`，分數都是 0.0（`data/data.txt`）。

---

## 1. 兩段式自動學習（暫時詞 → data.txt，含拆詞學習）

- Mac 偏好：**沒有開關**。只要是 `InputModeBopomofo` 就啟用。
- 使用說明：`UG:278-289`；變更記錄：`CL` learning.15。

### 1.1 選字時（`fixNodeWithReading`，`KH:250-285`）

1. 先複製 `prevWalk = _latestWalk.copyWithFixedNodes()`，再 `overrideCandidate`、`_walk`（`KH:257-266`）。
2. 找到選字後游標位置的節點 `currentNode`（`KH:269-274`）。
3. 若 `currentNode->currentUnigram().score() > -8`，依序呼叫（`KH:275-279`）：
   1. `observeUserOverrideWithWalkBefore:walkAfter:cursor:`（寫入本機與合併的選字學習模型，`LMM:961-970`）
   2. `_learnPhraseIfFrequentlyChosen:currentNode`
   3. `_learnBrokenUpPhraseIfFrequentlyChosen:currentNode walkBefore:prevWalk cursor:actualCursor`
4. 注意第 3.1 步先執行，所以後面兩步查到的次數**已包含這一次**。
5. 查次數用的 `_userOverrideModel` 是**合併模型** `gUserOverrideModel`（`KH:192`、`LMM:721-728`），所以次數包含其他裝置的選擇。

fcitx 對應：`KeyHandler::pinNode`（`fKH:1991-2035`），第 3 步的 `if` 在 `fKH:2020-2027`，目前只呼叫 `userOverrideStore_->observe` 與 `onUserOverrideObserved_`。兩個學習函式要接在 `observe` 之後、同一個 `if` 內。`pinNode` 也被 Tab 循環選字（`fKH:1203`）與重複標點（經由 `handleTabKey`）呼叫，Mac 的 `_handleTabState` 同樣經過 `fixNodeWithReading`（`KH:1740`），所以行為一致。

### 1.2 `_learnPhraseIfFrequentlyChosen`（`KH:455-489`）

常數 `kLearnPhraseThreshold = 2`（`KH:453`）。依序：

1. `_inputMode != InputModeBopomofo` 或 `node->spanningLength() < 2` → 結束（`KH:457-459`）。單字永遠不進暫時詞。
2. `rawValue` 非空且不等於 `value`（已展開的輸入巨集）→ 結束（`KH:464-466`）。
3. `value` 為空或含空白／換行 → 結束（`KH:469-471`）。
4. reading 以 `_` 開頭，或 reading 不含 `-` → 結束（`KH:472-474`）。
5. `phrase = "value reading"`（單一空格）。
6. 若 `candidateObservationCount(value) < 2` → `_addTemporaryPhrase:phrase`（§1.4），結束（`KH:477-480`）。
7. 否則 `removeTemporaryPhrase:phrase`，再 `learnUserPhrase:phrase`（§1.5）。成功才通知 `didLearnPhrase`（`KH:482-488`）。

`candidateObservationCount(candidate)`（`UOM:227-238`）：把合併模型中所有 key 底下等於 `candidate` 的 `count` 相加。它**只比對值，不比對讀音**，也**不考慮半衰期衰減**。

邊界情形：

- `UserOverrideModel::observe` 對 spanningLength > 3 的節點直接 return（`UOM:91-93`），所以 4 字以上的詞次數永遠不增加，每次選都只會重新加入暫時詞（更新時間），只能靠 §1.3「再打出來一次」寫入 data.txt。
- 同一個值在別的讀音或別的前後文被選過，也算進次數。

### 1.3 Commit 時暫時詞轉正（`KH:402-449` 的前半）

對 `_latestWalk.nodes` 逐一處理（`KH:409-438`）：

1. 英文字母與 `_english_` 節點拿去組英文字（phase 2 已有，`KH:413-424`）。
2. `spanningLength < 2`、reading 為空、reading 以 `_` 開頭 → 略過（`KH:425-427`）。
3. `phrase = "value reading"`；若 `phrase` 在本次組字已選過的集合 `_phrasesPickedInComposition` 裡，或 `![LanguageModelManager isTemporaryPhrase:phrase]` → 略過（`KH:428-431`）。
4. 否則 `removeTemporaryPhrase`，`learnUserPhrase` 成功就通知 `didLearnPhrase`（`KH:432-435`）。

`_phrasesPickedInComposition`：

- `_addTemporaryPhrase` 每次都把 phrase 加進集合（`KH:390-397`）。
- `clear` 清空集合（`KH:953`）。commit 路徑都是先學習再 `clear`。
- 用途：本次組字才剛選、剛成為暫時詞的詞，不算「再打一次」。

`isTemporaryPhrase` 查的是合併表 `gTemporaryPhrases`（`LMM:916-920`），所以在別台裝置選過的暫時詞，在這台打出來也會轉正。

這一步不檢查巨集與空白；暫時詞只能由 §1.2、§1.6 加入，那兩處已檢查過。

### 1.4 暫時詞存放（`LMM:817-959`）

資料結構：兩個 `"value reading" → timestamp` 字典，本機 `gLocalTemporaryPhrases` 與合併 `gTemporaryPhrases`（`LMM:817-818`）。

路徑：`learning/temporary/<本機學習檔名>`（`LMM:830-834`）。格式見 `docs/file-formats.md`：一行 `值 讀音 時間`，單一空格分三欄。

1. **讀單一檔**（`LMM:837-854`）：以 `\n` 切行、`" "` 切欄，恰好 3 欄才收；`now - timestamp > 30*86400` 的丟棄（`kTemporaryPhraseLifetime`，`LMM:95`）。
2. **重新載入** `_reloadTemporaryPhrases`（`LMM:856-886`）：
   1. 本機檔只在 `gLocalTemporaryPhrases == nil` 時讀一次（`LMM:859-861`）。
   2. 本機字典中 `removedTime(temporary, phrase) >= timestamp` 的刪掉（`LMM:863-867`）。
   3. 合併表 = 本機字典的複本，再讀 `temporary/` 下其他 `.txt`（不含本機檔名），跳過 `removedTime >= timestamp` 者，同鍵取較新時間（`LMM:868-884`）。
   4. `_applyTemporaryPhrases`（`LMM:888-898`）：轉成 `reading → [value...]` 交給 `setTemporaryPhrases`。同讀音多個值的順序是字典迭代順序（**UNKNOWN**，Swift／Foundation 不保證）。
3. 重新載入的時機：`reloadUserOverrideModel` 最後一步（`LMM:811`）。它在第一次取用選字模型時（`LMM:721-728`）、`updateUserPhrases`（`AD:182`）、`learning/` 有檔案變動時（`AD:333-343`）被呼叫。
4. **加入** `addTemporaryPhrase`（`LMM:922-930`）：本機與合併表都設為 now，`_applyTemporaryPhrases`，`_saveTemporaryPhrases`。
5. **移除** `removeTemporaryPhrase`（`LMM:949-959`）：不在合併表就什麼都不做；否則從兩表移除、apply、save。**不寫 removed.txt**（只有詞彙管理員的 `forgetTemporaryPhrase` 才寫，`LMM:939-947`）。
6. **存檔** `_saveTemporaryPhrases`（`LMM:900-914`）：把本機字典逐行寫成 `phrase %.0f\n`，在 I/O 佇列建立資料夾並 atomic 寫入。每次加入或移除都立刻寫，沒有 2 秒延遲。

邊界情形：

- 本機字典只讀一次，之後不會因過期而刪除；存檔也不過濾過期項目。過期的本機項目要等下次程式啟動讀檔時才會被丟掉。
- Mac 沒有「本機檔尚未載入就不得覆寫」的保護：本機檔還沒同步下來時，第一次 `addTemporaryPhrase` 會把只有一筆的內容覆寫上去。**Linux 建議比照 phase 1 的 pending 規則**（`UserOverrideStore`、`EnglishWordStore` 皆是），這是刻意的差異。
- 值或讀音含空格的行會被丟掉（3 欄檢查）。

LM 端（`LM:221-232`）：暫時詞排在使用者詞之後、系統詞之前，分數 0，同樣經過排除詞過濾。`hasUnigrams` 對暫時詞的 key 直接回傳 true（`LM:408-410`）。

### 1.5 `learnUserPhrase`（`LMM:448-509`）

這是寫入 data.txt 的排序規則，`writeUserPhrase`（`+` 提高排序用）是另一個函式（§2）。

1. `checkIfUserLanguageModelFilesExist` 失敗 → NO（`LMM:450-452`）。
2. 該「詞 讀音」已在 `exclude-phrases.txt` → NO，絕不自動加回排除詞（`LMM:456-458`）。比對規則：每行去頭尾空白後以單一空格切成恰好兩欄，兩欄都相等（`LMM:303-335`）。
3. `userPhrase` 以 `" "` 切開不是恰好兩欄 → NO（`LMM:460-463`）。
4. 讀 data.txt，失敗（含檔案不存在）→ NO（`LMM:466-471`）。
5. 以 `\n` 切行。對每行：去頭尾空白，以 `#` 開頭、或以單一空格切開不是恰好兩欄、或第二欄不等於讀音 → 略過。記下同讀音的第一行 `firstIndexOfReading`，以及第一欄等於詞的所有行（`LMM:476-489`）。
6. 沒有同讀音的行 → 附加到檔尾（`_writePhrase:atEndOfPath:`，檔尾不是 `\n` 先補換行，`LMM:337-374`），回傳其結果（`LMM:491-493`）。
7. 同讀音第一行就是這個詞 → NO（已在最前，`LMM:494-497`）。
8. 否則刪掉所有該詞的行，把 `userPhrase` 插在 `firstIndexOfReading`，以 `\n` 接回後 atomic 覆寫（`LMM:499-508`）。因為切行時保留了最後的空字串，原本結尾的換行會保留。

邊界情形：用 tab 分隔的行不會被辨識（兩欄檢查只看空格），可能造成重複。

學會後的通知（`IMCk:112-116`）：`NotifierController` 顯示 `已學會詞彙「%@」`（`L10N:120`）。Mac 不主動重新載入詞庫，依賴資料夾監看（`AD:333-354`）。

### 1.6 拆詞學習 `_learnBrokenUpPhraseIfFrequentlyChosen`（`KH:496-554`）

情境：內建詞「許是」被使用者把「是」改成「氏」。下次 walk 又會得到「許是」，選字模型永遠無法建議回「氏」，所以改學「許氏」這個新詞。

1. `_inputMode != InputModeBopomofo` 或 `node->spanningLength() != 1` → 結束（`KH:498-500`）。
2. 在 `walkBefore` 找游標處的節點；找不到 → 結束。它的長度 `phraseLength` 必須在 2 到 4 之間（`KH:502-510`）。`phraseStart = phraseEnd - phraseLength`。
3. 走訪 `_latestWalk`（選字之後的 walk），收集覆蓋 `[phraseStart, phraseEnd)` 的節點（`KH:513-536`）：
   - 完全在範圍之前或之後的略過或停止。
   - 有任何節點跨出範圍 → 結束。
   - 有節點 reading 為空、以 `_` 開頭，或是已展開的巨集 → 結束。
   - 串接各節點的值，收集各節點的 reading。
4. 值為空或含空白 → 結束（`KH:537-539`）。
5. `reading = readings 以 "-" 接起來`，`phrase = "value reading"`（`KH:541-542`）。
6. `observationCount(_latestWalk, cursor, node 的值) < 2` → `_addTemporaryPhrase:phrase`，結束（`KH:543-546`）。
7. 否則 `removeTemporaryPhrase`、`learnUserPhrase`，成功才通知（`KH:548-553`）。

`observationCount(walk, cursor, candidate)`（`UOM:240-254`）：以 walk 在 cursor 處的節點組出觀察鍵，回傳該鍵下 candidate 的次數。拆詞情形下 `observe` 也是用「選字後的 walk」組鍵（`UOM:140-147` 的 `breakingUp`），兩者一致。Mac 測試 `UserOverrideModelTest.ObservationCountOfBrokenUpPhrase`（`Source/Engine/UserOverrideModelTest.cpp:245-278`）驗證選兩次後次數為 2；fcitx 有同一份測試。

結果：

- 第一次拆詞：「許氏 ㄒㄩˇ-ㄕˋ」成為暫時詞（多音節 key，排在系統詞之上，所以下次打 ㄒㄩˇ-ㄕˋ 就是「許氏」）。
- 第二次在同樣前後文選「氏」，或第一次之後再把「許氏」打出來送出，就寫進 data.txt。

### 1.7 fcitx 掛點

- `pinNode`（`fKH:2020-2027`）：在 `observe` 之後呼叫兩個學習函式；需要 `prevWalk`（已有）與 `actualCursor`（已有）。
- `reset()`（`fKH:886-892`）：要清空 `phrasesPickedInComposition_`（對應 Mac `clear`）。
- commit 點：§0.1。
- LM：engine 建立 `TemporaryPhraseStore` 後，每次變動呼叫 `McBopomofoLM::setTemporaryPhrases`。LM 由 `LanguageModelLoader::getLM()` 取得（`fLML` 的 `lm_`）。
- 通知：engine 用 `notifications()->call<fcitx::INotifications::showTip>`，與 `fMB:1060-1066` 的提醒相同寫法。KeyHandler 新增 `setOnPhraseLearned(std::function<void(const std::string& value)>)`。
- 學會後要讓 LM 重讀 data.txt：`LanguageModelLoader::reloadUserModelsIfNeeded()`（`fLML:216`）是以時間戳判斷，同一秒內連續寫入可能偵測不到（**UNKNOWN**：`TimestampedPath` 的時間精度未確認）。建議學習寫入後強制重新載入。
- 重新載入暫時詞：`fMB:980` 每次啟用時會 `userOverrideStore_->reloadIfChanged()`，暫時詞可以搭同一個時機。

### 1.8 模組邊界（純 C++，放 `fSL/`）

`LearnPhrasePlan`（純函式，輸入 walk 與 UOM，輸出要做的事）：

```cpp
struct LearnAction { enum Kind { kNone, kAddTemporary, kLearnUserPhrase } kind; std::string value, reading; };
LearnAction PlanLearnPickedPhrase(const ReadingGrid::NodePtr& node, const UserOverrideModel& uom);          // §1.2
LearnAction PlanLearnBrokenUpPhrase(const ReadingGrid::NodePtr& node, const ReadingGrid::WalkResult& before,
                                    const ReadingGrid::WalkResult& after, size_t cursor, const UserOverrideModel& uom); // §1.6
std::vector<std::pair<std::string,std::string>> PhrasesToLearnOnCommit(const ReadingGrid::WalkResult& walk,
    const std::set<std::string>& pickedInComposition, std::function<bool(const std::string&)> isTemporary); // §1.3
```

`TemporaryPhraseStore`：

```cpp
class TemporaryPhraseStore {
 public:
  static constexpr double kLifetime = 30 * 86400;
  TemporaryPhraseStore(std::string deviceID, std::string deviceName, std::function<double()> wallNow);
  void setLearningFolder(std::filesystem::path learningFolder);    // 用 temporary/ 與 removed.txt
  void reload();
  bool contains(const std::string& value, const std::string& reading) const;
  void add(const std::string& value, const std::string& reading);  // 立即存檔（pending 時延後）
  void remove(const std::string& value, const std::string& reading);
  std::unordered_map<std::string, std::vector<std::string>> phrasesByReading() const; // 給 setTemporaryPhrases
  bool save();
  bool isLocalFilePending() const;
};
```

`UserPhraseFile`（從 `LanguageModelLoader` 抽出的純檔案操作，可測）：`learn(path, excludePath, value, reading)` 實作 §1.5；`write`、`remove` 實作 §2.3。`LanguageModelLoader` 目前的 `checkIfPhraseExists`（`fLML:301-329`）比對的是「整行去尾空白等於 `value reading`」，與 Mac「去頭尾空白後單一空格切成兩欄」略有不同（Mac 也接受行首空白），移植時以 Mac 為準。

可重用：`LearningPaths`（`learningFilename`、`otherDevicesLearningFiles` 已排除隱藏檔與衝突副本）、`AtomicFile::writeFileAtomically`、`UserOverrideStore::mergedModel()`。

### 1.9 測試

Mac 沒有暫時詞、`learnUserPhrase` 或 KeyHandler 學習流程的測試；只有 UOM 的 `candidateObservationCount`（`UserOverrideModelTest.cpp:177-179`、`:222-223`）與 `ObservationCountOfBrokenUpPhrase`（`:245`）。建議補：

- 選兩字詞一次 → 暫時詞檔出現一行，data.txt 不變。
- 同詞再選一次 → data.txt 寫入，暫時詞移除。
- 選一次、commit、再打出來 commit → data.txt 寫入。
- 同一次組字選了又 commit → 不轉正。
- 排除詞不會被學回。
- `learnUserPhrase` 的四種結果（新增到尾、插到同讀音第一行之前、已是第一行、檔案不存在）。
- 「許是」→「氏」拆詞兩次。
- 30 天過期、removed.txt 的時間比較、其他裝置合併取較新。

---

## 2. 候選字 `+`／`-`（提高排序、排除），含單字

- Mac 偏好：無。只在 `InputModeBopomofo`。
- 使用說明：`UG:247-257`；變更記錄：learning.21「也能用在單字」。

### 2.1 Mac 規則（`KH:2500-2571`）

條件：`_inputMode == InputModeBopomofo` 且 state 是 `InputStateChoosingCandidate`（包含子類別 `ChoosingPunctuationList`）。

1. `+` 或 `=` 是提高排序，`-` 或 `_` 是排除（`KH:2501-2502`）。
2. 取候選窗目前反白的候選（`selectedCandidateIndex`，全域索引）（`KH:2504-2507`）。
3. `value != rawValue`（已展開的巨集）→ 直接 `return YES`，**不響**（`KH:2514-2516`）。
4. reading 以 `_` 開頭、值為空、值含空白或換行 → `errorCallback`（響），return（`KH:2521-2525`）。所以符號、標點、英文（`_literal_`、`_english_`）都不行，**單字可以**。
5. 進入 `InputStateCustomMenu`：標題「您想要提升 "%@" 這個詞彙的權重嗎？」或「您想要從詞庫中排除 "%@" 這個詞彙嗎？」，選項「增加權重」或「排除」，以及「取消」（`KH:2527-2569`、`L10N:127-135`）。
6. 確認提高排序：`didRequestBoostScoreForPhrase` → `writeUserPhrase("value reading")`（`IMCk:101-110`），再 `keyHandlerDidRequestReloadLanguageModel`（同步重讀使用者詞，`IMCk:129-132`），`_walk`，回到 Inputting（`KH:2534-2540`）。
7. 確認排除：`didRequestExcludePhrase` → `removeUserPhrase("value reading")`（`IMCk:118-127`），其後同上（`KH:2549-2555`）。
8. 取消：回到原本的候選 state，並把反白還原到原本的索引（`KH:2560-2563`）。在選單按 Esc／Backspace 也回到原候選 state 並還原索引（`KH:2618-2624`）。
9. 兩者成功時都執行加詞 hook（`runUserPhraseHookIfEnabled`，`IMCk:106-108`、`123-125`）。

### 2.2 檔案寫入

- `writeUserPhrase`（`LMM:427-446`）：先從 `exclude-phrases.txt` 移除同一筆（`_removePhrase`），若 data.txt 已有同一筆 → NO，否則附加到 data.txt 檔尾。**附加在檔尾，不是插到最前**。單字的使用者詞在 LM 中會被改寫成「系統最高分 + 1e-9」（`LM:256-279`），所以附加在尾端仍會排第一；但若 data.txt 同讀音已有別的使用者詞，那個詞仍在前面（`EarlierUserPhraseComesFirst`，`McBopomofoLMTest.cpp:117`）。
- `removeUserPhrase`（`LMM:511-530`）：先從 data.txt 移除同一筆，若排除檔已有 → NO，否則附加到排除檔尾。
- `_removePhrase`（`LMM:376-425`）：整份讀入；每行去頭尾空白後以單一空格切；不是兩欄的行原樣保留；符合者刪除；有刪除才 atomic 覆寫。
- 不寫 removed.txt。

### 2.3 fcitx 現況與差異（`fMB:1383-1452`）

已有 `+`/`=`/`-`/`_` 與確認選單，呼叫 `keyHandler_->boostPhrase`／`excludePhrase`（`fKH:874-884`）→ `LanguageModelLoader::addUserPhrase`／`removeUserPhrase`（`fLML:165-214`），寫檔規則與 Mac 相同（移除排除、檢查存在、附加尾端）。要改的地方：

1. `fMB:1404-1407`：reading 不含 `-` 就 return。**要拿掉**，單字才能用。
2. `fMB:1397-1403`：只擋 `_half_punctuation_` 等五個前綴，而且是安靜 return。Mac 是任何 `_` 開頭、空值、含空白都響（`errorCallback`）。
3. 檢查順序：Mac 先檢查巨集（安靜），再檢查前綴（響）；fcitx 先檢查前綴。
4. `fMB:1386-1388`：排除了 `ChoosingPunctuationList`，Mac 沒排除（但符號 reading 以 `_` 開頭，結果只是響一聲）。
5. `fMB:1389-1393`：用 `currentPage * candidateList->size() + cursorIndex` 算全域索引。`size()` 是**本頁**候選數，最後一頁不滿時算錯。應改用 `candidateList->globalCursorIndex()`（`fMB:1466` 已有人這樣用）。`?` 查辭典（`fMB:1351`）有同樣問題。
6. 確認後：fcitx 只 `buildInputtingState()`，沒有重新 walk。Mac 會重讀詞庫再 `_walk`。`addUserPhrase` 內部會 `reloadUserModelsIfNeeded()`，但 KeyHandler 的 `latestWalk_` 沒更新，組字區不會反映新排序。需要在 `boostPhrase`／`excludePhrase` 之後 `walk()`。
7. 取消：fcitx 重建候選 state（`fMB:1430-1438`），索引沒還原；Mac 會還原。
8. `fKH:874-884` 兩者都呼叫 `onAddNewPhrase_`（加詞 hook），與 Mac 一致。

### 2.4 模組邊界

`CandidateAdjustment`（純函式）：

```cpp
enum class AdjustCheck { kAllowed, kIgnoreSilently, kRejectWithError };
AdjustCheck CheckCandidateAdjustable(const std::string& reading, const std::string& value, const std::string& rawValue);
```

檔案操作沿用 `UserPhraseFile`（§1.8）。

### 2.5 測試

Mac 無 `+`/`-` 測試。建議：單字「你 ㄋㄧˇ」可排除、`_literal_a` 響、巨集候選安靜、第二頁最後一項索引正確、排除後候選與組字區立即更新。

---

## 3. 英文聯想（Tab）

- Mac 偏好：沒有獨立開關。入口綁在智慧中英切換（`SmartChineseEnglishSwitchEnabled`，預設 true，`PF:40`、`PF:351-352`），因為兩個入口都在 smart 區塊內。
- 使用說明：`UG:200-212`；變更記錄：learning.15。

### 3.1 入口

1. **英文模式中按 Tab**（非 Shift）：`_enterEnglishCompletionWithInput`；失敗就 `errorCallback`，一律 return YES（`KH:1177-1184`）。
2. **注音未打完時按 Tab**（中文模式、smart 可用、非 Shift、reading 非空）：取 `_rawKeysOfCurrentReading`，若全是字母，先 `_switchToSmartEnglishWithText:keys`（變成英文、進英文模式），再試 `_enterEnglishCompletion`；失敗**不響**，只回到 Inputting（`KH:1187-1199`）。例：許氏鍵盤 `hel` 是 ㄏㄧㄥ，按 Tab 變成 hel 並列出 hello。
3. 優先順序：常用句的 Tab（§5.5，`KH:1133-1146`）在 smart 區塊之前，有常用句提示時 Tab 先接常用句。

### 3.2 `_enterEnglishCompletionWithInput`（`KH:808-845`）

1. 從游標往前找連續的 `_literal_<ASCII 字母>` reading（長度剛好 `len("_literal_")+1` 且最後一個 byte 是字母）（`KH:810-820`）。
2. 少於 2 個字母 → NO（`KH:821-823`）。
3. `key = "_english_" + word`；`getUnigrams(key).size() < 2`（沒有任何不同於本字的補完）→ NO（`KH:828-831`）。
4. 刪掉那些字母 reading，插入 `key`，`_walk`（`KH:832-836`）。
5. 記下 `originalCursorIndex`；若 `selectPhraseAfterCursorAsCandidate`，游標減 1；建立 `ChoosingCandidate`（**不是** `ChoosingPunctuationList`）（`KH:837-843`）。
6. 候選第一個是原字（`LM:167-168`），選它等於不改。
7. 選字經 `fixNodeWithReading`，會觀察 UOM；兩個學習函式因 reading 以 `_` 開頭或長度 1 而不作用。
8. commit 時 `_english_` 節點的值（選定的補完）計入英文字學習（`KH:418-421`）。

auto-space 的 `_literal_ ` 不是字母，所以不會被刪；英文字前已有的自動空白留著。

### 3.3 補完來源 `EnglishWords.completions(for:)`（`EW:52-101`）——精確演算法

Provider 在 `KH:205-214` 注入（只注入一次，給 `McBopomofo` 模式的 LM）。

1. `word.count < 2` → `[]`（`EW:53-55`）。
2. `loadIfNeeded()`；`lowercased = word.lowercased()`。
3. `byCount` = 所有學過的字（合併後的 `counts`，已濾掉 removed）依「次數遞減，同次數依字串遞增」排序（`EW:58`）。
4. `typed` = 兩段串接（`EW:62-67`）：
   1. `byCount` 中 `lower(k)` 以 `lowercased` 開頭者。
   2. `byCount` 中「首字母小寫等於 `lowercased` 首字母」且 `lowercased` 是 `lower(k)` 的子序列者（`isSubsequence`，`EW:217-232`：依序在 k 中找每個字母，找到後從下一個位置繼續）。前綴相符者也符合子序列，所以會重複出現，之後去重。
5. `frequent` = `typed` 中 `counts[k] >= 2`；`once` = `counts[k] < 2`（`EW:69-70`；`counts` 以原大小寫為鍵）。
6. `results = frequent + 拼字檢查器 completions(原字) + once + 拼字檢查器 guesses(原字)`（`EW:72-79`）。
7. `capitalized = 原字首字母是大寫`（`EW:81`）。
8. 去重與篩選（`EW:82-99`）：`seen` 初始為 `{word}`（原大小寫）。逐一處理：
   - 不是全部 ASCII 字母者略過（只收單字）。
   - `capitalized` 且候選全小寫 → 首字母改大寫。
   - 不在 `seen` 才加入。
   - 收滿 20 個（`maximumCompletions`，`EW:30`）就停。

所以「打過兩次以上的字最前面」來自第 5、6 步；「`aocd` → associated」只來自第 4.2 步，**只對學過的字有效**（第 6 步的 guesses 是否也能找到屬 **UNKNOWN**）。打過一次的字排在字典補完之後（避免打錯的字排前面）。

### 3.4 Linux 補完來源提案

macOS 拼字檢查器沒有 Linux 對應。fcitx 已有 `EnglishLexicon`（排序、去重、小寫的字表，`fSL/EnglishWords.h:18-43`）與 `EnglishWordStore`（`counts_`、`localCounts_`）。提案：

1. 在 `EnglishWordStore` 新增 `std::vector<std::string> completions(const std::string& word)`，第 1-5、7、8 步完全照 Mac。
2. 第 6 步的「completions」換成 `EnglishLexicon::wordsWithPrefix(lowercased, limit)`：在 `words_` 上 `lower_bound` 取前綴範圍，排除等於 `lowercased` 本身者。字表沒有詞頻，Mac 的順序（`UG:203` 範例 `ass` → assuming、assume、assistant、associate）看起來不是字母序，所以 Linux 順序必然不同；建議「長度遞增，同長度依字母序」，上限 40 個再交給第 8 步截成 20。這是 Linux 差異，要寫進說明。（實機驗收後補充：這 40 個裡沒有母音字母 a e i o u y 的字（如 blvd），以及超過三個字母、除首字母外沒有母音的字（如 assn、asst）多半是縮寫，排到字表補完的最後；and、ask 這類短字不算；第 8 步也不分大小寫排除與輸入相同的字。）
3. 第 6 步的「guesses」：Linux 不做，或以字表做「首字母相同且子序列」的搜尋（長度遞增）。建議先不做，避免大字表全掃的成本。
4. 第 4 步排序的字串比較：Swift 的 `String <` 對 ASCII 等同 byte 序，`std::string` 可直接比。
5. engine 在建立 LM 後呼叫 `lm->setEnglishCompletionProvider([store](const std::string& w){ return store->completions(w); })`。注意 provider 會在 `getUnigrams("_english_...")` 時被呼叫，包含 grid walk 期間；`completions` 內的 `loadIfNeeded` 可能讀檔，應保持輕量（60 秒快取已具備）。

### 3.5 fcitx 掛點

- 英文模式 Tab：`fKH:187-190` 目前是 `errorCallback`，換成 `enterEnglishCompletion`，失敗才響。
- 注音未打完 Tab：在 `fKH:191`（`else if (smartSwitching)`）分支最前面、大寫字母判斷之前加入 §3.1 第 2 條。
- 候選窗：`enterEnglishCompletion` 要回傳 `ChoosingCandidate`（用 `buildChoosingCandidateState(inputting, originalCursor)`，`fKH:1773-1785`）。
- Mac 判斷候選窗方向：任何候選顯示文字超過 8 個字就用直式（`IMC:971-978`）。fcitx 由設定決定（`fMB:2171-2210`），不需比照。

### 3.6 模組邊界

```cpp
// fSL/MixedInput.h
struct EnglishCompletionPlan { size_t lettersToDelete; std::string englishReading; };
std::optional<EnglishCompletionPlan> PlanEnglishCompletion(const std::vector<std::string>& readings, size_t cursor); // §3.2 第 1-3 步的前半
// fSL/EnglishWords.h
std::vector<std::string> EnglishWordStore::completions(const std::string& word);
std::vector<std::string> EnglishLexicon::wordsWithPrefix(std::string_view lowercased, size_t limit) const;
```

`size() < 2` 的檢查在 KeyHandler 做（要問 LM）。可重用 `TrailingLetters`（`fSL/MixedInput.h:31`）。

### 3.7 測試

Mac 無補完測試。建議：`ass` 列出字表字、學兩次 `ChatGPT` 後打 `chat` 排第一、學過 `associated` 後 `aocd` 找到、打一次的字排在字表之後、`Hel` 補完首字大寫、單一字母 Tab 響、Hsu `hel` + Tab。

---

## 4. 數字快打與中文模式直接算

- Mac 偏好：無。
- 使用說明：`UG:214-232`；變更記錄：learning.15、learning.16。

### 4.1 入口

數字快打不是獨立 state，而是**文字範本 state**（§6.1）的代碼「看起來像數字」時的另一種候選來源：

- `TextTemplate.entries(matching:)`：`QuickNumbers.isNumberCode(code)` 為真時，候選是 `QuickNumbers.candidates(for: code)`，每個都是 `Entry(code: "", text: 結果)`（`IS:364-367`）。
- 組字區標題：數字時「[數字快打] code」，否則「[文字範本] code」（`IS:307-313`、`L10N:126`、`L10N:283`）。
- 候選顯示：`code` 為空時直接顯示 text（`IS:319-323`）。
- Shift 加數字鍵選字時，`*`、`(`、`)`、`+`、`:` 不當選字鍵而當運算子輸入（`KH:369`，§6.1）。

### 4.2 正規化 `normalized`（`QN:47-58`）

1. 逐字換成半形：`（）＋－−＊×／÷．：＝` 與全形數字 `０-９`（`QN:36-45`）。
2. 再把「前後都是 ASCII 數字」的 `。` 或 `‧` 換成 `.`（`QN:50-56`）。

### 4.3 `isNumberCode`（`QN:61-67`）

正規化後：首字必須是 ASCII 數字或 `(`，而且全部字元是 ASCII 數字或 `./-:+*()=` 之一。空字串 → false。

### 4.4 `candidates(for:)`（`QN:69-86`）

1. 不是數字代碼 → `[]`。
2. 正規化，去掉結尾所有 `=`。
3. `results = date(code) + time(code)`。
4. 若全部是數字或 `.` → 加上 `amount(code)`；否則若 `Expression.evaluate(code)` 有值 → 加上 `arithmetic(code, value)`。
5. 依出現順序去重。

### 4.5 日期 `date`（`QN:109-168`）

1. 以 `.`、`/`、`-` 切開（略過空段）；所有段都是數字，且使用的分隔字元只有一種（`Set(separators).count == 1`）。
2. 三段：年月日。第一段長度 <= 3 視為民國年，加 1911（`QN:126-127`）。
3. 兩段且分隔字串**剛好是一個** `/`：月/日，年取今年，`hasYear = false`（`QN:130-138`）。
4. 其他 → `[]`。
5. 驗證：月 1-12、日 1-31、年 1-9999，且用公曆建出的日期的「日」等於輸入的日（排除 2/30 這類）（`QN:142-149`）。時區用目前時區。
6. 星期：`（星期日/一/.../六）`（全形括號）（`QN:150`）。
7. 有年時依序（`QN:154-165`）：
   - `2023年9月7日（星期四）`
   - `二〇二三年九月七日（星期四）`（年逐位數字用 `〇一二…九`，月日用 1-99 的中文數字）
   - `中華民國112年9月7日（星期四）`
   - `民國112年9月7日`（**沒有星期**）
   - `西元2023年9月7日（星期四）`
   - `2023/09/07`
   - `2023-09-07`
8. 一律再加：`9月7日（星期四）`、`九月七日（星期四）`（`QN:166`）。

中文 1-99 `chineseNumber`（`QN:243-250`）：`<10` 單字；否則十位為 1 時省略「一」，個位 0 省略，例如 十、十二、三十、九十九。

### 4.6 時間 `time`（`QN:172-194`）

條件：以 `:` 切成恰好兩段（保留空段）、都是數字、分鐘段長度恰好 2、時 0-23、分 0-59。依序輸出：

1. `12時30分`（`\(hour)時\(minute)分`，分為 0 也輸出「0分」）
2. `下午十二點三十分`：上午（時 < 12）／下午；12 小時制（0 與 12 都是 12）；分為 0 時不輸出分；分 < 10 時加「零」，例如 `上午九點零五分`
3. `下午12點30分`（分為 0 時省略）
4. `12:30 PM`（`%d:%02d`）
5. `PM 12:30`
6. `12:30`（`%02d:%02d`）

### 4.7 金額 `amount`（`QN:198-221`）

1. 以 `.` 切（保留空段），最多兩段，整數部分非空且長度 <= 16，否則 `[]`。
2. `grouped`：整數部分每三位加逗號，有小數再接 `.小數`（`QN:204`、`QN:252-261`）。
3. 沒有小數時依序：`6,548`、`陸仟伍佰肆拾捌元整`（大寫 + 元整）、`六千五百四十八`（小寫）、`6,548元`、`NT$6,548`、`六五四八`（逐位 `〇一…九`）、`陸伍肆捌`（逐位 `零壹貳參肆伍陸柒捌玖`）。
4. 有小數時依序：`grouped`、小寫中文數字、大寫中文數字、`grouped元`。

中文數字用 `ChineseNumbers.generate(intPart:decPart:digitCase:)`，與 fcitx 的 `ChineseNumbers::Generate(intPart, decPart, ChineseNumberCase)`（`src/ChineseNumbers/ChineseNumbers.h`）同源。兩邊對所有輸入是否逐字相同未逐一比對（**UNKNOWN**；建議以 Mac `Packages/ChineseNumbers/Tests/ChineseNumbersTests/ChineseNumbersTests.swift` 的案例對照 fcitx `ChineseNumbersTest.cpp`）。

### 4.8 四則運算

`Expression.evaluate`（`QN:276-353`）：

1. 字串不含 `+-*/` 任一者 → nil（單一數字不算運算式）。
2. 遞迴下降：`expression = term (('+'|'-') term)*`，`term = factor (('*'|'/') factor)*`，`factor = '-' factor | '(' expression ')' | 數字`。
3. 數字是連續的數字或 `.`，用 Swift `Double(String)` 解析，失敗 → nil。Swift 對 `.5`、`5.` 的解析結果（**UNKNOWN**，推測與 `strtod` 相同，可接受）。
4. 除以 0 → nil；必須吃完整個字串；結果必須是有限數。

`arithmetic(code, value)`（`QN:225-238`）依序：

1. `formatted(value)`
2. `10*15/6=25`（code 原樣）
3. `10×15÷6=25`（`*`→`×`、`/`→`÷`）
4. 若 `value >= 0`、是整數、`< 1e16`：`25元`（加千分位）、`貳拾伍元整`、`二十五`。

`formatted`（`QN:263-272`）：整數且絕對值 < 1e16 → 整數字串；否則 `%.6f` 再去掉結尾的 `0`。邊界：極小的非整數（如 1e-7）會變成 `0.`（結尾點不會被去掉）。

`9/7` 同時產生日期（今年 9 月 7 日）與除法結果，日期在前。

### 4.9 中文模式直接算（`=` 自動補答案）

`_appendArithmeticResultAfterKey:`（`KH:785-804`）：

1. 只在 `charCode == '='`、`InputModeBopomofo`、reading 為空時作用。
2. 取 `buildInputtingState` 的組字字串，切到游標前，再去掉最後一個字元（剛打的 `=` 或 `＝`）。Mac 以 UTF-16 長度計算；fcitx 的 `cursorIndex` 是 UTF-8 byte，必須以「最後一個 code point」為單位去掉。
3. `QuickNumbers.arithmeticResult(endingWith:)`（`QN:90-105`）：正規化後，從尾端往前收集 ASCII 數字與 `./-+*()`（**不含** `=` 與 `:`），再從最左邊開始依序嘗試每個起點，第一個能 `evaluate` 的就用（因此 `（2＋3` 會得到 `2+3` 的 5）。
4. 有結果 → `_insertLiteralText:result`（每個字元一個 `_literal_`，半形），更新 Inputting。

呼叫點（都在 `=` 已經插入組字區之後）：

- 英文模式下打可列印字元之後（`KH:1174`）
- 版面專屬標點之後（`KH:1613`）
- 通用標點之後（`KH:1620`）
- 沒有標點的符號鍵之後（`KH:1651`）

例：`（2＋3）＊3＝` → 組字區 `（2＋3）＊3＝15`；`3。5＊2＝` → `3。5＊2＝7`（`UG:224-231`）。

這要求數字以半形進組字區：許氏等不用數字鍵的配列靠 §7.1；標準配列的數字鍵是注音鍵，數字要靠 phase 2 的智慧英文（數字模式）。邊界：若開了「重複按標點選字」，連按兩次 `=` 會先循環候選再算（`KH:2207-2228`）。

### 4.10 fcitx 可重用與掛點

- 可重用：`ChineseNumbers::Generate`。`NumberInputHelper::FillCandidatesWithNumber`（`fNIH:37-95`）是上游 Ctrl+\ 功能選單的「數字輸入」，產出中文大小寫、羅馬數字、`_number_` 詞與蘇州碼，**與數字快打的格式與順序不同**，不能直接重用，只能參考。Mac 的同名 `NumberInputHelper.swift` 也只服務該功能選單。
- 新模組 `fSL/QuickNumbers.{h,cpp}`（連結 `ChineseNumbersLib`；需注入「今年」與時區／日曆函式以利測試）：

```cpp
namespace McBopomofo::SmartLearning::QuickNumbers {
std::string Normalize(std::string_view text);                 // §4.2，UTF-8
bool IsNumberCode(std::string_view code);                     // §4.3
std::vector<std::string> Candidates(std::string_view code, int currentYear); // §4.4-4.8
std::optional<std::string> ArithmeticResultEndingWith(std::string_view text); // §4.9 第 3 步
std::optional<double> Evaluate(std::string_view expression);
}
```

星期計算可用 `std::chrono::year_month_day` 與 `weekday`（C++20，SmartLearning 已是 C++20，`fSL/CMakeLists.txt:4`），不依賴系統時區即可得到公曆星期。

- `=` 掛點：fcitx 的標點處理在 `fKH:614-649`，`handlePunctuation` 成功後 return；Mac 是成功後再呼叫 append。在 `fKH:633-648` 各個 `return true` 前、以及英文模式可列印字元分支（`fKH:176-185`）與 §7.2 新分支後加入 `appendArithmeticResultAfterKey(charCode, stateCallback)`。

### 4.11 測試

Mac 無 QuickNumbers 測試。建議以 `UG:216-231` 範例為準：`2023.09.07`、`112.9.7`、`9/7`、`12:30`、`9:05`、`0:00`、`6548`、`6548.5`、`10*15/6`、`(2+3)*3=`、`1/0`、`2/30`、`2023.9/7`（分隔不一致）、全形 `（2＋3）＊3＝`、`3。5＊2＝`、`我算（2＋3`。

---

## 5. 常用句

- Mac 偏好：`CommonSentencesEnabled`，預設 **true**（`PF:45`、`PF:375-376`）。使用說明說在偏好設定「中英混打」可關閉（`UG:245`）。
- 使用說明：`UG:233-246`；變更記錄：learning.21。

### 5.1 常數（`CS:100-104`）

- `threshold = 3`（送出 3 次才建議）
- `minimumLength = 6`，`maximumLength = 40`
- `minimumPrefixLength = 2`
- `reloadInterval = 60` 秒

### 5.2 斷句與長度

- 分隔字元 `separators` = Unicode 標點（P* 類）∪ 符號（S* 類：Sm、Sc、Sk、So，**emoji 屬 So**）∪ 換行 ∪ tab（`CS:148-149`）。空白**不是**分隔，所以 `hello 你好` 是同一句。
- `length(of:)`（`CS:121-138`）：逐個字元（grapheme）；ASCII 字母或數字的連續段算 1；其他非空白字元各算 1；空白不算。
- `isValid`（`CS:142-146`）：長度 6-40，且不含分隔字元。
- C++ 對應：用 ICU `u_charType` 判斷 P*／S*；換行為 U+000A-U+000D、U+0085、U+2028、U+2029（Foundation `CharacterSet.newlines` 的定義）；grapheme 用 ICU `BreakIterator`（fcitx 已連結 ICU，`src/CMakeLists.txt:13`）。ASCII 字母數字只看單一 ASCII 字元。

### 5.3 記錄 `record`（`CS:166-189`）

1. 偏好關閉 → 不做。
2. 呼叫端傳入的是 commit 內容 = 所有節點的 `value()` 串接（`KH:445-448`），不是含注音變體標註的組字字串；只在 `InputModeBopomofo`（§0.1）。
3. 以分隔字元切段，各段去頭尾空白（`.whitespaces`），保留 `isValid` 者。沒有 → 不做。
4. `loadIfNeeded()`。對每句：本機與合併表的 `count += 1`、`lastUsed = now`（新項目從 count 0、pinned false 開始）。
5. `save()`：只寫本機檔（§5.6）。同步寫在主執行緒。

### 5.4 建議 `completion(forTypedText:)`（`CS:193-220`）

1. 偏好關閉 → nil。
2. `prefix` = 傳入文字中最後一個分隔字元之後的部分，再去掉開頭空白。
3. `prefix` 的字元（grapheme）數 < 2 → nil。
4. `loadIfNeeded()`。候選句必須同時：`pinned || count >= 3`（合併次數）、字元數 > `prefix` 字元數、以 `prefix` 開頭、`isAlive`。
5. 取次數最大者，同次數取 `lastUsed` 最新者。
6. 回傳去掉前 `prefix` 字元數之後的剩餘部分。

`isAlive`（`CS:158-163`）：`removedTime(sentence, 句子) >= lastUsed` → 死；否則 `pinned` 或 `now - lastUsed <= lifetime`。

`lifetime`（`CS:154-156`）：`90 天 + floor(max(0, count - 3) / 3) * 30 天`（整數除法）。建議時用合併次數，存檔時用本機次數。

### 5.5 KeyHandler 端

- 提示（`KH:3536-3546`，`buildInputtingState` 結尾）：每次建 Inputting 都先清掉 `_sentenceCompletion`；條件 `InputModeBopomofo`、原本沒有 tooltip（游標在讀音中間或注音標註的提示會優先）、reading 為空、tail 為空、游標在 grid 尾端。成立就以 head（游標前的組字字串）查詢，有結果則 tooltip = `Tab：<剩餘>`（`L10N:337`）。
- 所以「句首或標點後」是指**組字區內**：已送出的文字不算，查詢文字是整個組字區 head 最後一個分隔字元之後。
- Tab 接上（`KH:1133-1146`）：Tab、非 Shift、state 是 Inputting、reading 為空 → 先 `buildInputtingState`（刷新建議），有建議就 `_setSmartEnglishMode:NO`、`_insertLiteralText:completion`（每字一個 `_literal_`；若剩餘以 ASCII 字母開頭會觸發自動空白）、更新 Inputting。沒有建議 → 往下走一般 Tab 流程。
- 這個判斷在英文模式 Tab（英文聯想）之前，所以有提示時 Tab 先接句子。
- 接上的中文字是 `_literal_` + 3 bytes，phase 2 的 `IsChineseReading` 已處理（`docs/phase2-mac-behavior.md` §0.2）。

### 5.6 檔案（`CS:259-323`）

格式見 `docs/file-formats.md`：`learning/sentences/<本機學習檔名>`，一行 `句子\t次數\t最後使用時間\t釘選`。

- 讀：以 `\n` 切行、`\t` 切欄（保留空欄），至少 3 欄且次數能轉 Int、時間能轉 TimeInterval（`CS:263-278`）。
- `loadIfNeeded`：本機表已載入且距上次 < 60 秒就不重讀（`CS:280-285`）。
- `loadFiles`：本機檔只讀一次；合併 = 本機 + `sentences/` 下其他 `.txt`（次數相加、時間取大、釘選取聯集）（`CS:287-305`）。Mac 會讀隱藏的 `.txt`；Linux 用 `otherDevicesLearningFiles` 會排除隱藏檔與衝突副本（phase 1 規則）。
- `save`：濾掉 `!pinned && now - lastUsed > lifetime(本機項目)` 的項目，每行 `句子\t次數\t Int(時間)\t 0或1`，依行排序後 atomic 寫入（`CS:307-323`）。
- Mac 沒有「本機檔未載入就不寫」的保護；Linux 建議比照 phase 1 加上 pending。

### 5.7 fcitx 掛點

- `buildInputtingState`（`fKH:1760-1771`）：在 return 前比照 §5.5 計算，存進成員 `sentenceCompletion_`，tooltip 帶進 `Inputting` 建構子（`fIS:97-104` 已有 tooltip 參數）。`composedString.tooltip` 非空時不建議。
- Tab：在 `fKH:147`（smart 區塊之前）加入，條件同 Mac；需要判斷 `dynamic_cast<InputStates::Inputting*>(state)`。
- 顯示：`fMB:2239` 已把 tooltip 放進 `setAuxDown`。
- commit：`learnFromCommittedComposition`（§0.1）呼叫 `CommonSentenceStore::record`。
- 設定：新增 `fcitx::Option<bool> commonSentencesEnabled{this, "CommonSentencesEnabled", ..., true}`，放在 `fMBh:199-235` 的智慧學習選項群。
- 存檔：記錄後排程到 `scheduleLearningSave`（`fMB:762-776`，2 秒延遲）比 Mac 每次同步寫入更適合同步資料夾；這是 Linux 差異，可接受。

### 5.8 模組邊界

```cpp
namespace McBopomofo::SmartLearning {
size_t SentenceLength(std::string_view sentence);            // §5.2
bool IsValidSentence(std::string_view sentence);
std::vector<std::string> SplitSentences(std::string_view committed);
class CommonSentenceStore {
 public:
  CommonSentenceStore(std::string deviceID, std::string deviceName, std::function<double()> wallNow,
                      std::shared_ptr<RemovedEntries> removed);
  void setLearningFolder(std::filesystem::path learningFolder);
  void setEnabled(bool);
  void record(std::string_view committed);
  std::optional<std::string> completion(std::string_view typedHead);
  bool save();
  bool hasUnsavedChanges() const;
};
}
```

### 5.9 測試

Mac 無常用句測試。建議：`謝謝您的耐心等候` 送三次後 `謝謝` 提示、送兩次不提示、有標點的句子分段記錄、`ChatGPT 很好用的工具` 長度計算、41 字不記、emoji 斷句、90 天過期、次數 9 時壽命 150 天、釘選不過期、removed 時間比較、兩裝置次數相加、提示在游標不在尾端時不出現。

---

## 6. 文字範本與分類符號表

- Mac 偏好：無。符號表外觀跟隨「面板主題」（`CandidatePanelTheme`，`IMC:1059-1064`），與本規格無關。
- 使用說明：`UG:258-277`；變更記錄：learning.9-11、learning.15。

### 6.1 文字範本 state

進入（`KH:1555-1576`，反引號、無 Ctrl/Cmd/Opt）：

1. reading 非空 → `errorCallback`，return YES（測試 `testInputBufferThenBacktickThenEsc`）。
2. `InputModeBopomofo`：
   - `createSampleFileIfNeeded`（檔案不存在且使用者詞檔可建立時，寫入在地化範例，`IS:342-353`、`L10N:124`）。
   - state 是 Inputting → `_learnFromCommittedComposition` 後 commit 組字區。
   - `clear`，進入 `TextTemplate(code: "")`。
3. Plain Bopomofo：直接 `_enterSymbolTableWithKey:"_symbols_"`（§6.3）。

state 內容（`IS:288-428`）：

- `entries(matching:)`（`IS:357-378`）：數字代碼走 §4；空代碼列出全部；否則依序為代碼完全相同（不分大小寫）、代碼以輸入開頭、代碼包含輸入或內容包含輸入（`localizedCaseInsensitiveContains`）。
- 讀檔（`IS:382-402`）：`components(separatedBy: .newlines)`，`#` 開頭略過，第一個 tab 切兩段，代碼去頭尾空白；代碼或內容為空略過。
- `unescape`（`IS:404-426`）：`\n` 換行、`\t` tab，其他 `\x` 變成 `x`（所以 `\\` 是反斜線），結尾孤立的 `\` 保留。
- 候選顯示（`IS:319-329`）：`代碼  內容預覽`（兩個空格），預覽把換行換成 `⏎`，超過 30 字截斷加 `…`。
- 每次輸入都重新讀檔。

按鍵（`_handleTextTemplateState`，`KH:339-388`；分派見 `KH:1016-1024`）：

1. Esc → `Empty`（`KH:345-348`）。
2. Backspace／Delete：代碼為空 → `Empty`；否則去掉最後一字（`KH:349-357`）。
3. 反引號 → `clear`，`_enterSymbolTableWithKey:"_symbols_"`；失敗就響（`KH:359-365`）。不會再 commit（測試 `testDoubleBackquoteForceCommit`）。
4. Shift 按住且不是數字運算子：若輸入（忽略修飾鍵）等於某個選字鍵（不分大小寫）→ return NO，交給候選處理選字（`KH:368-376`）。數字運算子 = 代碼是數字代碼且字元是 `* ( ) + :` 之一（`KH:369`）。
5. 可列印 ASCII 且不是空白：代碼長度 > 50 → 響；否則附加（`KH:377-385`）。所以標點也會附加（測試 `testBacktickThenPunctuation`：`<` 成為代碼）。
6. 其他 → NO。若沒有候選 → 吞掉該鍵；有候選 → 走候選處理（`KH:1016-1024`）。

候選處理中與範本相關的部分：

- 選字一律要 Shift + 選字鍵（`KH:2954-2970`），按鍵標籤顯示為 `⇧ 1` 等（`IMC:1043-1050`）。
- Enter → commit 反白項目的內容，再 `Empty`（`KH:2691-2699`）。
- 空白 → 下一頁（`KH:2760`）。
- 候選窗一律直式（`IMC:956-963`），字級上限 32（`IMC:1018-1021`）。
- 選字後 commit 內容並 `Empty`（`IMCc:172-175`）。

邊界情形：

- 空代碼時按 Shift+9 想輸入 `(`：此時 `isNumberCode("")` 為假，Shift+9 會被當成選第 9 個候選；候選不足 9 個時這個鍵會被吞掉，`(` 打不進去。
- 預設選字鍵是 `123456789`（`PF:80`）。

### 6.2 fcitx 對應

fcitx 目前的反引號是上游行為：插入 `_punctuation_list` 並進入 `ChoosingPunctuationList`（`fKH:589-612`）；在清單中再按反引號進功能選單（`fMB:1274-1286`）。Mac fork 在 Bopomofo 模式改成文字範本，Plain 模式改成 `_symbols_` 符號表，原本「清單中再按反引號開功能選單」的路徑（`KH:2385-2400`）只在符號表內、且該鍵不是選字鍵時才會用到。

- 新 state：`InputStates::TextTemplate : NotEmpty`，比照 `NumberInput`（`fIS:269-282`），持有 `code` 與 `entries`。
- engine：
  - `keyEvent` 比照 `NumberInput` 的預先分派（`fMB:1093-1107`）。
  - 加入候選 state 清單（`fMB:1127-1137`、`fMB:1163-1172`）。
  - `handleCandidatesState` 的 `useShiftKey`（`fMB:1931-1932`）加入 TextTemplate。
  - Shift 選字目前只能用原始 keycode Shift+1..9（`fMB:1244-1252`），與 Mac 的「Shift + 任一選字鍵」不同；在 TextTemplate 數字代碼時，Shift+8／9／0、Shift+=、Shift+; 必須先交給 KeyHandler 當運算子，不可被這段攔截。
- KeyHandler：新增 `handleTextTemplateInput(Key, InputStates::TextTemplate*, ...)`，比照 `handleNumberInput`（`fKH:1382-1447`）。
- 反引號：在 `fKH:589` 的分支改為 Mac 行為（Bopomofo 模式進範本；Plain 模式進符號表）。
- 範例檔：使用者資料夾只在預設資料夾建立範本（`fSL/UserDataFolder.h:27-28` `shouldCreateTemplateFiles`），`text-templates.txt` 建議同規則，並用 `createFileIfAbsent`（`fSL/AtomicFile.h:19`）。

模組（純 C++）：

```cpp
namespace McBopomofo::SmartLearning {
struct TextTemplateEntry { std::string code, text; };
std::vector<TextTemplateEntry> ParseTextTemplates(std::string_view content); // 含 unescape
std::vector<TextTemplateEntry> MatchTextTemplates(const std::vector<TextTemplateEntry>&, std::string_view code); // 不分大小寫比對
std::string TextTemplateDisplayText(const TextTemplateEntry&);
}
```

`localizedCaseInsensitiveContains` 在 C++ 可用 ICU case folding；ASCII 代碼用簡單 `tolower` 即可。中文內容的比對結果與 Mac 是否完全一致屬 **UNKNOWN**（只影響非 ASCII 的大小寫變體）。

### 6.3 符號表資料（`LMM:578-613`、`LM:186-196`、`LM:302-393`）

載入（`_loadSymbolTable`，每次 `loadUserPhrases` 時，`LMM:182-188`）：

1. 讀 `symbol-table.txt`；內容為空或不存在 → 用內建 `kDefaultSymbolTable`（`LMM:59-87`，30 類，含 learning.15 加入的 emoji 分類）。
2. 以換行切行；`#` 或 BOM+`#` 開頭略過；沒有 tab 略過。
3. 分類名稱：tab 前的部分去掉 BOM、`*`、空格（行尾 `*` 是其他輸入法符號表格式的分類標記）。
4. 符號：tab 後以 ASCII 空格切開，各自去掉空格、tab、`\r`，非空才收（全形空白是符號）。
5. 沒有符號的分類略過。
6. `setSymbolTable` 只設給 Bopomofo 模式的 LM（`LMM:612`）；Plain 模式的 LM 沒有符號表，會用 `_punctuation_list`。

選單「編輯符號表」在檔案不存在時寫入「註解 + 內建表」（`AD:405-412`、`LMM:573-576`、`L10N:125`）。

`_symbols_<key>` 的候選（`LM:186-196`、`LM:302-349`）：

1. `getSymbolGroups(punctuationKey)`：第 0 組是該標點 key 的候選，再加上 `_punctuation_list_<最後一個字元>` 的候選（去重）。key 非空但第 0 組為空 → 整個回傳空（不是標點）。
2. 有符號表：清空第 0 組，接上每個分類一組。沒有符號表：接上 `_punctuation_list` 一組。
3. 所有符號分數 0。
4. `getSymbolSections`（`LM:352-372`）：各非空組的（標題、數量）；第 0 組標題是 `此鍵`（`L10N:121`），其餘用分類名稱（無符號表時為空字串）。
5. `getSymbolInitialIndex`（`LM:374-393`）：標點 key 的第一個值在所有組中的第一個位置，找不到為 0；`_symbols_`（空 key）為 0。
6. Engine 測試：`McBopomofoLMTest.SymbolsForPunctuation`（`Source/Engine/McBopomofoLMTest.cpp:139-189`），fcitx 已有同一份。

### 6.4 進入符號表的路徑（KeyHandler）

`_enterSymbolTableWithKey:`（`KH:289-306`）：LM 沒有該 key → NO；清空 `_readingBeforeSymbolTable`；插入 key、walk；記下 `originalCursorIndex`，若 `selectPhraseAfterCursorAsCandidate` 游標減 1；建立 `ChoosingPunctuationList`。

1. 範本中再按反引號：`_symbols_`（§6.1）。
2. Plain 模式按反引號：`_symbols_`（`KH:1573-1575`）。
3. 打完標點按空白（空白是選字鍵時）或 ↓：在 `Space and Down` 區塊（`KH:1393-1442`）的 `KH:1430` 呼叫 `_expandPunctuationToSymbolTableWithInput`（`KH:310-337`）：
   - 只在 `InputModeBopomofo`。
   - 游標處節點 spanningLength 為 1，reading 以 `_` 開頭且含 `punctuation_`（所以 `_half_punctuation_`、`_ctrl_punctuation_` 也算），但不是 `_symbols_` 或 `_punctuation_list` 開頭。
   - `_symbols_` + reading 有候選。
   - 刪掉該標點 reading，插入 `_symbols_<reading>`，並把原 reading 存到 `_readingBeforeSymbolTable`，取消時放回。
   - 反白從該標點在表中的位置開始（`IMC:1065-1071`）。
4. 在符號表中按某個標點鍵（且不是選字鍵）：若 `_punctuation_list_<鍵>` 有候選，刪掉目前的符號 reading，在 Bopomofo 模式改以 `_symbols__punctuation_list_<鍵>` 重開符號表（`KH:2402-2418`）；Plain 模式插入 `_punctuation_list_<鍵>`。

取消（Esc／Backspace 且不在選格階段，`KH:2646-2665`）：游標設回 `originalCursorIndex`，刪掉游標前的 reading，若有 `_readingBeforeSymbolTable` 就插回，grid 為空則 `EmptyIgnoringPreviousState`，否則 walk 後回 Inputting。

選定符號：走一般 `ChoosingCandidate` 的選字（`IMCc:51-58` → `fixNodeWithReading`），reading 保留為 `_symbols_...`，值是選定符號；會觀察 UOM，學習函式不作用。

候選顯示（`KH:3556-3590`）：同值多個候選會加「(讀音)」，但 **reading 以 `_` 開頭的不加**（`KH:3568-3573`），因為符號表中同一符號會出現多次（例如內建表的 ⭐、🔥、👉）。fcitx `fMB:2000-2020` 沒有這個例外，需要補上。

### 6.5 Mac 的格狀候選窗（`GC`、`KH:2361-2382`、`KH:2804-2823`）

排版（`GC:328-366` `buildRows`）：

1. 各段（section）的數量總和不等於候選數 → 全部當成一段無標題。
2. 依字型量測 cell 寬高；可見列數 = max(12, 螢幕高 × 0.6 / cell 高)。
3. 欄數上限 `maxColumns = max(min(max(前兩段中較長者的數量, 12), 螢幕寬 × 0.75 可放的欄數), 選字鍵數, 1)`。
4. 每段依 `maxColumns` 換行成多列，只有每段第一列有標題。

兩鍵選字：

1. 狀態 `isChoosingRow`，`reloadData` 與任何移動反白後都重設為 true（`GC:398-406`、`GC:435-447`）。
2. 選列階段：從反白所在列往下，依序在左側標上選字鍵（`rowKeyLabelRange`，`GC:113-124`）。
3. 按選字鍵（不分大小寫比對 `keyLabels`）：選列階段 → `chooseRow(atKeyLabelIndex:)`，反白移到該列第一格、`isChoosingRow = false`；超出範圍 → 響（`KH:2366-2378`、`GC:483-495`）。
4. 選格階段：標籤從反白那一格開始往右依序標示（`GC:174-181`）；按選字鍵 → `candidateIndexAtKeyLabelIndex` = 反白 + 索引，超出該列 → 無效（`GC:505-513`），走一般選字（`KH:2984-2992`）。
5. 選格階段按 Backspace → `cancelChoosingCandidateInRow`，回選列（`KH:2379-2382`、`GC:498-501`）。
6. 方向鍵：左右移一格、上下移一列（保留欄位，不足取該列最後一格），移動後回到選列階段（`KH:2804-2823`、`GC:449-470`）。
7. 空白／PageDown：往下一頁的列數；PageUp 往上（`GC:418-427`）。
8. Enter：選反白項目。
9. 滑鼠點擊：直接選（`GC:524-528`）。
10. 符號表的選字鍵比對在標點清單判斷之前，所以選字鍵含 `;` 時按 `;` 不會重開標點清單（learning.11 的修正，`KH:2384` 的 `gridKeyLabelIndex == NSNotFound`）。

### 6.6 fcitx5 能做到多少

fcitx5 的 `CommonCandidateList` 是線性分頁清單：

- 可設：`setPageSize`、`setLabels`（依頁內位置的標籤，不是依候選）、`setLayoutHint`（`NotSet`／`Vertical`／`Horizontal`）、`setGlobalCursorIndex`、`setCursorIncludeUnselected`。
- 每個 `CandidateWord` 可帶 comment 文字。
- 沒有二維格狀排版、沒有段落標題、標籤不能跟著反白在頁內浮動。
- 個別候選的自訂標籤（`CandidateWord::setCustomLabel`）是否存在於目標 fcitx5 版本：**UNKNOWN**。
- 第三方面板（例如 kimpanel）的呈現方式：**UNKNOWN**。

最接近的作法是用兩個清單模擬兩鍵選字，把邏輯放進純 C++ 的 `SymbolGrid` 模型：

1. **選列畫面**：直式清單，每個候選是一列，顯示文字為 `分類標題  符號 符號 …`（每段第一列帶標題，其餘列只有符號），頁大小 = 選字鍵數，頁面從反白所在列開始。按選字鍵 → 進入該列。↑↓ 移動列。
2. **選格畫面**：橫式清單，候選是該列的各格，頁大小 = 選字鍵數，標籤 `setLabels` 用選字鍵；←→ 移動格，↑↓ 移到上下列的同一欄並回到選列畫面（比照 Mac 第 6 步）。Backspace 回選列畫面。按選字鍵或 Enter 選定。
3. 欄數：Mac 依螢幕寬計算；Linux 拿不到面板寬度，建議 `maxColumns = 選字鍵數`（預設 9），讓每一格都有標籤，代價是列數變多。這是 Linux 差異，是否可接受交給 plan 決定。
4. 初始反白：`getSymbolInitialIndex` 換算成（列、格），開啟時直接停在選格畫面或選列畫面？Mac 開啟時是選列階段、反白在該符號（`GC:398-406` 後再設 `selectedCandidateIndex`，`IMC:1065-1071`，設定時 `isChoosingRow = true`）。Linux 建議同樣進選列畫面，頁面從該列開始。

純 C++ 模型：

```cpp
namespace McBopomofo::SmartLearning {
struct SymbolSection { std::string title; size_t count; };
class SymbolGrid {
 public:
  SymbolGrid(std::vector<SymbolSection> sections, size_t candidateCount, size_t maxColumns, size_t keyCount);
  struct Row { std::optional<std::string> title; size_t start, count; };
  const std::vector<Row>& rows() const;
  size_t highlighted() const; bool isChoosingRow() const;
  bool chooseRow(size_t keyIndex);            // §6.5 第 3 步
  std::optional<size_t> candidateAt(size_t keyIndex) const; // 第 4 步
  void cancelChoosingCell();                  // 第 5 步
  bool moveBy(int cells); bool moveByRows(int rows); bool setHighlighted(size_t);
  std::pair<size_t,size_t> labeledRowRange() const; // 第 2 步
};
}
```

fcitx 掛點：

- 進入：`fKH:398-410`（空白／↓ 進候選）加 §6.4 第 3 條；`fKH:589-612` 改反引號；`candidatePanelPunctuationMaybeEntered`（`fKH:759-796`）改成以 `_symbols__punctuation_list_<鍵>` 重開。
- 取消：`candidatePanelPunctuationListCancelled`（`fKH:798-821`）目前用 `selectPhraseAfterCursor` 決定刪前或刪後，並把游標設成 `originalCursor - 1`；Mac 是游標設回 `originalCursorIndex` 後刪游標前一個 reading，並放回 `_readingBeforeSymbolTable`。要改成 Mac 行為。
- engine：`handleCandidateKeyEvent`（`fMB:1200-1700`）中 `ChoosingPunctuationList` 的按鍵要先交給 `SymbolGrid`，在選字鍵判斷（`fMB:1254-1265`）之前；`handleCandidatesState`（`fMB:1913-2133`）依 `SymbolGrid` 狀態產生兩種清單。
- 資料：engine 載入使用者詞時呼叫 `lm->setSymbolTable(ParseSymbolTable(檔案內容或內建表))`；內建表文字搬到 `fSL/SymbolTable.cpp`，解析函式 `ParseSymbolTable(std::string_view)` 可測。

### 6.7 測試

Mac 測試（`McBopomofoTests/KeyHandlerBopomofoTests.swift`）：

- `testPunctuationTable`（656）
- `testInputBufferThenBacktickThenEsc`（3079）
- `testInputBufferWithCharactersThenBacktickThenEsc`（3114）
- `testBacktickKey`（3161）
- `testBacktickThenEsc`（3177）
- `testBacktickThenBackspace`（3199）
- `testBacktickThenBacktick`（3220）
- `testBacktickThenPunctuation`（3241）
- `testDoubleBackquoteForceCommit`（3262 起）

fcitx 對應的上游測試是舊行為（反引號直接進標點清單），需改寫。另建議：`SymbolGrid` 列切分、兩鍵選字、Backspace 回選列、方向鍵回選列、初始反白、範本解析與跳脫、比對順序、50 字上限、`symbol-table.txt` 的 `*` 與 BOM、全形空白。

---

## 7. 許氏數字進組字區、無標點符號鍵打全形

變更記錄：learning.7（數字）、learning.16（符號）。Mac 偏好：無（符號全半形依 `HalfWidthPunctuationEnable` 與 `HalfWidthPunctuationAfterEnglish`）。

### 7.1 數字（`KH:1578-1597`）

位置：反引號區塊之後、標點之前。條件：

1. `'0' <= charCode <= '9'`，沒有 Ctrl／Cmd／Opt。
2. state 是 `NotEmpty`（組字區已有內容；**不是**「有中文」，任何內容都算）。
3. LM 有 `_numpad_<d>`。Plain 模式的資料也有，所以 Plain 模式同樣適用。

動作：reading 非空 → 響並維持 state；否則插入 `_numpad_<d>`，`overrideCandidate(cursor-1, Candidate(reading, 半形數字))`（強制半形），walk，更新 Inputting。

只有數字鍵不是注音鍵的配列會走到（標準配列的數字鍵是注音鍵，在 BPMF 區塊就被吃掉）；smart 開啟時，未完成的注音後接數字會先被 phase 2 規則轉成英文，所以「reading 非空」只在 smart 關閉時發生。組字區為空時這段不作用，數字直接交給應用程式（後面的符號分支排除了英數字）。

fcitx 現況：許氏數字在 `fKH:614` 起嘗試 `_punctuation_Hsu_<d>`、`_punctuation_<d>`（資料中都沒有），最後落到 `fKH:693-704`：NotEmpty 時響並吞鍵，Empty 時放行。掛點：`fKH:612` 之後、`fKH:614` 之前插入。`grid_.overrideCandidate` 有接受 `Candidate` 的多載（`pinNode` 用的就是，`fKH:2004-2006`）。

### 7.2 沒有標點的符號鍵（`KH:1645-1652`）

位置：標點與大寫字母處理之後。條件：

1. `InputModeBopomofo`，reading 為空。
2. `0x20 < charCode < 0x7f` 且不是英數字。
3. 沒有 Ctrl／Cmd／Opt。
4. state 是 `NotEmpty` 或 `Empty`（空組字區也會開始組字）。

動作：

- 若 `HalfWidthPunctuationEnable` 或 `_isAfterHalfWidthAlphanumeric` → 半形原字元，否則 `charCode + 0xFEE0`（全形）。
- `_insertLiteralText`，更新 Inputting，再呼叫 `_appendArithmeticResultAfterKey`（例：許氏 `10／2＝`）。

fcitx 掛點：`fKH:683`（大寫字母區塊結束）之後、`fKH:685`（數字鍵盤）之前。`MapFcitxKey` 對簡單鍵一律傳 `asciiKey(sym, false)`（`fMB:96-98`），所以 Shift 產生的符號也是 `shiftPressed == false`，用 `key.ascii` 判斷即可。`isAfterHalfWidthAlphanumeric()` 已存在（`fKH:964-973`）。

### 7.3 模組

這兩條都只有幾行，不需要獨立模組；全形換算可放在 `MixedInput`：`std::string FullWidthSymbol(char c)`（U+FF01-U+FF5E，UTF-8 編碼）。

### 7.4 測試

Mac 無專門測試。建議：許氏 `ㄋㄧˇ` 後打 `2026` 為半形；許氏空組字區打 `1` 放行；許氏 `/` → `／`；半形模式 `/` → `/`；`10／2＝` → `10／2＝5`。

---

## 8. RemovedEntries：phase 3 只需要讀

### 8.1 結論

- 所有寫入 removed.txt 的呼叫端都在詞彙管理員（§0.3 第 4 條）。KeyHandler、候選窗 `+`/`-`、暫時詞轉正（`removeTemporaryPhrase`）、常用句記錄都**不寫** removed.txt。
- `+`/`-` 對英文字（`_literal_`、`_english_`）、常用句、符號都會被 §2.1 第 4 條擋下並響，Mac 沒有這條路徑。
- 所以 phase 3 **只需要讀**：
  - `temporary`：`removedTime >= 該筆時間` 視為刪除（§1.4）。
  - `sentence`：`removedTime >= lastUsed` 視為刪除（§5.4）。
  - `english`：有記錄（時間不為 0）即刪除（phase 2 已實作於 `fSL/EnglishWords.cpp:186-200`）。
- Linux 若日後要做詞彙管理員，再加寫入 API。

### 8.2 模組

把 `EnglishWords.cpp` 的私有讀取抽出成共用類別：

```cpp
namespace McBopomofo::SmartLearning {
class RemovedEntries {
 public:
  static constexpr char kEnglish[] = "english", kSentence[] = "sentence", kTemporary[] = "temporary";
  RemovedEntries(std::function<double()> monotonicNow);   // 60 秒快取用
  void setLearningFolder(std::filesystem::path learningFolder);
  double removedTime(std::string_view kind, std::string_view key);  // 無記錄為 0
  // 以後給詞彙管理員：add / remove（重讀、修改、排序、整數時間、atomic 覆寫）
};
}
```

`EnglishWordStore`、`TemporaryPhraseStore`、`CommonSentenceStore` 共用同一個實例。讀檔規則照 `CS:67-77`：保留空欄、恰好 3 欄、第 3 欄可轉數字。`temporary` 的 key 是 `值 讀音`，中間是空格。

---

## 9. 偏好對照（Mac → 建議 fcitx）

- `CommonSentencesEnabled`，預設 true（`PF:45`、`PF:375-376`）→ 新增同名 `fcitx::Option<bool>`。
- 自動學習（暫時詞、拆詞）：Mac 無開關，fcitx 也不加。
- `+`/`-`、文字範本、數字快打、`=` 直接算、許氏數字、符號全形：Mac 無開關。
- 英文聯想：跟隨 `SmartChineseEnglishSwitchEnabled`（fcitx 已有，`fMBh:199-205`）。
- 符號全半形：`HalfWidthPunctuationEnable`（fcitx 已有 HiddenOption，`fMBh:295-298`）與 `HalfWidthPunctuationAfterEnglish`（`fMBh:211-215`）。
- 候選窗主題 `CandidatePanelTheme`：fcitx 由面板主題決定，不移植。

---

## 10. 實作模組總表（全部放 `src/SmartLearning/`，不含 fcitx header）

1. `LearnPhrase`：§1.2、§1.3、§1.6 的純函式。
2. `TemporaryPhraseStore`：§1.4。
3. `UserPhraseFile`：§1.5 `learn`、§2.2 `write`／`remove`（`LanguageModelLoader` 改為呼叫它）。
4. `CandidateAdjustment`：§2.4。
5. `EnglishWordStore::completions`、`EnglishLexicon::wordsWithPrefix`、`PlanEnglishCompletion`：§3。
6. `QuickNumbers`：§4（連結 `ChineseNumbersLib`）。
7. `CommonSentenceStore`（連結 ICU）：§5。
8. `TextTemplates`：§6.1、§6.2。
9. `SymbolTable`（解析與內建表）、`SymbolGrid`：§6.3-§6.6。
10. `RemovedEntries`：§8。

KeyHandler 新增的狀態：`phrasesPickedInComposition_`、`sentenceCompletion_`、`readingBeforeSymbolTable_`，以及 `setOnPhraseLearned` 回呼。`reset()` 要清前三者。

`NoFcitxIncludeTest.cmake` 會檢查以上模組沒有 include fcitx header。

---

## 11. Mac 測試總覽

- 有測試：符號表的 LM 行為（`McBopomofoLMTest.SymbolsForPunctuation`）、UOM 次數（`UserOverrideModelTest` 的 `SerializeAndDeserialize`、`DeserializeAppendMergesCounts`、`ObservationCountOfBrokenUpPhrase`）、反引號與範本的 KeyHandler 行為（§6.7 所列 9 個）。
- 沒有測試：暫時詞、`learnUserPhrase`、KeyHandler 學習流程、`+`/`-`、英文補完、QuickNumbers、`=` 直接算、常用句、RemovedEntries、許氏數字、符號全形、格狀候選窗（`Packages/CandidateUI/Tests` 只有直式與橫式候選窗的測試）。

---

## 12. UNKNOWN 清單

1. 暫時詞同讀音多個值的順序：Mac 以 `NSMutableDictionary` 迭代順序交給 `setTemporaryPhrases`，不保證順序（§1.4）。
2. fcitx `TimestampedPath` 的時間精度，同一秒內寫入 data.txt 後 `reloadUserModelsIfNeeded` 能否偵測（§1.7）。
3. fcitx 大寫字母 commit 路徑（`fKH:673`）是否也要學暫時詞與常用句；Mac 在該路徑不學（§0.1）。
4. macOS 拼字檢查器 `guesses` 是否能由 `aocd` 找到 associated（§3.3）；以及其 `completions` 的排序依據（`UG:203` 範例看起來不是字母序）（§3.4）。
5. Swift `Double(String)` 對 `.5`、`5.` 等寫法的解析結果，推測同 `strtod`（§4.8）。
6. Mac `ChineseNumbers.generate` 與 fcitx `ChineseNumbers::Generate` 是否對所有輸入逐字相同（§4.7）。
7. `localizedCaseInsensitiveContains` 對非 ASCII 內容的比對，與 C++ 實作是否完全一致（§6.2）。
8. 目標 fcitx5 版本是否提供 `CandidateWord::setCustomLabel`，以及各面板（classic UI、kimpanel 等）對直式長列候選的呈現（§6.6）。
9. Linux 格狀符號表的欄數：拿不到面板寬度，`maxColumns = 選字鍵數` 是提案，Mac 依螢幕寬度計算（§6.6）。
