# 【M5Stack】ATOMS3とDLight Unitで照度ログモニターを作る（Ambient連携・グラフ表示）

ATOMS3と照度センサーユニット（Unit DLight）を使って、照度を測定し、クラウド（Ambient）へ送信しつつ、手元の画面でリアルタイムグラフを確認できるデバイスを作ってみました。

この記事では、その機能と実装したArduinoスケッチのポイントを解説します。

## 作ったもの

ATOMS3にUnit DLightを接続し、以下の機能を持たせました。

1.  **照度測定**: 1秒ごとに照度（Lux）を取得。
2.  **画面表示**:
    *   **上部**: 現在の照度を大きな文字で表示（単位なしのシンプル表示）。
    *   **下部**: 過去90分間の照度推移を折れ線グラフ（ドットプロット）で表示。オートスケール対応。
3.  **クラウド送信**: 1分間の「平均値」を計算し、IoTデータ可視化サービス「Ambient」へ送信。
4.  **堅牢なWiFi接続**: SoftAPなどは使わず、接続が切れたら自動再接続・再起動する仕組みを実装。

## 用意するもの

*   M5Stack ATOMS3
*   M5Stack Unit DLight (デジタル照度センサ)
*   Groveケーブル (Unit DLightに付属)

## ソースコードの解説

作成したスケッチの主要な部分を解説します。

### 1. ライブラリと設定

```cpp
#include <M5Unified.h>
#include <M5_DLight.h>
#include <Ambient.h>
#include <WiFi.h>
#include <Wire.h>

// Ambient設定
#define AMBIENT_CHANNEL_ID 97514          // 自分のチャネルID
#define AMBIENT_WRITE_KEY "3b6e1a975e1cef7f" // 自分のライトキー

// WiFi設定
#define WIFI_SSID "your_ssid"
#define WIFI_PASS "your_password"
```

*   `M5_DLight`: DLightユニットを簡単に扱うためのライブラリです。
*   `Ambient`: Ambientへデータを送信するためのライブラリです。
*   これらは事前にライブラリマネージャからインストールしておく必要があります。

### 2. データの構造とグラフ用バッファ

```cpp
// グラフの設定
#define MAX_HISTORY 90 // 90分（90プロット）分保存

// グラフ用履歴バッファ
float historyLux[MAX_HISTORY];
int historyCount = 0; 
```

グラフ表示用に、過去のデータを配列 `historyLux` に保存しています。1分に1回、平均値をここに追加し、画面下部にプロットします。

### 3. 堅牢なWiFi接続ロジック

IoTデバイスで重要なのが「切れにくい（切れても戻る）WiFi接続」です。今回は `WiFiConnect()` という関数を作り、以下のロジックを実装しました。

*   接続待ち中、画面にインジケータを表示。
*   一定時間（約3秒）繋がらない場合は `WiFi.disconnect()` してリトライ。
*   それでも繋がらない場合（約20-30秒経過）は、**`ESP.restart()` でマイコンごと再起動**させます。これはWiFiスタックの不調などをリセットする最も強力な方法です。

```cpp
void WiFiConnect() {
    // ... (省略)
    while (WiFi.status() != WL_CONNECTED) {
        // ... (省略)
        // 3回リトライしてもダメなら再起動
        if (lpcnt2 > 3) {
            M5.Display.drawString("WiFi Fail -> Restart", ...);
            ESP.restart();
        }
    }
}
```

また、 `loop()` 関数の先頭で常に接続状態を監視し、切れていれば即座にこの `WiFiConnect()` を呼び出します。

### 4. グラフの描画ロジック

画面下部のグラフは、オートスケール（自動縮尺）に対応させています。

```cpp
void drawGraphPoints(float* data, uint16_t color) {
    // 最大値をデータから検索
    float plotMax = data[0];
    for (int i = 1; i < historyCount; i++) {
        if (data[i] > plotMax) plotMax = data[i];
    }

    // きりの良い数値にスケールを合わせる (100, 300, 1000...)
    const float steps[] = {100, 300, 1000, 3000, 10000, 30000, 65000};
    // ...
```

測定値に合わせてY軸の最大値（`plotMax`）を動的に変更し、それに合わせてドットの位置を計算しています。
また、Y軸の視認性を高めるために、左側に「0」「最大値の半分（中間）」「最大値」のラベルを表示し、縦線も描画しています。

### 5. メインループの処理

`loop()` 内では2つのタイマー管理を行っています。

1.  **1秒ごと**: 照度を測定し、現在の値を画面に大きく表示。結果を積算用変数に加算。
2.  **1分ごと**: 積算した値を回数で割って「平均値」を算出。これをAmbientに送信し、グラフ履歴に追加してグラフを更新。

```cpp
// 1分ごとの処理
if (millis() - lastSendTime >= SEND_INTERVAL) {
    if (accData.count > 0) {
        float avgLux = accData.luxSum / accData.count; // 平均算出
        
        // Ambient送信
        ambient.set(1, avgLux);
        ambient.send();

        // グラフ更新
        updateGraphHistory(avgLux);
        drawScreen();
        
        // 積算リセット
        accData.luxSum = 0; accData.count = 0;
    }
}
```

平均値を送信・プロットすることで、突発的な光の変化によるグラフの乱れを抑え、全体の傾向をつかみやすくしています。

## まとめ

ATOMS3の小ささを活かしつつ、画面には必要な情報（現在値と履歴）を見やすく配置しました。平均値を送ることでデータ通信量やサーバー負荷も抑えつつ、詳細な動きは手元の1秒更新の表示で確認できる、実用的なロガーになりました。
