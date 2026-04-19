from machine import Pin, SPI, UART
from spi_displays import ST7735
import nmea
import framebuf

# # LuatOS ESP32-C3
TFT_SCL = 6   #
TFT_SDA = 7   #
TFT_MISO = 2  #
TFT_RST = 5   #
TFT_DC  = 4   #
TFT_CS  = 8   #
TFT_BLK = 11  #
GPS_RX = 1    # 02
GPS_TX = 0    # 03


spi = SPI(1, baudrate=20_000_000, sck=Pin(TFT_SCL), mosi=Pin(TFT_SDA), miso=Pin(TFT_MISO), phase=0, polarity=0)
tft = ST7735(spi=spi, width=160, height=128, dc=Pin(TFT_DC), cs=Pin(TFT_CS), rst=Pin(TFT_RST), bl=Pin(TFT_BLK), rotation=1)
fb = framebuf.FrameBuffer(tft.buffer, tft.width, tft.height, framebuf.RGB565)
gps = UART(1, baudrate=9600, tx=GPS_TX, rx=GPS_RX)


def read_uart(uart) -> str:
    if not uart.any():
        return ""
    raw_data = uart.readline()
    if not raw_data:
        return ""
    try:
        data = raw_data.decode("utf-8", "ignore")
    except Exception:
        data = "".join([chr(b) for b in raw_data if 32 <= b <= 126 or b in (10, 13)])
        # data = "".join([chr(c) for c in raw_data])
    return data.strip()


def parse(n=10):
    nmea.reset()
    data = nmea.current()
    while n >= 0:
        raw = read_uart(uart=gps)
        print(data)
        if len(raw) > 0:
            if raw[3:6] == "RMC":
                if data['valid'] == True:
                    return data
                n -= 1
            nmea.parse(raw)
            data = nmea.current()

    return data




fb.fill(0x0000)
tft.text("17:38:56", 20, 50, 0xFFFF, 16)
tft.show()