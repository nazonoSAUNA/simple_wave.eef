# 単純音生成(拡張編集フィルタプラグイン)
- [ダウンロードはこちら](../../releases/)
- simple_wave.eefをpluginsフォルダに配置してください

- このプラグインを読み込むためにはpatch.aul r43_ss_58以降が必要です https://scrapbox.io/nazosauna/patch.aul
- 但しpatch.aul r43_ss_68以降でなければ音量を変化させたり音量フェードを付けた時にノイズが目立つと思われます


## パラメータ
- Hz：トラックバーで周波数を指定します
- 波形の種類：三角波 のこぎり波 正弦波 矩形波 パルス波1/4 パルス波1/8 から指定します
- /2：Hzトラックバーの値を半分にします。1オクターブ下がります
- C-B：Hzトラックバーの値を、440Hzをベースとした12音階の最も近い値に変化させます
- *2：Hzトラックバーの値を倍にします。1オクターブ上がります

![image](https://github.com/user-attachments/assets/0322756e-44f5-41ba-94ee-5a1b3acdd3e8)



# 開発者向け
- aviutl_exedit_sdk：ほぼhttps://github.com/nazonoSAUNA/aviutl_exedit_sdk/tree/command を使用しています
