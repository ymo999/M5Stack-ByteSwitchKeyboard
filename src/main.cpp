#include <Arduino.h>
#include <M5Unified.h>
#include "unit_byte.hpp"

// ByteSwitchユニット関連
UnitByte device;                            // UnitByteクラスのインスタンス生成
constexpr uint8_t I2C_ADDR = 0x47;          // ByteSwitchユニットのI2Cアドレス

// 音階データ
const int noteFrequencies[8] = {262, 294, 330, 349, 392, 440, 494, 523};    // C4～C5（Hz）
const char* noteNames[8] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "Do(High)"};

const float SEMITONE_RATIO = 1.059463f;     // 半音の周波数比（平均律：半音上がるごとに2^(1/12)倍になる）

// 各スイッチのON時LED色（虹色：赤→紫の順に対応）
const uint32_t noteColors[8] = {
    0xFF0000,   // Do        赤
    0xFF7F00,   // Re        橙
    0xFFFF00,   // Mi        黄
    0x00FF00,   // Fa        緑
    0x0000FF,   // Sol       青
    0x4B0082,   // La        藍
    0x9400D3,   // Si        紫
    0xFF00FF    // Do(High)  マゼンタ（虹の最後、区別しやすい色に）
};

// 現在再生中のスイッチ番号（-1は無音）
int playingSwitch = -1;

// 現在再生中の周波数変調率（本体ボタンによる半音/オクターブ変化を反映した値）
float playingModifier = 1.0f;

// 各スイッチの前回状態（LED更新の要否判定用、0=OFF/1=ON）
uint8_t prevSwitchStatus[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// 初期化
void setup() {
    M5.begin();
    Serial.begin(115200);                     // 今後のデバッグ用にシリアルモニターに出力する設定

    // ByteSwitchユニットの初期化（Port A: SDA PinNo=21, SCL PinNo=22, 400kHz）
    bool beginResult = device.begin(&Wire1, I2C_ADDR, 21, 22, 400000);
    Serial.printf("device.begin() result: %d\n", beginResult);   // 1=成功 / 0=失敗

    // LED手動モード：スイッチ状態に応じてloop内で個別に色を設定する
    device.setLEDShowMode(BYTE_LED_USER_DEFINED);
    for (uint8_t i = 0; i < 8; i++) {
        device.setRGB888(i, 0x000000);   // 起動時は全消灯
    }

    // スピーカーの初期設定
    M5.Speaker.setVolume(144);

    // 画面の初期表示
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10,10);
    M5.Display.println("ByteSwitch Keyboard");
}


// メインループ
void loop() {
    M5.update();

    /*
        本体ボタンによる周波数変調率の計算
        BtnA:半音下げる（♭）／BtnB:半音上げる（♯）／BtnC:1オクターブ上げる（組み合わせ可）
    */
    float modifier = 1.0f;
    if (M5.BtnA.isPressed()) {
        modifier /= SEMITONE_RATIO;     // BtnA：半音下げる（フラット）
    }
    if (M5.BtnB.isPressed()) {
        modifier *= SEMITONE_RATIO;     // BtnB：半音上げる（シャープ）
    }
    if (M5.BtnC.isPressed()) {
        modifier *= 2.0f;               // BtnC：オクターブ上げる
    }

    /* 
        8スイッチを順に確認し、状態変化があったものだけLEDを更新
        スイッチを押す→0、スイッチを押さない→1
    */
    int target = -1;

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t status = device.getSwitchStatus(i);
        uint8_t noteIndex = 7 - i;                      // スイッチ番号を音階の並びに変換（ByteSwitchの並び0～7と、鍵盤の並びが逆になるため）
        // Serial.printf("SW%d:%d ", i, status);           // 各スイッチの生の状態を出力

        // LED更新
        if (status != prevSwitchStatus[i]) {
            device.setRGB888(i, status ? 0x000000 : noteColors[noteIndex]);
            prevSwitchStatus[i] = status;
        }
        
        // 再生対象の決定（番号が小さいスイッチを優先）
        if (!status && target == -1) {
            target = noteIndex;
        }
    }

    // 再生対象に変化があった場合のみ、音の開始・停止を行う
    if (target != playingSwitch) {
        if (target == -1) {
            M5.Speaker.stop();
        } else {
            float freq = noteFrequencies[target] * modifier;
            M5.Speaker.tone(freq);       // duration省略=鳴りっぱなし
        }
        
        playingSwitch = target;
        playingModifier = modifier;

        // 画面に現在の音名を表示（いったん黒で塗りつぶして前の表示を消す）
        M5.Display.fillRect(0, 60, 320, 40, BLACK);
        M5.Display.setCursor(10, 60);
        if (target == -1) {
            M5.Display.println("(silence)");
        } else {
            M5.Display.printf("Now: %s", noteNames[target]);
            if (modifier != 1.0f) {
                M5.Display.printf(" (x%.3f)", modifier);    // 変調がかかっている場合は倍率も表示
            }            
        }
    }

    // Serial.println();
    delay(10);              // CPU負荷軽減
}
