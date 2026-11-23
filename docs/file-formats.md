# 使用者資料夾檔案格式

本文件說明「智慧學習版」使用者詞庫資料夾內每個檔案的格式與讀寫規則。內容整理自 macOS 版原始碼（儲存庫 `McBopomofo`，分支 `smart-learning`，commit `7446fd0`），Linux 版必須與之完全相容，讓兩邊可以共用同一個同步資料夾。

引用格式：`檔案:行號` 皆指上述 commit 的原始碼。

- `LMM` = `Source/LanguageModelManager.mm`
- `UOM` = `Source/Engine/UserOverrideModel.cpp`
- `CS` = `Source/CommonSentences.swift`
- `EW` = `Source/EnglishWords.swift`
- `BBD` = `Source/Engine/ByteBlockBackedDictionary.cpp`／`.h`
- `IS` = `Source/InputState.swift`
- `VM` = `Source/VocabularyManager.swift`

所有檔案皆為 UTF-8，換行為 `\n`。

## 資料夾配置

```
<資料夾>/                      (LMM:550 dataFolderPath)
  data.txt                     使用者詞
  data-plain-bpmf.txt          純注音模式的使用者詞
  exclude-phrases.txt          排除詞
  exclude-phrases-plain-bpmf.txt
  phrases-replacement.txt      詞彙替換
  text-templates.txt           文字範本
  symbol-table.txt             符號表
  learning/                    (LMM:671)
    <裝置ID>-<名稱>.txt        選字學習（每台裝置一份）
    removed.txt                詞彙管理員刪除記錄
    temporary/<裝置ID>-<名稱>.txt
    english/<裝置ID>-<名稱>.txt
    sentences/<裝置ID>-<名稱>.txt
```

檔名常數見 LMM:630-653、LMM:563-571。

## 通用詞庫格式（data.txt、exclude-phrases.txt、phrases-replacement.txt）

三者共用同一個解析器（BBD:474-690），規則如下：

- 以 `#` 開頭的行是註解（BBD:533、BBD:614）。行首空白、空行會被略過（BBD:70-85，空白指空格、tab、`\r`、`\n`）。
- 欄位以空格或 tab 分隔，欄位間可有多個空白。
- 只有一欄的行會被忽略並記為警告（`MISSING_SECOND_COLUMN`）。
- 檔案內含 NUL 字元則整份解析失敗（BBD:509-522）。
- 值尾端的空白會被去除（BBD:583-601）。

### data.txt、data-plain-bpmf.txt

- 欄位順序為「值 讀音」（value-then-key；`UserPhrasesLM::load` 使用 `VALUE_THEN_KEY`，`Source/Engine/UserPhrasesLM.cpp:56-58`）。最後一個不含空白的字串是讀音，其前的內容（含內部空白）是詞。
- 讀音的注音音節以 `-` 連接，例如 `今天 ㄐㄧㄣ-ㄊㄧㄢ`（讀音格式見 `Source/Engine/McBopomofoLMTest.cpp:44,356`）。
- 同一讀音有多個詞時，依檔案內出現順序成為候選（`dict_[key].emplace_back`，BBD:685），以前面的優先。使用者詞的分數固定為 0（`UserPhrasesLM.h:58`）。
- 寫入方式：
  - 新增詞：附加在檔尾；若檔案最後一個字元不是 `\n`，先補一個換行（LMM:337-374）。同一「詞 讀音」已存在則不寫（LMM:437）。
  - 學習（`learnUserPhrase`）：把詞放到同讀音第一筆之前，讓最近的選擇優先（LMM:474-509）。若該詞已是同讀音的第一筆則不動；若已在排除檔則不加入（LMM:456）。需要改動時整份讀入後以 atomic 方式覆寫（LMM:504）。
  - 刪除詞：整份讀入，濾掉符合「詞 讀音」（以單一空格切成恰好兩欄）的行後 atomic 覆寫（LMM:376-425）。欄數不是兩欄的行（含註解）原樣保留。
- 刪除詞時會同時寫入 `exclude-phrases.txt`；新增詞時會從 `exclude-phrases.txt` 移除同一筆（LMM:427-446、LMM:511-530）。

### exclude-phrases.txt、exclude-phrases-plain-bpmf.txt

