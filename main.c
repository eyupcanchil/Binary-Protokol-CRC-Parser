#include <stdio.h>
#include <windows.h>
#include "protocol.h"

// Gelen paketi ekrana yazdırmak için yardımcı fonksiyon
void PrintPacketInfo(Packet_t *packet) {
    printf("\n==========================================\n");
    printf(" [!!!] YENI PAKET BASARIYLA COZULDU [!!!]\n");
    printf("==========================================\n");
    printf("  Versiyon : %d\n", packet->version);
    printf("  Uzunluk  : %d byte\n", packet->length);
    printf("  Payload  : ");
    for (int i = 0; i < packet->length; i++) {
        printf("%02X ", packet->payload[i]);
    }
    printf("\n  CRC      : 0x%04X (Dogrulandi)\n", packet->crc);
    printf("------------------------------------------\n\n");
}

int main() {
    printf("=============================================\n");
    printf("   C PARSER DONANIM SIMULATORU (FAZ 2)       \n");
    printf("=============================================\n");
    printf("[*] COM2 portu dinleniyor... (Cikmak icin Ctrl+C)\n\n");

    // COM2 Portunu Aç (Sanal portumuz)
    HANDLE hComm = CreateFile("\\\\.\\COM2", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hComm == INVALID_HANDLE_VALUE) {
        printf("[HATA] COM2 portu acilamadi! \n");
        printf("Lutfen com0com veya HHD Virtual Serial Port kurarak COM1 <-> COM2 baglantisi olusturun.\n");
        system("pause");
        return 1;
    }

    // Baudrate ayarları (115200)
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hComm, &dcbSerialParams)) {
        printf("[HATA] Port ayarlari alinamadi.\n");
        CloseHandle(hComm);
        return 1;
    }
    
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    
    if (!SetCommState(hComm, &dcbSerialParams)) {
        printf("[HATA] Port ayarlari ayarlanamadi.\n");
        CloseHandle(hComm);
        return 1;
    }

    // Okuma zaman aşımı (Timeout) ayarları
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(hComm, &timeouts);

    ParserContext_t parser;
    Parser_Init(&parser);
    Packet_t rx_packet;

    uint8_t rx_buffer[1];
    DWORD bytesRead;

    // Sonsuz UART Dinleme Döngüsü
    while (1) {
        // Porttan tek 1 byte oku (Non-blocking parser'ımızı test etmek için)
        if (ReadFile(hComm, rx_buffer, 1, &bytesRead, NULL)) {
            if (bytesRead > 0) {
                uint8_t byte = rx_buffer[0];
                
                // Durum geçişlerini konsola basarak görelim
                printf("RX: 0x%02X | State: %d\n", byte, parser.state);
                
                // Gelen byte'ı FSM parser'ımıza besliyoruz
                if (Parser_ProcessByte(&parser, byte, &rx_packet)) {
                    // Fonksiyon true dönerse, EOF (0x55) gelmiş, CRC tutmuş ve paket hatasız çözülmüş demektir!
                    PrintPacketInfo(&rx_packet);
                }
            }
        } else {
            Sleep(1); // CPU'yu %100 kullanmasını engellemek için küçük bekleme
        }
    }

    CloseHandle(hComm);
    return 0;
}