# WHY 2025 badge firmware for the ESP32-P4
This repository contains the firmware and launcher that runs on the badge's ESP32-P4 application processor.
Like MCH2022's native apps, you can once again write apps in C that run natively on the badge.
This time, however, these native apps are no longer separate firmwares but are instead loaded by the launcher and re-use common code from it.

Are you interested in helping out?
[Contact us!](https://badge.team/contact/)


## Getting started

1. Clone the repo
2. Run `make prepare`. This is only needed the first time and will install the necessary submodules, such as the ESP-IDF software. This will take a few minutes.
	This command will ask you to run idf.py build. This is not needed.

## Build the firmware

1. Run `make flash` to build *and* flash your version of the software
	Ignore the suggestions to flash