- 格式與 data.txt 相同（`excludedPhrases_` 也是 `UserPhrasesLM`，`Source/Engine/McBopomofoLM.h:207`）：`詞 讀音`。
- 查詢某讀音時，排除檔中同讀音的詞會從使用者詞、暫時詞與系統詞庫中濾掉（`Source/Engine/McBopomofoLM.cpp:201-239`、`:487`）。
- 寫入方式同上：附加於檔尾（LMM:524），移除時整份 atomic 覆寫。

### phrases-replacement.txt

- 欄位順序為「原詞 取代詞」（key-then-value，預設模式，`PhraseReplacementMap.cpp:54`）：第一欄為原詞，其後到行尾（去除尾端空白）為取代詞。
- 啟用時，每個候選詞的值若在此檔有對應就換成取代詞（`McBopomofoLM.cpp:492-499`）。重複的原詞取第一筆（`PhraseReplacementMap.cpp:57-63`）。
- 由使用者手動編輯，程式不寫入。

## text-templates.txt

- 每行 `代碼<Tab>內容`（IS:376-403）。以 `#` 開頭的行是註解。以第一個 tab 切成兩段（`maxSplits: 1`），代碼去除前後空白；代碼或內容為空的行略過。
- 內容中的跳脫：`\n` 換行、`\t` tab、`\\` 反斜線（IS:405-420 的 `unescape`，其餘字元前的反斜線被去除）。
- 比對順序：代碼完全相同（不分大小寫）、代碼前綴相同、代碼或內容包含輸入（IS:356-378）。
- 若檔案不存在，程式會建立一份附範例的檔案（IS:340-352）。
- 寫入：詞彙管理員修改時整份 atomic 覆寫（VM:372-387）。

## symbol-table.txt

- 每行 `分類名稱<Tab>符號 符號 ...`（LMM:578-613）。
- 以 `#` 開頭（或 BOM 後接 `#`）的行是註解；沒有 tab 的行略過。
- 分類名稱去除前後的 BOM、`*`、空白。符號以 ASCII 空格切開（全形空白本身是符號），沒有符號的分類略過。
- 檔案不存在或為空時使用內建預設表（LMM:57-87）。
- 由使用者手動編輯，程式不寫入。

## learning/<裝置ID>-<名稱>.txt（選字學習）

### 檔案內容

每行五欄，以 tab 分隔（`UserOverrideModel::serialize`，UOM:256-267）：

```
key<TAB>candidate<TAB>count<TAB>timestamp<TAB>forceHighScoreOverride
```

- `key`：觀察鍵，格式為 `前前節點-前節點-目標節點`，每個節點是 `(讀音,詞)`，句首或標點視為 `()`（UOM:353-407）。範例：`()-(ㄗˋ,自)-(ㄏㄨㄟˋ,會)`。節點讀音以 `_` 開頭者視為標點（UOM:358-362）。
- `candidate`：使用者選的詞。
- `count`：次數（正整數）。
- `timestamp`：Unix 時間（秒，浮點數，以 17 位有效數字輸出，UOM:258）。
- `forceHighScoreOverride`：`1` 或 `0`。
- 順序由舊到新：依 LRU 由最舊的 key 寫到最新（UOM:259 `rbegin`→`rend`）。讀入時每讀一行就把該 key 移到最前，所以檔案順序決定新舊（UOM:307-313）。同一 key 下的多個 candidate 連續寫出。

### 讀入規則（UOM:269-329）

- 欄位數不是 5、`key` 或 `candidate` 為空的行略過。
- `count` 以 `stoul`、`timestamp` 以 `stod` 解析，失敗略過；`count == 0` 略過。
- 同 key 同 candidate 重複出現時：count 累加、timestamp 取較大、`forceHighScoreOverride` 取後讀到的。
- `append = true` 時保留原有資料；`false` 時先清空。
- 讀完後超過容量就從最舊的丟棄。

### 容量與衰減（LMM:34-37、LMM:42-45）

- 本機模型上限 5000 個 key（`kUserOverrideModelCapacity`）。
- 合併模型上限 20000 個 key（`kMergedUserOverrideModelCapacity`）。
- 半衰期 30 天（`kObservedOverrideHalflife = 30 * 86400` 秒）。衰減係數 `exp((now - timestamp) * ln(0.5) / 半衰期)`；低於 `1/1048576`（約 20 個半衰期）視為完全衰減，不再建議（UOM:40-41、UOM:341-351）。
- 建議時，未衰減的 candidate 中時間戳最新者勝出（UOM:203-224）。

### 誰寫、怎麼寫

