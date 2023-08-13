<h1 align="center"> Ouchat Electronics </h1>
<p align="center">
<img align="center" src="https://ouchat.fr/ouchat.svg" alt="ouchat logo" width="150">
</p>

<p align="center"><img src="https://img.shields.io/badge/version-6.0.0-blue" alt="version">
<img src="https://img.shields.io/badge/framework-esp--idf-lightgrey" alt="platform">
<img src="https://img.shields.io/github/license/RJRP44/Ouchat-Electronics" alt="license">
<img src="https://v2.api.ouchat.fr/api/debug/reliability?cat=1uyq8" alt="reliability">
</p>


A project using [vl53l5cx](https://www.st.com/en/imaging-and-photonics-solutions/vl53l5cx.html) to detect the passage of the cat to the outside.

> **Warning**
> This Project **only** works on ESP32D with at least 8MB of flash size.

## 📌 Contents

* [How does it works ?](#how-does-it-works)
* [Settings](#settings)
* [Libraries](#libraries)
* [Structure](#structure)

---
### <a name="how-does-it-works">💭 How does it works ?</a>

The data processing will be detailed in four layers of data processing. First we need to prepare data, next analyze the data with the necessary calculations and finally determine the result.

#### Data Preparation

The raw data of the sensor as a major issue : the sensor is tilted by 60° then all values are not standardize, so we need to correct that with some math.

---
### <a name="settings"> ⚙ Settings</a>

This project has 2 value that must be assigned to work properly.
In order to configure them, you must run this command in your project folder :
```bash 
> idf.py menuconfig
```
Next go to `Ouchat Configuration` category. 

The values to be assigned are :
- Ouchat Secret Key
- Ouchat Cat ID

---

### <a name="libraries">💾 Libraries </a>

This project is using my [V53L5CX-Library](https://github.com/RJRP44/V53L5CX-Library) for esp-idf framwork (all the source code is available on github).

---
### Structure

```
├── CMakeLists.txt
├── 📁 components /        
│   └── 📁 ouchat /
│       ├── ouchat_api.c
│       ├── ouchat_ble.c
│       ├── ouchat_led.c
│       ├── ouchat_logger.c
│       ├── ouchat_processing.c
│       ├── ouchat_protocomm.c
│       ├── ouchat_sensor.c
│       ├── ouchat_utils.c
│       ├── ouchat_wifi.c
│       ├── ouchat_wifi_prov.c
│       └── 📁 include / 
│           ├── ouchat_api.h
│           ├── ouchat_ble.h
│           ├── ouchat_led.h
│           ├── ouchat_logger.h
│           ├── ouchat_processing.h
│           ├── ouchat_protocomm.h
│           ├── ouchat_sensor.h
│           ├── ouchat_utils.h
│           ├── ouchat_wifi.h
│           └── ouchat_wifi_prov.c
├── 📁 main /
│   ├── CMakeLists.txt
│   ├── main.c
└── README.md  
```
---

## 📝 License

Copyright © 2023 [RJRP44](https://www.github.com/RJRP44).

This project is [GNU GPL](https://www.gnu.org/licenses/gpl-3.0.fr.html)  licensed.

## ✨ Show your support

Give a ⭐️ if this project helped you!

## 👤 Authors

- [@RJRP44](https://www.github.com/RJRP44)

