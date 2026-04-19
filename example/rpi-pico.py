# Параметр	SPI(1) (HSPI)	SPI(2) (VSPI)
# ID в коде             SPI(1)      SPI(2)
# SCL (Clock)	        GPIO 14	GPIO 18
# SDA (MOSI)	        GPIO 13	GPIO 23
# MISO (вам не нужен)	GPIO 12	GPIO 19
from machine import Pin, SPI
from spi_displays import ST7735
import framebuf

SCL=10
SDA=11
RST=9
DC=8
CS=5
BLK=13

tft = ST7735(
    spi=SPI(1, baudrate=20_000_000, sck=Pin(SCL), mosi=Pin(SDA), miso=None, phase=0, polarity=0),
    width=160,height=128,
    dc=Pin(DC), cs=Pin(CS), rst=Pin(RST), bl=Pin(BLK),
    rotation=1
)
fb = framebuf.FrameBuffer(tft.buffer, tft.width, tft.height, framebuf.RGB565)

fb.fill(0x0000)
tft.text("17:38:56", 20, 50, 0xFFFF, 16)
tft.show()