## 準備  Arduino IDEをPCにインストールして設定を行う

https://docs.m5stack.com/en/arduino/arduino_ide
で以下を進める。
Setting Up the Arduino Development Environment

1. Installing Arduino IDE

Arduino IDEのサイトからdownloadしてインストールする。
![image.png](https://qiita-image-store.s3.ap-northeast-1.amazonaws.com/0/269534/c5e2ec6a-5e14-471b-9da9-a802e8454a54.png)


2. Installing Arduino Board Management
    - この項の中の 2. Selecting the Development Board　では M5Stack -> 自分の使うマイコン（ATOMS3など）を選択する。
    - この項の中の 3. Manual (Offline) Installation of the Development Board は必要なし。（手順としてスキップ）

3. Installing Arduino Libraries

    - この項の中の Installing Arduino Library Management では自分の使うマイコン用のライブラリ（M5AtomS3 by M5Stack、M5StamPLC by M5Stackなど）を選択する。
    - また、使用するセンサーに合わせて、M5-DLight by M5Stack, M5-Unit-ENV by M5Stackをインストールする。
    - Manual Installation with Gitは必要なし。（手順としてスキップ）
    - Examples　も必要なし。（手順としてスキップ）

4. Downloading Programs to Devices
　　（手順としてスキップ）

5. 実際のスケッチをコピー＆ペーストする。

6. PCとM5Stack(ATOMS3など)をUSBケーブルで接続する。



---

## 手順1 ボードを選択する

Arduino IDE 上部メニューから、以下を操作します。

### 方法A

1. **[ツール] → [ボード] → [M5Stack]　→ 使っている機種を選択する **M5ATOMS3**とか **M5StamPLC**とか

 **本体の型番とボード名が一致していることが重要**


---

## 手順2 シリアルポートを選択する

次に **書き込み先のUSBポート** を指定します。

1. **[ツール] → [ポート]**
2. 一覧から **COM○○（Windows）** または **/dev/ttyUSB○ / /dev/tty.usbserial○（Mac）** を選択

### ポートが分からないときの確認方法

* M5Stackを **USBから外す**
* ポート一覧を確認
* **再度USBを接続**
* 新しく出てきたポートが M5Stack

---

## 手順3 書き込み設定（基本はそのままでOK）

以下は **通常は変更不要** です。

* Upload Speed：既定値
* Flash Frequency：既定値
* Partition Scheme：既定値

※ トラブルが出たときだけ変更します（初心者は触らない）

---

## 手順4 スケッチを書き込む

いよいよ書き込みです。

### 方法

* 左上の **「→（右向き矢印）」ボタン** をクリック
  または
* **[スケッチ] → [マイコンボードに書き込む]**

### 書き込み中の表示

* 下部の黒いログ欄に

  ```
  Connecting....
  Writing at 0x00010000...
  ```

  などと表示されます

### 成功すると

```
書き込みが完了しました。
```

と表示されます。

---

## 書き込みに失敗する場合（よくある原因）

### ① ボードが違う

→ 実機と **違うボード名** を選んでいる

### ② ポートが違う

→ 他のUSB機器のポートを選んでいる

### ③ USBケーブルが充電専用

→ **データ通信対応ケーブル**に変更する

### ④ Core2 / ATOM S3 で接続できない

→ 書き込み時に
**本体のリセットボタンを押しながら書き込む** と成功することあり
ATOMS#の場合は横の四角い縦型のボタンを3秒押して緑色のLEDが光るのを確認してから書き込むと書き込める場合あり。通常はこのボタンを押してから書き込まなくても書き込める。書き込みが成功しないときに、横のボタンを3秒押してから書き込みをトライする。

---

## 最小確認用スケッチ（表示テスト）

```cpp
#include <M5Stack.h>

void setup() {
  M5.begin();
  M5.Lcd.println("Hello M5Stack!");
}

void loop() {
}
```

* これを書き込んで
* 画面に **Hello M5Stack!** が出れば成功

---

## ポイント
* **ボード選択が一番重要**
* **ポート選択を間違えない**
* エラーの8割は
  👉「ボード or ポートの選択ミス」

---
