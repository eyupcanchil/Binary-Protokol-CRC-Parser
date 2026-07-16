import customtkinter as ctk
import serial
import serial.tools.list_ports
import threading
import time

class ProtocolDashboard(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Binary Protocol - UART Dashboard (Faz 2)")
        self.geometry("750x550")
        
        self.serial_port = None
        self.is_connected = False

        # --- SOL PANEL (Bağlantı ve Gönderim) ---
        self.left_frame = ctk.CTkFrame(self, width=250)
        self.left_frame.pack(side="left", fill="y", padx=10, pady=10)

        ctk.CTkLabel(self.left_frame, text="Bağlantı Ayarları", font=("Arial", 16, "bold")).pack(pady=10)
        
        self.port_combobox = ctk.CTkComboBox(self.left_frame, values=self.get_ports())
        self.port_combobox.pack(pady=5, padx=10)
        
        # Eğer COM1 yoksa ilk bulduğunu seçsin
        ports = self.get_ports()
        if "COM1" in ports:
            self.port_combobox.set("COM1")
        elif ports:
            self.port_combobox.set(ports[0])
        else:
            self.port_combobox.set("COM1 (Sanal)")
        
        self.connect_btn = ctk.CTkButton(self.left_frame, text="Bağlan", command=self.toggle_connection)
        self.connect_btn.pack(pady=10, padx=10)

        ctk.CTkLabel(self.left_frame, text="Test Senaryoları", font=("Arial", 14, "bold")).pack(pady=(20, 5))
        
        self.payload_entry = ctk.CTkEntry(self.left_frame, placeholder_text="Hex Payload (örn: 10 20 30)")
        self.payload_entry.pack(pady=5, padx=10)
        
        self.send_btn = ctk.CTkButton(self.left_frame, text="Normal Paket Gönder", fg_color="#28a745", hover_color="#218838", command=lambda: self.send_packet(corrupt=False))
        self.send_btn.pack(pady=5, padx=10)

        self.corrupt_btn = ctk.CTkButton(self.left_frame, text="Bozuk Paket Gönder", fg_color="#dc3545", hover_color="#c82333", command=lambda: self.send_packet(corrupt=True))
        self.corrupt_btn.pack(pady=5, padx=10)
        
        self.stuffed_btn = ctk.CTkButton(self.left_frame, text="Tehlikeli Veri (Stuffing)", fg_color="#ffc107", text_color="black", hover_color="#e0a800", command=self.send_stuffed_packet)
        self.stuffed_btn.pack(pady=5, padx=10)

        # --- SAĞ PANEL (Log Ekranı) ---
        self.right_frame = ctk.CTkFrame(self)
        self.right_frame.pack(side="right", fill="both", expand=True, padx=10, pady=10)
        
        ctk.CTkLabel(self.right_frame, text="Sistem Logları (TX - Gönderilenler)", font=("Arial", 16, "bold")).pack(pady=10)
        
        self.log_textbox = ctk.CTkTextbox(self.right_frame, state="disabled", font=("Courier", 12))
        self.log_textbox.pack(fill="both", expand=True, padx=10, pady=10)

    def get_ports(self):
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports] if ports else ["COM1", "COM2"]

    def log(self, message):
        self.log_textbox.configure(state="normal")
        self.log_textbox.insert("end", message + "\n")
        self.log_textbox.see("end")
        self.log_textbox.configure(state="disabled")

    def toggle_connection(self):
        if not self.is_connected:
            try:
                self.serial_port = serial.Serial(self.port_combobox.get(), 115200, timeout=1)
                self.is_connected = True
                self.connect_btn.configure(text="Bağlantıyı Kes", fg_color="#dc3545", hover_color="#c82333")
                self.log(f"[*] {self.port_combobox.get()} portuna 115200 baud ile bağlanıldı.")
            except Exception as e:
                self.log(f"[!] Bağlantı hatası (Sanal portlar açık mı?): {e}")
        else:
            self.serial_port.close()
            self.is_connected = False
            self.connect_btn.configure(text="Bağlan", fg_color=["#3B8ED0", "#1F6AA5"], hover_color=["#36719F", "#144870"])
            self.log("[*] Bağlantı kesildi.")

    def calculate_crc(self, data):
        # Python tarafında CCITT-16 hesaplaması (C kodu ile birebir aynı sonuç verir)
        crc = 0xFFFF
        for byte in data:
            crc ^= (byte << 8)
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ 0x1021
                else:
                    crc <<= 1
                crc &= 0xFFFF
        return crc

    def build_raw_packet(self, payload, corrupt=False):
        version = 1
        length = len(payload)
        
        raw_data = [version, length] + payload
        crc = self.calculate_crc(raw_data)
        
        # Basit yapı (Bu simülasyonda Python tarafı pure raw basıyor, C tarafı okuyacak)
        # Not: Gerçek senaryoda verici de stuffing yapmalıdır. 
        # C tarafındaki Parser_ProcessByte fonksiyonumuzu test etmek için, 
        # Python'da stuffing işlemini kasten manuel yapıp yollayacağız.
        
        stuffed_payload = []
        for b in raw_data:
            if b in (0xAA, 0x55, 0x7D):
                stuffed_payload.append(0x7D)
                stuffed_payload.append(b ^ 0x20)
            else:
                stuffed_payload.append(b)
                
        # CRC için de stuffing yapılmalı
        crc_msb = (crc >> 8) & 0xFF
        crc_lsb = crc & 0xFF
        
        stuffed_crc = []
        for b in (crc_msb, crc_lsb):
            if b in (0xAA, 0x55, 0x7D):
                stuffed_crc.append(0x7D)
                stuffed_crc.append(b ^ 0x20)
            else:
                stuffed_crc.append(b)

        packet = [0xAA] + stuffed_payload + stuffed_crc + [0x55]

        if corrupt:
            # Paketin içindeki payload verisini kasıtlı olarak boz
            if len(packet) > 4:
                packet[3] ^= 0xFF

        return bytes(packet)

    def send_packet(self, corrupt=False):
        if not self.is_connected:
            self.log("[!] Lütfen önce porta bağlanın.")
            return

        hex_str = self.payload_entry.get().strip()
        if not hex_str:
            hex_str = "10 20 30"
        
        try:
            payload = [int(x, 16) for x in hex_str.split()]
        except:
            self.log("[!] Hatalı hex formatı. Lütfen boşluk bırakarak yazın (örn: AA 01).")
            return

        packet_bytes = self.build_raw_packet(payload, corrupt)
        self.serial_port.write(packet_bytes)
        
        hex_output = " ".join([f"{b:02X}" for b in packet_bytes])
        status = "BOZUK YAYIN" if corrupt else "NORMAL YAYIN"
        self.log(f"[{status}] -> {hex_output}")

    def send_stuffed_packet(self):
        # İçinde 0xAA ve 0x55 barındıran tehlikeli payload
        self.payload_entry.delete(0, 'end')
        self.payload_entry.insert(0, "01 AA 02 55 7D")
        self.send_packet(corrupt=False)
        self.log("[*] Uyarı: İçerisinde SOF, EOF ve ESC barındıran veri Byte Stuffing ile paketlenip gönderildi.")

if __name__ == "__main__":
    app = ProtocolDashboard()
    app.mainloop()