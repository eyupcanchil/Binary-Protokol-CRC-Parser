#include <stdio.h>
#include <string.h>
#include "protocol.h"

int main() {
    ParserContext_t parser;
    Parser_Init(&parser); // Belleği ve state'leri sıfırla

    // --- TEST 1: NORMAL PAKET ---
    // Neden bu test? En temel haberleşme senaryosunu (sorunsuz durumu) test ediyoruz.
    printf("[TEST 1] Standart Paket\n");
    uint8_t test_payload1[] = {0x10, 0x20, 0x30, 0x40};
    uint8_t tx_buffer[64];
    
    // Uygulama veriyi pakete çevirir (SOF, CRC vs. ekler)
    uint16_t tx_len = Packet_Build(test_payload1, sizeof(test_payload1), 1, tx_buffer, sizeof(tx_buffer));
    
    Packet_t rx_packet;
    bool success = false;
    
    // Neden for döngüsü? UART'tan RX kesmesiyle (interrupt) byte'ların TEK TEK 
    // gelişini simüle ediyoruz. 
    for (uint16_t i = 0; i < tx_len; i++) {
        if (Parser_ProcessByte(&parser, tx_buffer[i], &rx_packet)) {
            success = true; // Sadece EOF geldiğinde ve paket sorunsuzsa true döner.
            break;
        }
    }

    if (success) {
        printf("  [OK] Paket Basariyla Ayristirildi!\n\n");
    }


    // --- TEST 2: ZORLU SENARYO (BYTE STUFFING) ---
    // Neden bu test? Payload içine bilerek 0xAA (SOF), 0x7D ve 0x55(EOF) koyduk.
    // Eğer byte stuffing algoritmamız iyi çalışmasaydı sistem bunu yeni bir paket 
    // veya paketin sonu zannedip çökecekti.
    printf("[TEST 2] Icerisinde tehlikeli bytelar olan paket\n");
    uint8_t test_payload2[] = {0x01, 0xAA, 0x02, 0x7D, 0x03, 0x55};
    tx_len = Packet_Build(test_payload2, sizeof(test_payload2), 1, tx_buffer, sizeof(tx_buffer));

    Parser_Init(&parser); // State'i sıfırla
    success = false;
    for (uint16_t i = 0; i < tx_len; i++) {
        if (Parser_ProcessByte(&parser, tx_buffer[i], &rx_packet)) {
            success = true;
        }
    }
    if (success) {
        printf("  [OK] Stuffed Paket Basariyla Cozuldu! Sistemin kafasi karismadi.\n\n");
    }


    // --- TEST 3: GÜRÜLTÜ / BOZUK VERİ ---
    // Neden bu test? Elektromanyetik gürültüden dolayı hat üzerinde bir veri değişirse
    // sistem bunu yutacak mı, yoksa CRC sayesinde fark edip paketi çöpe mi atacak bunu sınıyoruz.
    printf("[TEST 3] CRC Hata Yakalama Simulasyonu\n");
    tx_len = Packet_Build(test_payload1, sizeof(test_payload1), 1, tx_buffer, sizeof(tx_buffer));
    
    // Kasıtlı olarak paketin 4. indisindeki veriyi (payload'un bir parçası) bozuyoruz
    tx_buffer[4] = 0x99; 

    Parser_Init(&parser);
    success = false;
    for (uint16_t i = 0; i < tx_len; i++) {
        if (Parser_ProcessByte(&parser, tx_buffer[i], &rx_packet)) {
            success = true;
        }
    }

    if (!success) {
        printf("  [OK] Basarili! Bozuk paket CRC sayesinda tespit edildi ve reddedildi.\n");
    }

    return 0;
}