🚀 Binary Protocol & CRC Parser Simulation

Merhabalar! Bu depo, gömülü sistemlerde donanım ile yazılımın haberleşmeye başladığı en alt katman olan "Veri Bağı (Data Link)" katmanının çalışma mantığını kavramak için sıfırdan geliştirdiğim ikili (binary) haberleşme protokolü projesini içermektedir.

Gelecekte STM32F103 gibi mikrodenetleyiciler üzerinde Modbus veya aviyonik standartlar gibi daha karmaşık endüstriyel protokollerle çalışmadan önce, işin temelinde yatan o ham verinin (stream) nasıl güvenilir "paketlere" (frame) dönüştürüldüğünü uygulamalı olarak görmek istedim.

Bu proje, C dilinde yazılmış donanımdan bağımsız bir protokol ayrıştırıcı (parser) ve bunu bilgisayar ortamında sanal seri portlar üzerinden test etmemi sağlayan bir Python arayüzünden oluşuyor.

✨ Temel Özellikler
,
Non-Blocking FSM (Sonlu Durum Makinesi): Gelen veriler while döngüleriyle sistemi kilitlemeden, durum makinesi (IDLE, VERSION, LENGTH, PAYLOAD, CRC, EOF) mantığıyla anlık olarak işlenir.

CRC-16-CCITT Doğrulaması: Hat üzerindeki gürültüleri simüle etmek ve veri bütünlüğünü sağlamak için Lookup Table tabanlı hızlı bir CRC algoritması kullanıldı.

Byte Stuffing (Veri Maskeleme): İletilecek verinin (payload) içinde tesadüfen kontrol karakterleri (SOF, EOF) geçerse sistemin çökmemesi için veriler otomatik olarak maskelenir ve alıcıda çözülür.

Çift Yönlü Masaüstü Simülasyonu: C dilindeki donanım kodunu karta gömmeden önce test edebilmek için, Python (CustomTkinter) ile sanal COM portları üzerinden haberleşen interaktif bir test arayüzü (Dashboard) geliştirildi.

📦 Paket Yapısı (Frame Format)

İletişim sırasında veriler hatta şu güvenli formatta basılır:

[SOF (0xAA)] | [VERSION] | [LENGTH] | [PAYLOAD...] | [CRC-16] | [EOF (0x55)]

🛠️ Nasıl Çalışır?

Proje iki ayrı bacaktan oluşur ve Virtual COM Port (örn. com0com) aracılığıyla birbiriyle konuşur:

Python GUI (TX): Test verilerini (Hex) alır, içine bilerek tehlikeli veriler veya CRC hataları enjekte edebilmenizi sağlar, paketleyip sanal seri porta yazar.

C Donanım Simülatörü (RX): Tıpkı bir mikrodenetleyicinin UART kesmesi (interrupt) gibi çalışır. Sanal porttan gelen byte'ları tek tek yakalar, FSM parser'ından geçirir ve paketin durumunu (Çözüldü / CRC Hatası) anında GUI'ye geri bildirir.

🚀 Çalıştırma Adımları

Sistemi kendi bilgisayarınızda denemek için:

Bilgisayarınızda (örneğin com0com ile) birbirine bağlı COM1 ve COM2 sanal portlarını oluşturun.

C simülatörünü GCC ile derleyip çalıştırın:

Bash
gcc main.c protocol.c -o HardwareSim
./HardwareSim
Gerekli Python kütüphanelerini kurup arayüzü başlatın:

Bash
pip install customtkinter pyserial
python gui.py
Arayüz üzerinden COM1 portuna bağlanıp test senaryolarını çalıştırabilirsiniz!
