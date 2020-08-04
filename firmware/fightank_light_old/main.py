import config
import dht
import machine
import time
import socket

def TempHumiditySensor():
	hub_addr = socket.getaddrinfo(config.HOST, config.PORT)[0][-1]

	print(wlan.isconnected())
	print(hub_addr)

	if wlan.isconnected():
		have_measurements = False
		try:
			d.measure()
			temp = d.temperature() + CALIBRATION_DIFF
			humidity = d.humidity()
			have_measurements = True
			print(temp)
		except OSError as e:
			print("Measurement failed: ")
			print(e)

		if have_measurements:
			try:
				s = socket.socket()
				s.connect(hub_addr)

				s.send(bytes('TEMP {} {}\n'.format(int(temp * 100), config.TEMP_HUMIDITY_REGION), 'utf8'))
				s.send(bytes('HUMIDITY {} {}\n'.format(int(humidity * 10), TEMP_HUMIDITY_REGION), 'utf8'))
			except OSError as e:
				print(e)
			finally:
				s.close()

def OfflinePIR():
	# How long to keep lights on after last movement
	TIME_DELAY_SECONDS=10
	LIGHT_PINS = [ 5 ]
	
	light_pwms = [ machine.PWM(machine.Pin(FET_PINS[pin])) for pin in LIGHT_PINS ]

	# Wait for sensor startup
	for i in range(60):
		if i % 2 == 0:
			leds[2].duty(10)
		else:
			leds[2].duty(0)
		time.sleep(1)		

	pir_input = machine.Pin(PIR_PIN, machine.Pin.IN)
	last_movement_time = 0

	while True:
		val = pir_input.value()
		now = time.time()
		if pir_input.value():
			leds[2].duty(10)
			last_movement_time = now
		else:
			leds[2].duty(0)

		if (now - last_movement_time) < TIME_DELAY_SECONDS:
			for light in light_pwms:
				light.duty(30)
		else:
			for light in light_pwms:
				light.duty(0)
			

TempHumiditySensor()
