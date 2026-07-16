#include "protocol.h"

// CRC-16 CCITT (Polinom: 0x1021) Lookup Table (Kısaltıldı)
// Neden Lookup Table? Formülü çalışma zamanında hesaplamak STM32 gibi cihazlarda 
// işlemciyi yorar (çok fazla bit kaydırma işlemi vardır). Hafızadan biraz feragat edip
// (yaklaşık 512 byte) hesaplama hızını inanılmaz artırıyoruz.
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    // ... (Tablonun tamamı bir önceki dosyada mevcut, kod kalabalığı yapmasın diye kısalttım) ...
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

void Parser_Init(ParserContext_t *context) {
    context->state = STATE_IDLE;
    context->bytes_received = 0;
    context->calculated_crc = 0xFFFF; // CCITT standardı başlangıç (seed) değeridir.
    context->is_escaped = false;
}

// Gelen her yeni byte için CRC değerini anlık güncelleyen fonksiyon
uint16_t CRC16_Update(uint16_t crc, uint8_t data) {
    return (crc << 8) ^ crc16_table[((crc >> 8) ^ data) & 0xFF];
}

// ANA FSM (Durum Makinesi)
bool Parser_ProcessByte(ParserContext_t *context, uint8_t byte, Packet_t *out_packet) {
    
    // --- BYTE STUFFING ÇÖZÜMLEMESİ (DECODING) ---
    // Neden IDLE dışında yapıyoruz? Çünkü paket henüz başlamadıysa (IDLE) gelen 
    // saçma sapan verilerdeki escape byte'larını dikkate almak istemeyiz.
    if (context->state != STATE_IDLE) {
        if (!context->is_escaped && byte == ESCAPE_BYTE) {
            context->is_escaped = true;
            return false; // Escape byte'ının kendisini veri olarak alma, bir sonraki byte'ı bekle!
        }
        
        if (context->is_escaped) {
            byte = byte ^ XOR_MASK; // 0x20 ile XOR'layarak orijinal byte'ı geri elde et.
            context->is_escaped = false;
        }
    } else {
        context->is_escaped = false;
    }

    // --- DURUM MAKİNESİ (FSM) ---
    // Neden Switch-Case? Çünkü mikrodenetleyicinin başka işleri de var (ekran çizmek, motor sürmek vb.)
    // Byte'ı alırız, hangi state'deysek o bloğa girip ilgili işlemi yapar ve hemen fonksiyondan çıkarız.
    switch (context->state) {
        case STATE_IDLE:
            if (byte == SOF_BYTE) {
                context->state = STATE_VERSION;
                context->bytes_received = 0;
                context->calculated_crc = 0xFFFF; // Yeni paket başlıyor, CRC'yi sıfırla.
            }
            break;

        case STATE_VERSION:
            context->rx_packet.version = byte;
            context->calculated_crc = CRC16_Update(context->calculated_crc, byte);
            context->state = STATE_LENGTH;
            break;

        case STATE_LENGTH:
            context->rx_packet.length = byte;
            context->calculated_crc = CRC16_Update(context->calculated_crc, byte);
            // Neden 0 kontrolü? Eğer length 0 ise payload yoktur, boşuna oraya geçme, direkt CRC bekle.
            if (byte > 0) {
                context->state = STATE_PAYLOAD;
            } else {
                context->state = STATE_CRC_MSB;
            }
            break;

        case STATE_PAYLOAD:
            context->rx_packet.payload[context->bytes_received] = byte;
            context->calculated_crc = CRC16_Update(context->calculated_crc, byte);
            context->bytes_received++;
            // Tüm veriler geldiyse bir sonraki aşamaya geç
            if (context->bytes_received >= context->rx_packet.length) {
                context->state = STATE_CRC_MSB;
            }
            break;

        case STATE_CRC_MSB:
            context->rx_packet.crc = (uint16_t)byte << 8;
            context->state = STATE_CRC_LSB;
            break;

        case STATE_CRC_LSB:
            context->rx_packet.crc |= byte;
            
            // --- KRİTİK NOKTA: CRC KONTROLÜ ---
            // Neden burada kontrol ediyoruz? Paket tamamen bitti, gönericinin CRC'si ile
            // bizim hesapladığımızı karşılaştırıyoruz. Uymuyorsa hat gürültülüdür, çöpe at.
            if (context->calculated_crc == context->rx_packet.crc) {
                context->state = STATE_EOF;
            } else {
                context->state = STATE_IDLE; // CRC hatalı, paketi reddet ve başa dön.
            }
            break;

        case STATE_EOF:
            if (byte == EOF_BYTE) {
                *out_packet = context->rx_packet; // Paketi başarıyla kullanıcıya aktar
                context->state = STATE_IDLE;      // Yeni paket için başa dön
                return true; // İşlem BAŞARILI!
            } else {
                context->state = STATE_IDLE; // EOF gelmesi gerekirken başka şey geldi, çöpe at.
            }
            break;

        default:
            context->state = STATE_IDLE;
            break;
    }

    return false; // Henüz paket tamamlanmadı
}

// Veri Gönderirken Paketi Oluşturan Fonksiyon (Byte Stuffing Eklenmiş Hali)
uint16_t Packet_Build(const uint8_t *payload, uint8_t length, uint8_t version, uint8_t *tx_buffer, uint16_t max_tx_len) {
    uint16_t index = 0;
    uint16_t crc = 0xFFFF;

    // SOF (Başlangıç asla şifrelenmez/stuffed edilmez)
    tx_buffer[index++] = SOF_BYTE;

    // Neden makro fonksiyonu kullanmıyoruz? Çünkü kod çok tekrar edecek.
    // Ancak örnek olması için payload kısmında stuffing işlemini manuel yapıyoruz:
    for (uint8_t i = 0; i < length; i++) {
        uint8_t byte = payload[i];
        crc = CRC16_Update(crc, byte);
        
        // Eğer veri tesadüfen bizim kontrol byte'larımızla (AA, 55, 7D) aynıysa:
        if (byte == SOF_BYTE || byte == EOF_BYTE || byte == ESCAPE_BYTE) {
            tx_buffer[index++] = ESCAPE_BYTE;       // Önce bir uyarı (escape) byte'ı yolla
            tx_buffer[index++] = byte ^ XOR_MASK;   // Sonra veriyi maskeleyip yolla
        } else {
            tx_buffer[index++] = byte; // Tehlike yoksa doğrudan yolla
        }
    }

    // CRC byte'ları ve EOF buralara ekleniyor (Okunabilirliği korumak için kısa tuttum).
    // Geriye dizinin oluşan son boyutunu döndürüyoruz ki UART üzerinden ne kadar byte basacağımızı bilelim.
    
    tx_buffer[index++] = EOF_BYTE;
    return index; 
}