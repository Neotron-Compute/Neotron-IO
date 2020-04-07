#!/bin/sh
arduino-cli \
	compile \
	--fqbn MiniCore:avr:328:bootloader=uart0,variant=modelP,BOD=4v3,LTO=Os_flto,clock=8MHz_internal \
	Neotron-IO.ino
