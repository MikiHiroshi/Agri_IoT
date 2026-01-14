# ATOMS3とUnit EnvIIIで作る！グラフ付き環境モニター（Ambient連携あり）

M5Stack社の超小型マイコン**ATOMS3**と、環境センサーユニット**Unit EnvIII**を使って、温度・湿度・気圧を測定し、画面にグラフ表示しながらクラウドサービスの**Ambient**にデータを記録する「環境モニター」です。

## ✨ 機能

*   **リアルタイム計測**: 温度、湿度、気圧を1秒ごとに更新して表示します。
*   **グラフ表示**: ATOMS3の小さな画面に、過去90分間のデータをグラフで表示します。
*   **クラウド記録**: IoTデータ可視化サービス「Ambient」に1分ごとにデータを送信し、スマホやPCから長期的なグラフを確認できます。
*   **安定動作**: WiFiが切れても自動で再接続する機能付きです。

## 🛠 材料

| アイテム | 説明 |
| :--- | :--- |
| **M5Stack ATOMS3** | 液晶画面付きのとても小さなマイコンです。 |
| **M5Stack Unit EnvIII** | 温度・湿度・気圧が測れるセンサーユニットです。 |
| **GROVEケーブル** | ATOMS3とユニットをつなぐケーブル（10cmはUnit EnvIIIに付属していますが長いケーブルは別途購入）。 |
| PC | Windows または Mac (Arduino IDEをインストール用) |

## 💻 ソフトウェアの準備

開発には**Arduino IDE**を使用します。

### 1. Arduino IDEのインストール
まだの方は公式サイトからインストールしてください。

### 2. ボードマネージャの設定
Arduino IDEでM5Stack製品を使えるようにします。
*   `ファイル` > `基本設定` > `追加のボードマネージャのURL` に以下を追加:
    `https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json`
*   `ツール` > `ボード` > `ボードマネージャ` で「M5Stack」を検索してインストールします。
*   インストール後、ボード一覧から **「M5Stack ATOMS3」** を選択してください。

### 3. ライブラリのインストール
このプログラムでは以下のライブラリを使用します。
`ツール` > `ライブラリを管理` から検索してインストールしてください。

*   **M5Unified**: M5Stack製品を簡単に扱うための基本ライブラリ。
*   **M5Unit-ENV**: 環境センサーUnit EnvIII (SHT30, QMP6988) を使うためのライブラリ。
*   **Ambient ESP32 ESP8266 lib**: Ambientへデータを送るためのライブラリ。

## ☁️ Ambientの設定

データをグラフ化するために、無料のIoTデータ可視化サービス **Ambient** を使います。

1.  [Ambient公式サイト](https://ambidata.io/)にアクセスし、ユーザー登録（無料）をします。
2.  ログイン後、「チャネルを作る」をクリックして新しいチャネルを作成します。
3.  作成されたチャネルの **「チャネルID」** と **「ライトキー」** をメモしてください。これらは後でプログラムに書き込みます。

## 📝 プログラムの解説

このプログラムの主な機能を簡単に説明します。

### 1. 必要なライブラリと設定
最初にライブラリを読み込み、WiFi情報やAmbientの鍵を設定します。

```cpp
#include <M5Unified.h>
#include <M5UnitENV.h>
#include <Ambient.h>
// ... (WiFiなど)

// ★ここに自分のAmbient情報を入れます
#define AMBIENT_CHANNEL_ID 00000 
#define AMBIENT_WRITE_KEY "xxxxxxxxxxxxxxxx"
```

### 2. センサーデータの取得
`SHT30`（温度・湿度）と `QMP6988`（気圧）という2つのセンサーからデータを読み取ります。

```cpp
// センサーの準備
SHT3X sht3x;
QMP6988 qmp;

// ...ループ内...
if (sht3x.update()) {
    currentData.temp = sht3x.cTemp; // 温度
}
if (qmp.update()) {
    currentData.press = qmp.pressure / 100.0; // 気圧
}
```

### 3. グラフの描画
ATOMS3の画面は小さいですが、頑張って3種類のデータ（温度・湿度・気圧）を重ねて表示できるように工夫しています。
`drawGraph()` 関数で、過去のデータを配列から取り出して点を打っています。

### 4. Ambientへの送信とWiFi再接続
1分間の平均値を計算してAmbientに送信します。
また、もしWiFiが切れてしまった場合でも、`WiFiConnect()` 関数が自動的に再接続を試みたり、どうしても繋がらない場合はリセットして復旧を試みるように作られています。

## 🚀 インストール手順

1.  Arduino IDEで `ファイル` > `新規ファイル` を作成します。
2.  以下のコードを全てコピーして貼り付けます。
3.  コードの上の方にある設定項目を書き換えます。

    ```cpp
    // --- Configuration ---
    #define AMBIENT_CHANNEL_ID 12345            // ★あなたのチャネルIDに書き換え
    #define AMBIENT_WRITE_KEY "abcdef123456"    // ★あなたのライトキーに書き換え

    // WiFi Credentials
    #define WIFI_SSID "your_wifi_ssid"          // ★家のWiFiの名前に書き換え
    #define WIFI_PASS "your_wifi_password"      // ★家のWiFiのパスワードに書き換え
    ```

4.  PCとATOMS3をUSBケーブルで繋ぎます。
5.  Arduino IDEの「書き込みボタン（右矢印）」を押します。

書き込みが完了すると、自動的にWiFiに接続され、画面に温度などが表示され始めます！

## 🎉 完成
これで、手のひらサイズの高性能な環境モニターの完成です。
Ambientのページを見ると、グラフが少しずつ描かれていくのが分かるはずです。

ATOMS3は非常にコンパクトで画面付き。はんだ付けいらず。ぜひ試してみてください！
