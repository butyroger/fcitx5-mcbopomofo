# 小麥注音 智慧學習版（Linux／fcitx5）

這是小麥注音 fcitx5 版的「智慧學習版」，選字引擎與 macOS 智慧學習版（learning.23）一致，並針對日常打字調整預設值。

## 這個版本有什麼

- 選字引擎與 macOS 智慧學習版相同。
- 學習到的候選字選擇會依裝置各存一份，放在「用戶資料夾」的 `learning/<裝置ID>-<主機名>.txt`，並自動與其他裝置的檔案合併，因此 Mac 與 Linux 可以互相共用學到的選字。
- 可在 fcitx5 設定中指定「User data folder」（用戶資料夾），指向同步資料夾即可與 Mac 共用詞庫與學習紀錄。使用者自選的資料夾裡不會自動建立範本檔；若資料夾尚未同步完成，會顯示等待同步的提示。
- 預設值與作者的 Mac 設定相同：許氏鍵盤、選字鍵 `asdfghjkl`、直式候選字窗等。

## 中英混打與輸入輔助（learning.2）

- 智慧中英切換：英文字母後面可直接接著打中文，不需先切換輸入法；英文與中文之間會自動補空白（例如「你好 hello」「用 iPhone 拍」），按 Backspace 刪光英文時，自動補的空白會一起消失。
- 英文大小寫自動修正：依內建英文字典與學習到的英文字詞修正，例如 `iphone` 加空白變成 iPhone；按 Enter 直接送出則保留原本輸入的小寫。
- 學習英文字詞：打過兩次以上的英文字會記錄在用戶資料夾的 `learning/english/`，並隨同步資料夾在各裝置間共用。
- 標點：英文或數字之後的標點使用半形（例如「價格是300.」），中文之後維持全形（例如「好，」）。
- Esc 修剪：組字中按一次 Esc 清除最後打錯的亂碼，連按兩次清除全部。
- 狀態圖示：提供「中／英」與「全／半」圖示；開啟 Caps Lock 時會有提醒（此提醒取決於桌面環境是否把 Caps Lock 鍵送給輸入法，目前仍在實機驗證中）。
- 狀態列選單可快速切換標準鍵盤與許氏鍵盤，切換後立即生效。
- 新增相依套件 `wamerican`（英文字典），安裝時由 apt 自動帶入。

## 自動學習與輸入輔助（learning.3）

