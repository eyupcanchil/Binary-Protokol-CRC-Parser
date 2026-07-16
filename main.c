#include <stdio.h>
#include <windows.h>
#include "protocol.h"

// UART (Sanal Port) üzerinden Python Arayüzüne metin gönderme fonksiyonu
void SendStatusToGUI(HANDLE hComm, const char* msg) {
    DWORD bytesWritten;
    WriteFile(hComm, msg, strlen(msg), &bytesWritten, NULL);
}

int main() {
    printf("=============================================\n");
    printf("   C PARSER DONANIM SIMULATORU (FAZ 3)       \n");
    printf("=============================================\n");
    printf("[*] COM2 dinleniyor... Islemleri artik Python Arayuzunden izleyebilirsiniz.\n\n");

    HANDLE hComm = CreateFile("\\\\.\\COM2", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hComm == INVALID_HANDLE_VALUE) {
        printf("[HATA] COM2 portu acilamadi!\n");
        system("pause");
        return 1;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hComm, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    SetCommState(hComm, &dcbSerialParams);

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

    while (1) {
        if (ReadFile(hComm, rx_buffer, 1, &bytesRead, NULL)) {
            if (bytesRead > 0) {
                uint8_t byte = rx_buffer[0];
                
                // İşlemden ÖNCEKİ durumu kaydediyoruz (Hataları yakalamak için)
                ParserState_t prev_state = parser.state;
                
                // FSM Parser'ı çalıştır
                if (Parser_ProcessByte(&parser, byte, &rx_packet)) {
                    
                    // PAKET BAŞARIYLA ÇÖZÜLDÜYSE GUI'YE HABER VER
                    char buf[256];
                    sprintf(buf, "[OK] PAKET COZULDU! Ver: %d | Uzunluk: %d | CRC: 0x%04X\n", 
                            rx_packet.version, rx_packet.length, rx_packet.crc);
                    SendStatusToGUI(hComm, buf);

                } else {
                    // EĞER PAKET TAMAMLANMADIYSA HATA VAR MI DIYE KONTROL ET
                    if (prev_state == STATE_CRC_LSB && parser.state == STATE_IDLE) {
                        SendStatusToGUI(hComm, "[HATA] Paket Reddedildi: CRC Uyusmazligi!\n");
                    }
                    else if (prev_state == STATE_EOF && parser.state == STATE_IDLE) {
                        SendStatusToGUI(hComm, "[HATA] Paket Reddedildi: EOF (0x55) Eksik!\n");
                    }
                }
            }
        } else {
            Sleep(1);
        }
    }

    CloseHandle(hComm);
    return 0;
}