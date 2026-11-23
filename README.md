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
- 狀態圖示：提供「中／英」與「全／半」圖示；開啟 Caps Lock 時會有提醒。
- 狀態列選單可快速切換標準鍵盤與許氏鍵盤，切換後立即生效。
- 新增相依套件 `wamerican`（英文字典），安裝時由 apt 自動帶入。

## 安裝（Ubuntu 24.04／Linux Mint 22）

先安裝 fcitx5 與本套件（`.deb` 檔請至專案的 Releases 頁面下載，以下以下載到目前目錄為例）：

```bash
sudo apt update
sudo apt install fcitx5 fcitx5-config-qt ./fcitx5-mcbopomofo-smart-learning_3.1.1-learning.2_amd64.deb
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
