# Smart Room Occupancy Monitoring & Automation System

An intelligent, microcontroller-based system designed to automatically track room occupancy and control electrical appliances to conserve energy. This project was developed as part of our hardware lab, focusing on system integration, PCB design, and embedded programming.

---

## 🎯 Features

* **Real-time Bidirectional Counting:** Accurately detects and counts people entering and exiting a room.
* **High-Reliability IR Sensing:** Utilizes a dual IR sensor array with **38kHz modulation** to provide robust detection that is immune to ambient light noise.
* **Automatic Appliance Control:** Automatically switches a connected appliance (e.g., light or fan) on when the room is occupied and off when it's empty.
* **Live Visual Feedback:** A 2-digit 7-segment display shows the current number of people in the room.
* **User Alert System:** An audible 10-second buzzer sounds before the system shuts down power, allowing any remaining occupants to override.
* **Manual Override:** A push button allows users to cancel the shutdown sequence and manually set the occupancy count.
* **Custom Hardware:** The entire circuit is designed as a custom Arduino Shield PCB for a clean and reliable final product.

---

## ⚙️ System Architecture & Working Principle

The system is built around an Arduino Uno, which acts as the central processing unit.

![Block Diagram](https://i.imgur.com/gK6r0Qc.png)  ### How It Works

The core of the project is the direction-detection algorithm. Two parallel IR beams are placed at the entrance.
1.  **Modulated IR Signal:** Each IR transmitter LED emits a beam of light that is blinking at 38,000 times per second (38kHz). The corresponding TSOP1738 receiver is tuned to this specific frequency, allowing it to completely ignore interference from sunlight or other light sources.
2.  **State-Machine Logic:** The Arduino runs a state-machine algorithm to determine direction:
    * **Entry:** If the outer beam (A) is broken *before* the inner beam (B), the system registers an entry and increments the count.
    * **Exit:** If the inner beam (B) is broken *before* the outer beam (A), the system registers an exit and decrements the count.
3.  **Appliance Control:** The system checks the `person_count` variable. If the count is greater than zero, it keeps the appliance relay ON. If the count drops to zero, it initiates the 10-second shutdown sequence.

---

## 🔌 Hardware Components

| Component | Specification | Quantity |
| :--- | :--- | :--- |
| **Microcontroller** | Arduino Uno R3 | 1 |
| **Power Supply** | 9V 1A DC Adapter | 1 |
| **IR Receiver** | TSOP1738 (38kHz) | 2 |
| **IR Transmitter** | 5mm IR LED (940nm) | 2 |
| **Display** | 2-Digit 7-Segment (Common Cathode) | 1 |
| **Transistor** | NPN BJT (BC547) | 1 |
| **Alert** | 5V Piezo Buzzer | 1 |
| **Input** | Tactile Push Button | 1 |
| **Indicators** | 5mm Red LEDs | 6 |
| **Resistors** | Assorted (100Ω, 220Ω, 1kΩ) | 1 Kit |
| **PCB** | Custom Designed Arduino Shield | 1 |

---

## 💻 Software & Dependencies

* **IDE:** [Arduino IDE](https://www.arduino.cc/en/software) (version 1.8.x or newer)
* **Language:** C++
* **Libraries:** No external libraries are required. All functions use the standard Arduino core libraries.

---

## 🚀 Installation & Usage

1.  **Clone the Repository:**
    ```bash
    git clone [https://github.com/aditya-8128/Smart_Room_Occupancy.git](https://github.com/aditya-8128/Smart_Room_Occupancy.git)
    ```
2.  **Assemble the Hardware:** Build the circuit on a breadboard or solder the components onto the custom PCB shield as per the KiCad schematic file.
3.  **Upload the Code:** Open the `Smart_Room_Occupancy.ino` file in the Arduino IDE, select your board (Arduino Uno) and COM port, and click "Upload".
4.  **Power On:** Power the device using a 9V DC adapter. The system will initialize and the display will show `0`.
5.  **Operation:** As you pass through the sensors, the count will update. The connected appliance will turn on with the first entry and off 10 seconds after the last exit.

---

## 📈 Future Scope

This project serves as a strong foundation for a more advanced smart building solution.
* **IoT Integration:** Replace the Arduino with an ESP32 to publish occupancy data to a cloud dashboard (like Blynk or Adafruit IO) via Wi-Fi using the MQTT protocol.
* **Smart Dimming:** Integrate an LDR (Light Dependent Resistor) to adjust the light brightness based on ambient light levels, further saving energy.
* **Data Logging:** Add a MicroSD card module to log entry/exit timestamps for long-term occupancy analysis.

---

## 👥 Contributors

* Aditya Upadhyay
* Tushar Joshi
* Nikita Verma

