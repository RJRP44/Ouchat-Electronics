<h1 align="center"> Ouchat Electronics </h1>
<p align="center">
<img align="center" src="https://ouchat.fr/ouchat.svg" alt="ouchat logo" width="150">
</p>

<p align="center"><img src="https://img.shields.io/badge/version-6.0.0-blue" alt="version">
<img src="https://img.shields.io/badge/framework-esp--idf-lightgrey" alt="platform">
<img src="https://img.shields.io/github/license/RJRP44/Ouchat-Electronics" alt="license">
<img src="https://img.shields.io/badge/reliability-95.0%25-orange" alt="reliability">
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
│   ├── 📁 ouchat /
│   │   ├── ouchat_api.c
│   │   ├── ouchat_processing.c
│   │   ├── ouchat_utils.c
│   │   └── 📁 include / 
│   │       ├── ouchat_api.h
│   │       ├── ouchat_processing.h
│   │       └── ouchat_utils.h     
│   └── 📁 Vl53l5cx /
│       ├── platform.c
│       ├── vl53v5cx_api.c
│       └── 📁 include /
│           ├── platform.h
│           ├── vl53v5cx_api.h
│           └── vl53v5cx_buffer.h
├── 📁 main /
│   ├── CMakeLists.txt
│   ├── main.c
└── README.md  
```
---

## 📝 License

Copyright © 2022 [RJRP44](https://www.github.com/RJRP44).

This project is [BSD 3-Clause](https://opensource.org/licenses/BSD-3-Clause/)  licensed.

## ✨ Show your support

Give a ⭐️ if this project helped you!

## 👤 Authors

- [@RJRP44](https://www.github.com/RJRP44)

