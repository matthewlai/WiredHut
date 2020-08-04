import config

import machine
import network

LED_PINS = [23, 22, 21, 19]
DHT_PIN = 17
PIR_PIN = 36
FET_PINS = {
	1: 25,
	2: 26,
	3: 27,
	4: 14,
	5: 13,
	6: 2,
	7: 4,
	8: 16
}

machine.freq(80000000)

def connect():
	wlan = network.WLAN(network.STA_IF)
	wlan.active(True)

	if not wlan.isconnected():
		wlan.connect(config.SSID, config.PASS)
		while not wlan.isconnected():
			pass
	return wlan

def disconnect(wlan):
	wlan.disconnect()
	wlan.active(False)

leds = [ machine.PWM(machine.Pin(pin)) for pin in LED_PINS ]
for led in leds:
	led.duty(0)

for k, v in FET_PINS.items():
	fet_pin = machine.Pin(v, machine.Pin.OUT)
	fet_pin.value(0)

leds[0].duty(0)
wlan = connect()
leds[0].duty(20)

import webrepl
webrepl.start(password=config.PASS)