- 本裝置只寫自己的檔案（本機模型，保存的是這台裝置選的內容）。選字後延遲 2 秒才存，連續選字合併成一次（LMM:36、LMM:970-981）。
- 以 atomic 方式整份覆寫（LMM:1004 `NSDataWritingAtomic`），寫入在序列佇列上執行。
- 本機檔案尚未載入完成時不得覆寫（LMM:985-990）。
- Linux 版的「載入完成」判定：
  - `learning/` 不存在、但用戶資料夾存在時，視為本機檔案是空的、已載入完成；存檔時才建立 `learning/`，建立失敗就不寫入。
  - 用戶資料夾本身不存在（同步資料夾還沒同步或沒掛載）時，本機檔案維持未載入，不寫入，也不會建立任何資料夾。
  - `learning/` 的位置是檔案而不是資料夾、本機檔存在但讀不到、或改過電腦名稱的舊檔改名失敗時，本機檔案也維持未載入，不寫入。
- Linux 版以暫存檔寫入、`fsync` 並確認 `close` 成功後才改名覆蓋；任何一步失敗都刪除暫存檔、保留原檔。
- 讀取：先載入本機檔案到本機模型，再把本機模型複製到合併模型，接著依檔名排序讀入資料夾內其他裝置的檔案，以 append 方式併入合併模型（LMM:788-809）。

### 裝置 ID 與檔名

