# 第一階段實機驗收紀錄

套件：`fcitx5-mcbopomofo-smart-learning_3.1.1-learning.1_amd64.deb`
驗收日期：
機器（Ubuntu 24.04／Linux Mint 22）：

## 安裝指令

Linux Mint 22（Cinnamon）：

```bash
sudo apt update
sudo apt install fcitx5 fcitx5-config-qt ./fcitx5-mcbopomofo-smart-learning_3.1.1-learning.1_amd64.deb
im-config -n fcitx5
```

Ubuntu 24.04（GNOME）：同上，另外安裝並啟用 GNOME Shell 擴充套件「Input Method Panel」（可用 `gnome-shell-extension-manager` 搜尋安裝）。

安裝後登出再登入。

## 驗收項目

1. 安裝 fcitx5 與 `.deb`，在系統輸入法設定加入小麥（Ubuntu GNOME 要另裝 Input Method Panel 擴充套件）。
   - 結果：

2. 安裝 Synology Drive Client（從 Synology 官網下載），同步 `Drive/McBopomofo`。
   - 結果：

3. 在 fcitx5 設定把「User data folder」指到同步下來的 `McBopomofo` 資料夾。
   - 結果：

4. 打一段日常文字：許氏鍵盤、`asdfghjkl` 選字、空白選字、Esc、數字鍵盤 Enter、Caps Lock 都和 Mac 相同。
   - 結果：

5. Mac 上加的 `data.txt` 詞在 Linux 打得出來；Linux 加的詞在 Mac 打得出來。
   - 結果：

6. 在 Mac 對某個讀音選次選字幾次後，Linux 切一次輸入法再打同一讀音，會優先出現該次選字；反過來也一樣。
   - 結果：

7. `learning/` 出現 `<裝置ID>-<主機名>.txt`，而 Mac 的兩個檔案內容不變。
   - 結果：

## 備註
