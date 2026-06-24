# DJ Proto — Aksak Ölçü Destekli İki Deck DJ Yazılımı

İki deck'li, aksak ölçü (7/8, 9/8) destekli DJ yazılımı prototipi.
Gerçek PCM waveform, otomatik BPM analizi, özel beatgrid, loop ve sync sistemleri içerir.

---

## Proje Yapısı

```
dj-proto/
├── core/                    # Platform-bağımsız C++ kütüphanesi
│   ├── include/djcore/      # Public header dosyaları
│   │   ├── TimeSignature.h  # Ölçü veri modeli + grouping
│   │   ├── WaveformAnalyzer.h  # PCM → peak hesaplama
│   │   ├── BPMAnalyzer.h    # Otomatik BPM analizi
│   │   ├── BeatGrid.h       # Beatgrid hesaplama
│   │   ├── LoopEngine.h     # Loop ve quantize
│   │   └── TrackData.h      # JSON tabanlı veri kaydı
│   ├── src/                 # Implementasyonlar
│   └── tests/               # Catch2 unit testleri (Linux'ta çalışır)
├── app/                     # JUCE masaüstü uygulaması (Windows)
│   └── Source/
│       ├── Main.cpp
│       ├── MainComponent    # İki deck + sync kontrolleri
│       ├── DeckComponent    # Tek deck UI
│       ├── WaveformDisplay  # Gerçek waveform + beatgrid görüntüleme
│       └── AudioEngine      # JUCE ses motoru + deck yönetimi
├── scripts/
│   └── generate_test_clicks.py  # Click-track test WAV dosyaları üretir
└── .github/workflows/
    ├── linux-tests.yml      # Linux: Core testleri çalıştır
    └── windows-build.yml    # Windows: JUCE app derle + .exe artifact
```

---

## Hızlı Başlangıç

### 1. Test WAV Dosyaları Üretme

```bash
pip install numpy
python scripts/generate_test_clicks.py --out tests/audio --bpm 120
```

Oluşan dosyalar: `7_8_2+2+3_120bpm.wav`, `9_8_2+2+2+3_120bpm.wav`, vb.

---

### 2. Core Kütüphanesi Testleri (Linux / macOS)

**Gereksinimler:** CMake 3.21+, GCC 12+ veya Clang 14+, internet bağlantısı (Catch2 için)

```bash
cd dj-proto
cmake -S . -B build \
  -DDJCORE_BUILD_TESTS=ON \
  -DDJAPP_BUILD=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build && ctest --output-on-failure
```

**Çalışan testler:**
- ✅ Sessiz WAV → düz waveform
- ✅ Tek click → doğru konumda waveform tepesi
- ✅ 7/8 – 2+2+3 grid hesaplama
- ✅ 7/8 – 2+3+2 ve 3+2+2 grup başlangıçları
- ✅ 9/8 – 2+2+2+3 grid hesaplama
- ✅ 9/8 – 3+2+2+2 grup başlangıçları
- ✅ 7/8 bir ölçü loop = tam 7 sekizlik
- ✅ 9/8 bir ölçü loop = tam 9 sekizlik
- ✅ 10.000 döngüde <1 sample kayma
- ✅ Quantize: kapalı / alt vuruş / grup / ölçü
- ✅ TrackAnalysis kaydetme ve yükleme (JSON)
- ✅ Dosya değişince yeniden analiz algılama

---

### 3. Windows .exe Derleme (Visual Studio 2022)

**Gereksinimler:**
- Visual Studio 2022 (C++ masaüstü iş yükü seçili olmalı)
- CMake 3.21+ (VS ile gelen veya ayrı)
- İnternet bağlantısı (JUCE indirilecek)

```powershell
cd dj-proto
cmake -S . -B build `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DDJCORE_BUILD_TESTS=ON `
  -DDJAPP_BUILD=ON

cmake --build build --config Release --parallel

# EXE: build\app\Release\DJProtoApp.exe
```

---

### 4. GitHub Actions ile Otomatik Derleme

Reponuza `push` yaptığınızda:

| Workflow | Runner | Ne yapar |
|---|---|---|
| `linux-tests.yml` | ubuntu-22.04 | Core testlerini derler ve çalıştırır |
| `windows-build.yml` | windows-latest (VS 2022) | Tam uygulamayı derler, `.exe` artifact olarak kaydeder |