- 兩段式學習：同一個詞選過一次會先成為「暫存詞」，下次組字時優先出現；同一處再選一次，或之後再打出來送出，就寫入使用者詞庫 `data.txt`。被拆開重選的詞也一樣會學起來。暫存詞存在 `learning/temporary/`，30 天沒用到就過期。
- 候選字窗按 `+` 提高、`-` 排除，單字也能用；符號與英文不能調整。
- 英文聯想：打英文字母的開頭按 Tab，列出學過的英文字與字典裡的字。組字區空白時，Tab 照常交給應用程式。
- 數字快打：按 `` ` `` 後打數字，可選擇日期、時間、金額大寫、千分位等格式，例如 `6548` 可選「陸仟伍佰肆拾捌元整」。
- 直接計算：中文模式打算式再按 `=`，自動接上結果，例如 `（2＋3）＊3＝15`。除以零時不會接上結果。
- 常用句：同一句送出 3 次後，打出開頭會提示「Tab：後半句」，按 Tab 接上。可在設定中關閉（Common sentences）。常用句存在 `learning/sentences/`。
- 文字範本：按 `` ` `` 列出用戶資料夾 `text-templates.txt` 的範本；在範本中再按 `` ` ``，或打標點後按 ↓，開啟分類符號表，先選列、再選格。
- 許氏鍵盤：組字區有字時打數字會進入組字區；沒有對應標點的符號鍵在中文後打全形、在英數後打半形。
- 在其他裝置刪除的暫存詞、常用句與英文字（記在 `learning/removed.txt`），Linux 同步後會跟著不再顯示。從 Linux 刪除這些項目，會在下一版隨詞彙管理員提供。

## 選單整理（learning.5）

- 拿掉選單的「編輯文字範本」，文字範本改在「管理詞彙…」的文字範本分頁編輯。
- 「編輯符號表」在自選的詞庫資料夾（例如 Synology Drive）也會先把內建符號表寫成檔案再開啟，不再提示「找不到檔案」，與 Mac 版相同。

## 管理詞彙、浮動組字窗與面板主題（learning.4）

- 管理詞彙：從 fcitx 選單「管理詞彙…」或應用程式選單的「小麥注音管理詞彙」開啟，分頁管理中文詞彙、排除詞彙、英文詞彙、常用句、暫存詞與文字範本。輸入詞會自動帶出注音；改動立即生效，在 Linux 刪除的英文字、常用句與暫存詞也會同步到其他裝置。詞庫資料夾裡沒有的檔案（可能還在同步）不會被建立。
- 浮動組字窗：在 fcitx 選單勾選「此程式使用浮動組字窗」，該程式的組字改顯示在 fcitx 面板，送出時才交給程式。清單也可在設定「使用浮動組字窗的程式」中修改。
- 選單新增「編輯符號表」「使用說明」「關於小麥注音…」；文字範本在「管理詞彙…」的文字範本分頁編輯（沒有詞彙管理員的版本才有「編輯文字範本」）。使用說明（含「關於」）安裝在 `/usr/share/fcitx5-mcbopomofo/user-guide.html`，設定頁的「打開使用者手冊」也開啟這份說明。
- 候選字面板主題：附「小麥注音淺色」（`mcbopomofo-light`）與「小麥注音深色」（`mcbopomofo-dark`），配色同 Mac 版的候選字面板。主題不會自動套用，請到「Fcitx 5 設定」→「附加元件」→「經典使用者介面」→「設定」：
  - 淺色：「主題」選「小麥注音淺色」。
  - 深色：「主題」選「小麥注音深色」。
  - 跟隨系統：「主題」選淺色、「深色模式」選深色，並勾選「跟隨系統淺色/深色設定」。
  - 字型與字級在同一頁的「字體」設定（主題不指定字型）。主題是全域設定，會套用到所有輸入法；Ubuntu GNOME 使用「Input Method Panel」擴充套件時不適用。

## 安裝（Ubuntu 24.04／Linux Mint 22）

先安裝 fcitx5 與本套件（`.deb` 檔請至專案的 Releases 頁面下載，以下以下載到目前目錄為例）：

```bash
sudo apt update
sudo apt install fcitx5 fcitx5-config-qt ./fcitx5-mcbopomofo-smart-learning_3.1.1-learning.4_amd64.deb
im-config -n fcitx5
```

本套件與官方的 `fcitx5-mcbopomofo` 互斥，安裝時會取代它。

登出再登入後，開啟「Fcitx 5 設定」，在「輸入法」頁籤加入「小麥注音」。

- Linux Mint（Cinnamon）：不需要額外套件。
- Ubuntu（GNOME）：另外需要安裝 GNOME Shell 擴充套件「Input Method Panel」並啟用，候選字窗才會顯示在正確位置。

```bash
sudo apt install gnome-shell-extension-manager
# 開啟 Extension Manager，搜尋並安裝「Input Method Panel」
```

## 與 Mac 共用詞庫（Synology Drive）

1. 在 Linux 安裝 Synology Drive Client for Linux，登入後建立同步工作，把 NAS 上的 `McBopomofo` 資料夾同步到本機，例如 `~/SynologyDrive/McBopomofo`。
2. 等第一次同步完成（資料夾內的 `data.txt` 等檔案都已出現）。
3. 開啟「Fcitx 5 設定」，在小麥注音的設定中，把「User data folder」設為該同步資料夾，例如 `~/SynologyDrive/McBopomofo`。
4. 切換一次輸入法讓設定生效。之後 Mac 與 Linux 加的詞、學到的選字會互相同步。

---

# fcitx5-mcbopomofo: 小麥注音輸入法 fcitx5 模組

![Build](https://github.com/openvanilla/fcitx5-mcbopomofo/actions/workflows/ci.yaml/badge.svg)

本專案是[小麥注音](https://github.com/openvanilla/McBopomofo)的 Linux 版本，功能與小麥注音 macOS 版同步：

- 提供自動選字注音
- 提供傳統注音（打一字選一字「ㄅ半」注音）
- 用戶自訂詞功能：可在組字區加詞，亦可直接修改用戶詞庫檔案及刪詞設定檔
- 記憶使用者最近選字
- 支援標準、倚天、許氏鍵盤、倚天 26 鍵等常見注音鍵盤排列
- 便捷輸入各種中文標點符號
- 支援微軟新注音式「游標後」跟漢音式「游標前」選字

與 macOS 版的主要差別如下：

- 使用 fcitx5 內建選字窗
- 繁簡轉換使用 fcitx5 內建 chttrans 模組

此外，在專案組織上：

- 目前直接使用 McBopomofo macOS 版產生的資料檔
- 語言模型及選字引擎程式碼目前從 macOS 版拷貝過來

## 安裝方式

以下說明如何在 Ubuntu 24.04 LTS（或是 22.04 LTS）上編譯安裝。

請先安裝 fcitx5, CMake, 以及以下開發用模組：

```bash
sudo apt install \
    g++ pkg-config fcitx5 libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev fcitx5-modules-dev \
    cmake extra-cmake-modules gettext libfmt-dev libicu-dev libjson-c-dev
