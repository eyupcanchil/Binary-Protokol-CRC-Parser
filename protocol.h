#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// --- PROTOKOL SABİTLERİ ---
// Neden bu değerler? HDLC ve PPP gibi endüstri standardı protokollerde 
// escape için 0x7D, bit çevirme (XOR) için 0x20 kullanılır. Standarda sadık kaldık.
#define SOF_BYTE       0xAA
#define EOF_BYTE       0x55
#define ESCAPE_BYTE    0x7D
#define XOR_MASK       0x20

#define MAX_PAYLOAD_SIZE 256

// --- FSM (DURUM MAKİNESİ) DURUMLARI ---
// Neden enum kullandık? Okunabilirliği artırmak ve state geçişlerini takip etmek için.
typedef enum {
    STATE_IDLE,       // Sistem boşta, 0xAA bekliyor
    STATE_VERSION,    // 0xAA geldi, şimdi versiyon bilgisini bekliyor
    STATE_LENGTH,     // Versiyon alındı, veri uzunluğu bekleniyor
    STATE_PAYLOAD,    // Uzunluk alındı, veriler toplanıyor
    STATE_CRC_MSB,    // Veri bitti, CRC'nin ilk byte'ı bekleniyor
    STATE_CRC_LSB,    // CRC'nin ikinci byte'ı bekleniyor
    STATE_EOF         // 0x55 bekleniyor
} ParserState_t;

// --- PAKET YAPISI ---
typedef struct {
    uint8_t version;
    uint8_t length;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint16_t crc;
} Packet_t;

// --- PARSER BAĞLAMI (CONTEXT) ---
// Neden global değişken kullanmadık da struct içine aldık?
// Eğer ileride 2 farklı UART hattından (örneğin biri sensör, biri PC) aynı anda 
// veri okuman gerekirse, iki ayrı ParserContext_t oluşturup kodları kopyalamadan 
// iki hattı da birbirinden bağımsız yönetebilirsin.
typedef struct {
    ParserState_t state;
    Packet_t rx_packet;
    uint16_t bytes_received;
    uint16_t calculated_crc;
    bool is_escaped; // Bir önceki byte 0x7D (Escape) miydi?
} ParserContext_t;

// Fonksiyon Prototipleri
void Parser_Init(ParserContext_t *context);
bool Parser_ProcessByte(ParserContext_t *context, uint8_t byte, Packet_t *out_packet);
uint16_t CRC16_Update(uint16_t crc, uint8_t data);
uint16_t Packet_Build(const uint8_t *payload, uint8_t length, uint8_t version, uint8_t *tx_buffer, uint16_t max_tx_len);

#endif // PROTOCOL_H
