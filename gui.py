import customtkinter as ctk
import serial
import serial.tools.list_ports
import threading

class ProtocolDashboard(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Binary Protocol - UART Dashboard (Faz 3)")
        self.geometry("850x600")
        
        self.serial_port = None
        self.is_connected = False

        # --- SOL PANEL (Bağlantı ve Kontroller) ---
        self.left_frame = ctk.CTkFrame(self, width=250)
        self.left_frame.pack(side="left", fill="y", padx=10, pady=10)

        ctk.CTkLabel(self.left_frame, text="Bağlantı Ayarları", font=("Arial", 16, "bold")).pack(pady=10)
        
        self.port_combobox = ctk.CTkComboBox(self.left_frame, values=self.get_ports())
        self.port_combobox.pack(pady=5, padx=10)
        self.port_combobox.set("COM1")
        
        self.connect_btn = ctk.CTkButton(self.left_frame, text="Bağlan", command=self.toggle_connection)
        self.connect_btn.pack(pady=10, padx=10)

        ctk.CTkLabel(self.left_frame, text="Test Senaryoları", font=("Arial", 14, "bold")).pack(pady=(20, 5))
        
        self.payload_entry = ctk.CTkEntry(self.left_frame, placeholder_text="Hex Payload (örn: 10 20 30)")
        self.payload_entry.pack(pady=5, padx=10)
        
        self.send_btn = ctk.CTkButton(self.left_frame, text="Normal Paket Gönder", fg_color="#28a745", hover_color="#218838", command=lambda: self.send_packet(corrupt=False))
        self.send_btn.pack(pady=5, padx=10)

        self.corrupt_btn = ctk.CTkButton(self.left_frame, text="Bozuk Paket Gönder", fg_color="#dc3545", hover_color="#c82333", command=lambda: self.send_packet(corrupt=True))
        self.corrupt_btn.pack(pady=5, padx=10)

        # --- SAĞ PANEL (Log Ekranları) ---
        self.right_frame = ctk.CTkFrame(self)
        self.right_frame.pack(side="right", fill="both", expand=True, padx=10, pady=10)
        
        # Giden Veriler (TX)
        ctk.CTkLabel(self.right_frame, text="TX (Arayüzden Giden Veriler)", font=("Arial", 14, "bold")).pack(pady=(5,0))
        self.tx_log_textbox = ctk.CTkTextbox(self.right_frame, height=150, state="disabled", font=("Courier", 12))
        self.tx_log_textbox.pack(fill="both", expand=True, padx=10, pady=5)

        # Gelen Veriler (RX)
        ctk.CTkLabel(self.right_frame, text="RX (C Donanımından Gelen Yanıtlar)", font=("Arial", 14, "bold")).pack(pady=(15,0))
        self.rx_log_textbox = ctk.CTkTextbox(self.right_frame, height=150, state="disabled", font=("Courier", 13, "bold"), fg_color="#1a1a1a", text_color="#00ff00")
        self.rx_log_textbox.pack(fill="both", expand=True, padx=10, pady=5)

    def get_ports(self):
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports] if ports else ["COM1", "COM2"]

    def tx_log(self, message):
        self.tx_log_textbox.configure(state="normal")
        self.tx_log_textbox.insert("end", message + "\n")
        self.tx_log_textbox.see("end")
        self.tx_log_textbox.configure(state="disabled")

    def rx_log(self, message):
        self.rx_log_textbox.configure(state="normal")
        self.rx_log_textbox.insert("end", message + "\n")
        self.rx_log_textbox.see("end")
        self.rx_log_textbox.configure(state="disabled")

    def toggle_connection(self):
        if not self.is_connected:
            try:
                self.serial_port = serial.Serial(self.port_combobox.get(), 115200, timeout=0.1)
                self.is_connected = True
                self.connect_btn.configure(text="Bağlantıyı Kes", fg_color="#dc3545")
                self.tx_log(f"[*] {self.port_combobox.get()} portuna bağlanıldı.")
                
                # Arka planda C programından gelenleri okuyacak Thread'i başlat
                self.read_thread = threading.Thread(target=self.read_from_port, daemon=True)
                self.read_thread.start()
            except Exception as e:
                self.tx_log(f"[!] Bağlantı hatası: {e}")
        else:
            self.is_connected = False
            self.serial_port.close()
            self.connect_btn.configure(text="Bağlan", fg_color=["#3B8ED0", "#1F6AA5"])
            self.tx_log("[*] Bağlantı kesildi.")

    def read_from_port(self):
        # Bu fonksiyon sürekli olarak C programının gönderdiği metinleri okur
        while self.is_connected and self.serial_port and self.serial_port.is_open:
            try:
                if self.serial_port.in_waiting > 0:
                    data = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                    if data:
                        # Gelen veriyi GUI'de göstermek için ana thread'e pasla
                        self.after(0, self.rx_log, f">> {data}")
            except:
                break

    def calculate_crc(self, data):
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

    def send_packet(self, corrupt=False):
        if not self.is_connected:
            self.tx_log("[!] Lütfen önce porta bağlanın.")
            return

        hex_str = self.payload_entry.get().strip()
        if not hex_str:
            hex_str = "10 20 30"
        
        try:
            payload = [int(x, 16) for x in hex_str.split()]
        except:
            self.tx_log("[!] Hatalı hex formatı.")
            return

        # Raw data: Version (1) + Length + Payload
        raw_data = [1, len(payload)] + payload
        crc = self.calculate_crc(raw_data)
        
        packet = [0xAA] + raw_data + [(crc >> 8) & 0xFF, crc & 0xFF] + [0x55]

        if corrupt:
            packet[2] = 0x99 # Paketi bilerek bozuyoruz
            status = "BOZUK"
        else:
            status = "NORMAL"

        packet_bytes = bytes(packet)
        self.serial_port.write(packet_bytes)
        
        hex_output = " ".join([f"{b:02X}" for b in packet_bytes])
        self.tx_log(f"[{status}] -> {hex_output}")

if __name__ == "__main__":
    app = ProtocolDashboard()
    app.mainloop()