# QNX Sentinel
## Automotive ECU Watchdog & Recovery Framework

QNX Sentinel is an automotive ECU watchdog and recovery framework developed using **QNX Neutrino RTOS**.

The system monitors multiple ECU services, detects software failures such as missed heartbeats, service freezes/deadlocks, and execution overruns, and recovers only the affected ECU service without rebooting the entire system.

The software part of the project has been completed. We are currently working on hardware integration using **Raspberry Pi, UART communication, and GPIO-based status indication**.

---

## Team

**Team Name:** QNX Sentinel

**Institution:** Dhanekula Institute of Engineering and Technology

### Team Members

- **Anumolu Mohini Sri**
- **Talari Krishna Kishore**

---

## Problem Statement

### Automotive ECU Watchdog & Recovery Framework

Modern automotive systems depend on multiple Electronic Control Units (ECUs). A failure in one ECU service should not require restarting the entire system.

The objective of this project is to develop a supervisory framework that:

- Monitors multiple ECU services
- Detects missed heartbeats
- Detects service freezes/deadlocks
- Detects execution overruns
- Identifies the failed ECU service
- Restarts only the affected ECU service
- Verifies successful recovery
- Allows healthy ECU services to continue operating

---

## Project Objective

The main objective of QNX Sentinel is to demonstrate **real-time fault detection and selective recovery using QNX Neutrino RTOS**.

The framework uses:

- QNX Processes
- Threads
- Message-passing IPC
- Timers
- Priority Scheduling
- Process Monitoring
- Watchdog Supervision
- Fault Detection
- Fault Recovery

---

## System Architecture

```text
                    QNX Neutrino RTOS
                           |
        -----------------------------------------
        |                |                      |
   Engine ECU        Brake ECU             Sensor ECU
        |                |                      |
        -----------------------------------------
                           |
                    Heartbeat Messages
                           |
                    Health Monitor
                           |
                       Watchdog
                           |
                    Failure Detected
                           |
                  Recovery Manager
                           |
                  Restart Failed ECU
                           |
                  Recovery Verification
                           |
                    ECU Recovered

## Development Status

The software part of the project has been completed. We are currently working on the hardware integration and testing.

- Software framework — Completed
- ECU processes — Completed
- Heartbeat monitoring — Completed
- Watchdog implementation — Completed
- Fault detection — Completed
- ECU recovery — Completed
- Raspberry Pi integration — In Progress
- UART communication — In Progress
- GPIO integration — In Progress
- Hardware testing — In Progress