**Artifact'ı indirme:** GitHub Actions → ilgili run → "Artifacts" → `DJProto-Windows-x64`

---

## Teknik Notlar

### BPM Analizi

- Yöntem: Spektral flux onset tespiti + otokorelasyon
- BPM aralığı: 60–220 (otomatik /2 veya x2 ile normalize edilir)
- Güven: Yüksek (≥0.45), Orta (≥0.25), Düşük (<0.25)
- İlk güçlü vuruş: en güçlü onset'ten geriye doğru ölçülerek bulunur

### Waveform

- PCM verisi `djcore::WaveformAnalyzer::computePeaks()` ile işlenir
- Her blok (varsayılan: 256 sample) için max mutlak genlik kaydedilir
- Zoom seviyesine göre `getPeaksForRange()` ile piksel başına peak hesaplanır
- **BPM veya ölçü değiştirildiğinde waveform verisi ASLA değişmez** — yalnızca beatgrid katmanı yeniden hesaplanır

### Beatgrid (Aksak Ölçüler)

- BPM, payda notasını sayar (7/8'de BPM = sekizlik nota/dakika)
- Sub-beat süresi = `60.0 / BPM` saniye
- Ölçü süresi = `numerator × sub-beat süresi`
- 7/8 2+2+3 örneği: 7 sub-beat, 3 grup, grup başlangıçları sub-beat 0, 2, 4'te
- **Program 7/8 veya 9/8 ölçüyü 4/4 gibi değerlendirmez**

### Loop

- 1 ölçü loop = `numerator × sub-beat süresi` sample uzunluğunda
- 7/8 @ 120 BPM: 7 × (60/120 × 44100) = 7 × 22050 = **154350 sample**
- 9/8 @ 120 BPM: 9 × 22050 = **198450 sample**
- Drift testi: 10.000 döngü sonunda < 1 sample hata

### Sync

| Mod | Ne yapar | Kısıtlama |
|---|---|---|
| Tempo Sync | Yalnızca çalma hızını ayarlar | Konum değişmez |
| Beat Sync | Tempo + alt vuruş fazı hizalama | — |
| Bar Sync | Tempo + ölçü başlangıcı hizalama | **Farklı ölçülerde devre dışı** |

7/8 + 9/8 karışımında Bar Sync otomatik olarak uygulanmaz; kullanıcıya uyarı gösterilir.

### Veri Kaydı

- Biçim: JSON (`<dosya_adı>.djdata.json`)
- Hash: dosyanın ilk 256 kB'ının djb2 hash'i
- Dosya değişirse: hash eşleşmez → yeniden analiz
- Kaydedilen veriler: BPM, güven, firstBeat, ölçü, gruplama, phrase başlangıçları, cue noktaları

---

## Windows'ta Test Edilmesi Gereken Özellikler

Bu özellikler JUCE donanım erişimi gerektirdiğinden Linux'ta doğrulanamaz:

- [ ] WAV / MP3 / FLAC dosya yükleme (JUCE AudioFormatManager)
- [ ] Gerçek ses çıkışı (WASAPI / DirectSound)
- [ ] ASIO sürücü desteği (JUCE AudioDeviceManager)
- [ ] İki deck aynı anda çalma
- [ ] Tempo değiştirme sırasında ses sürekliliği
- [ ] Loop geçişlerinde tık/boşluk olmaması
- [ ] Playhead ← JUCE ses tamponu konumu doğruluğu
- [ ] 1 saat çift deck testi: belirgin kayma olmaması

---

## Bilinen Eksikler (Prototip Aşaması)

- [ ] Pitch-preserving tempo değişikliği (şu an basit yeniden örnekleme — resampling)
- [ ] Gerçek FFT tabanlı onset tespiti (şu an RMS tabanlı basit yaklaşım)
- [ ] Phrase otomatik analizi (şu an manuel işaretleme)
- [ ] ASIO çıkışı (WASAPI çalışır; ASIO ayarı JUCE AudioDeviceManager'dan yapılır)
- [ ] Waveform cache diskte saklanması (şu an bellek içi)
- [ ] İkinci deck (Aşama 2'de eklenecek)
- [ ] EQ ve efektler (kapsam dışı)