- 裝置 ID：UUID 字串的前 8 個字元（UUID 為大寫十六進位，如 `1A2B3C4D`），存在偏好設定，產生後不變（LMM:674-683）。
- 檔名：`<裝置ID>-<電腦名稱>.txt`。電腦名稱為空時用 `Mac`；名稱中的 `/`、`:`、`\`、`\n`、`\r`、`\t` 都換成 `_`（LMM:685-699）。
- 改電腦名稱時：載入本機檔案前，在 `learning/` 內找檔名以「裝置ID-」開頭、副檔名為 `txt`、且不等於目前檔名的檔案，改名為目前檔名（只處理第一個）（LMM:759-767）。目的檔已存在時 `moveItem` 失敗，錯誤被忽略。
- 同樣的檔名也用於 `temporary/`、`english/`、`sentences/` 底下（LMM:825-834、CS:115-118）。

### 其他裝置的檔案

- 只要是 `learning/` 內副檔名為 `txt`、且不等於本機檔名的檔案，就視為其他裝置的檔案併入（LMM:802）。子資料夾（`temporary`、`english`、`sentences`）不是 `.txt` 副檔名，不會被誤讀。

## learning/temporary/<裝置ID>-<名稱>.txt（暫時詞）

- 每行 `值 讀音 時間`，以單一空格分三欄；時間為 Unix 秒，整數格式（`%.0f`）（LMM:836-854、LMM:900-914）。
- 讀入時超過 30 天（`kTemporaryPhraseLifetime = 30 * 86400`）的行丟棄；欄數不是 3 的行略過（LMM:95、LMM:844-850）。
- 暫時詞的鍵是「值 讀音」，同鍵取時間較新者（LMM:880-882）。
- 每台裝置只寫自己的檔，整份 atomic 覆寫（LMM:900-914）。讀取時合併資料夾內所有 `.txt`（LMM:870-884）。
- 若 `removed.txt` 中同鍵（kind = `temporary`）的刪除時間 >= 該筆時間，視為已刪除（LMM:863-866、LMM:877）。
- 暫時詞在查詢時排在使用者詞之後、系統詞庫之前，分數同使用者詞（`McBopomofoLM.cpp:221-232`）。

## learning/english/<裝置ID>-<名稱>.txt（英文字詞）

- 每行 `字<Tab>次數`（EW:234-247）。以 tab 切開（略過空欄），恰好兩欄且次數為整數才採用。
- 每台裝置寫自己的檔，內容依行排序後 atomic 覆寫，在背景佇列寫入（EW:273-285）。
- Linux 版：`learning/english/` 不存在（`learning/` 也可以不存在）、但用戶資料夾存在時，視為本機檔案是空的、已載入完成，存檔時才建立缺少的資料夾；建立失敗就不寫入。用戶資料夾本身不存在時維持未載入，不寫入。其餘「維持未載入」的情況同選字學習。
- 讀取時合併 `english/` 內所有 `.txt`（不含本機檔），同字次數相加（EW:253-265）。
- `removed.txt` 中 kind = `english` 且有記錄的字（刪除時間不為 0）會被濾掉（EW:266-269）。
- 每 60 秒重新載入（EW:31、EW:250）。長度至少 3 才會記錄（EW:29、EW:42）。
- 手動加入的字，本機次數至少設為 2（EW:200-207）。

## learning/sentences/<裝置ID>-<名稱>.txt（常用句）

- 每行 `句子<Tab>次數<Tab>最後使用時間<Tab>已釘選`（至少 3 欄，第 4 欄為 `1` 表示使用者手動加入）（CS:261-278、CS:316）。時間為 Unix 秒整數。
- 句子長度 6 到 40（英文字詞或數字串算 1），且不得含標點、符號、換行、tab（CS:100-102、CS:120-149）。累計 3 次（`threshold`）才會被當成常用句建議（CS:100、CS:209）。
- 保存期限：90 天，次數超過 3 之後每多 3 次多 30 天；已釘選者永不過期（CS:154-156、CS:158-163）。寫檔時濾掉過期項目（CS:311-316）。
- 每台裝置寫自己的檔，依行排序後 atomic 覆寫（CS:307-323）。
- 讀取時合併 `sentences/` 內所有 `.txt`（不含本機檔）：次數相加、最後使用時間取較大、釘選取聯集（CS:287-305）。每 60 秒重新載入（CS:104、CS:280-285）。
- `removed.txt` 中 kind = `sentence` 的刪除時間 >= 該句最後使用時間則視為已刪除（CS:158-160）。

## learning/removed.txt（詞彙管理員的刪除記錄）

- 每行 `kind<Tab>key<Tab>時間`，時間為 Unix 秒整數（CS:26-86）。kind 只有三種：`english`、`sentence`、`temporary`（CS:30-32）。`temporary` 的 key 是「值 讀音」（含空格）。
- 單一檔案，所有裝置共用（不分裝置）。讀入時以 `\n` 切行、`\t` 切三欄（保留空欄位），第 3 欄必須能轉成數字，否則略過（CS:67-77）。
- 新增刪除記錄：重新讀檔，設定該 `kind\tkey` 的時間為現在，再整份 atomic 覆寫；輸出前依行排序（CS:48-52、CS:80-85）。
- 移除記錄（使用者重新加入該字詞時）：重新讀檔，刪除該鍵後整份覆寫（CS:54-59）。
- 查詢「刪除時間」：沒有記錄回傳 0；快取最多 60 秒（CS:43-46、CS:61-65）。
- 判斷是否已刪除：各種資料各自比對（見上方各節）——暫時詞、常用句是「刪除時間 >= 該筆時間」，英文字詞是「有記錄即刪除」。

## 同步衝突副本

Mac 程式碼沒有專門辨識同步工具衝突副本的規則。實際行為只由下列條件決定：

- `learning/`、`temporary/`、`english/`、`sentences/` 內只讀取副檔名為 `.txt` 的檔案（LMM:802、LMM:871、EW:261、CS:294）。
- 不讀本機檔名本身（LMM:802、LMM:871、EW:261、CS:294）。
- 以 `.` 開頭、副檔名為 `icloud` 的檔案是 iCloud 的未下載佔位檔，Mac 版會觸發下載而不讀取（LMM:797-801）。
- 因此，其他裝置的檔名只要仍以 `.txt` 結尾（例如同步工具產生的 `<裝置ID>-<名稱> (衝突副本).txt`），Mac 版都會當成另一台裝置的檔案併入，次數會重複計入；副檔名被改掉的副本則不會被讀。
- 改名邏輯（LMM:759-767）會把以「本機裝置ID-」開頭的 `.txt` 檔改名為本機檔名；若本機檔存在則改名失敗，被忽略。

## Linux 版衝突副本規則

以下是 Linux 版新增的規則，Mac 現有行為不含這些（見上節）：

- 讀取「所有裝置的檔案」時，排除同步工具產生的衝突副本：檔名含 `_Conflict`（Synology Drive 的 `*_Conflict*`）或 `.sync-conflict-`（Syncthing 的 `*.sync-conflict-*`）者。
- 適用於四條讀取路徑：`learning/`（選字學習）、`learning/temporary/`、`learning/english/`、`learning/sentences/`。
- 被排除的檔案只是不讀取：不改名、不刪除、不寫入，留給使用者或同步工具自行處理。
- 尋找「改過電腦名稱的本機舊檔」（以本機裝置ID開頭的檔案改名為目前檔名）時，衝突副本不會被當成本機檔案採用或改名。