```

然後在本專案的 git 目錄下執行以下指令：

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build

# 初次安裝後，執行以下指令，小麥注音 icon 就會出現在 fcitx5 選單中
sudo update-icon-caches /usr/share/icons/*
```

## 在其他 Linux Distro 或 FreeBSD 上安裝

- 我們利用 [wiki](https://github.com/openvanilla/fcitx5-mcbopomofo/wiki#%E5%AE%89%E8%A3%9D%E8%AA%AA%E6%98%8E) 收集在其他 Linux distro 或 FreeBSD 上安裝的方式，歡迎提供內容與指正。

## C++ 語法風格

### clang-format

本專案的程式碼大致採用 [Google C++ style](https://google.github.io/styleguide/cppguide.html)。我們已經設定好 `.clang-format` 檔案，請在發送 PR 前使用 clang-format 重整風格即可。

```bash
find src/ -name *.cpp -o -name *.h | xargs clang-format -i
```

本專案跟 Google C++ style 不同的地方如下：

- 我們不使用 `snake_case` 變數名稱。變數或參數一律使用 `lowerCamelCase` 風格。
- 成員函數不使用 `Foo::PascalCaseMethod()`。成員函數一律使用 `Foo::lowerCamelCaseMethod()` 風格。

### cpplint

我們也推薦使用 [cpplint](https://github.com/cpplint/cpplint) 檢查 C++ 常見問題。

```bash
cpplint --filter=-build/c++11,-build/include_alpha,-build/include_order,-build/include_subdir,-readability/nolint,-runtime/references,-whitespace --recursive src/
```

### clang-tidy

編譯時開啟 `-DENABLE_CLANG_TIDY=On` cmake flag 以啟用 `clang-tidy` 檢查 (需要 clang 14+)。

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DENABLE_CLANG_TIDY=On
```

## 使用 Ninja 加快建置速度

如果系統已經安裝 [Ninja](https://ninja-build.org/)，可使用以下指令，讓 CMake 將 `make` 換成 `ninja`，加快建置速度。尤其在啟用 clang-tidy 檢查程式時，可[加速不少](https://github.com/openvanilla/fcitx5-mcbopomofo/issues/96)：

```
rm -rf build  # 如果先前已經使用 make，要先移除 build 目錄
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DENABLE_CLANG_TIDY=On
cmake --build build  # 使用 ninja 建置
```

## 使用 SIMD 指令集加快資料解析速度的實驗

[PR #194](https://github.com/openvanilla/fcitx5-mcbopomofo/pull/194) 加入了選項，可使用 SIMD 指令集加速用戶詞庫的解析速度。這個選項屬於實驗性質。要啟用該解析器，可在 CMake 建置時增加以下定義：

```
cmake -B build \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_EXPERIMENTAL_SIMD_SUPPORT_AVX512=1
```

## 社群公約

歡迎小麥注音 Linux 用戶回報問題與指教，也歡迎大家參與小麥注音開發。

我們採用了 GitHub 的[通用社群公約](https://github.com/openvanilla/fcitx5-mcbopomofo/blob/master/CODE_OF_CONDUCT.md)。公約的中文版請參考[這裡的翻譯](https://www.contributor-covenant.org/zh-tw/version/1/4/code-of-conduct/)。我們以上述公約，作為維護小麥注音社群的準則。
